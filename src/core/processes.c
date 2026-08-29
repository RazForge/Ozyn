#include "processes.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

/*
 * Process manager implementation.
 *
 * Controlled boundary to Linux process system.
 * Uses fork/execvp for creation, waitpid(WNOHANG) for non-blocking reap.
 * Static array of 32 process records. No dynamic allocation.
 */

/* ---- State name ---- */

const char *ozayn_process_state_name(ozayn_process_state_t state) {
    switch (state) {
        case OZAYN_PROC_CREATED:    return "CREATED";
        case OZAYN_PROC_STARTING:   return "STARTING";
        case OZAYN_PROC_RUNNING:    return "RUNNING";
        case OZAYN_PROC_EXITED:     return "EXITED";
        case OZAYN_PROC_FAILED:     return "FAILED";
        case OZAYN_PROC_TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

/* ---- Publish event helper ---- */

static void publish_proc_event(ozayn_process_manager_t *mgr,
                               ozayn_event_type_t event_type,
                               ozayn_process_t *proc) {
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             event_type,
                             OZAYN_SRC_CORE,
                             proc);
    }
}

/* ---- Find by PID ---- */

ozayn_process_t *ozayn_process_manager_get_by_pid(
    ozayn_process_manager_t *mgr,
    pid_t pid)
{
    if (!mgr || !mgr->initialized) return NULL;

    for (int i = 0; i < mgr->process_count; i++) {
        if (mgr->processes[i].pid == pid &&
            mgr->processes[i].state != OZAYN_PROC_EXITED &&
            mgr->processes[i].state != OZAYN_PROC_FAILED &&
            mgr->processes[i].state != OZAYN_PROC_TERMINATED) {
            return &mgr->processes[i];
        }
    }
    return NULL;
}

/* ---- Lifecycle ---- */

ozayn_result_t ozayn_process_manager_init(ozayn_process_manager_t *mgr) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_process_manager_t));
    mgr->next_id       = 1;
    mgr->initialized   = 1;

    LOG_INFO("PROCESSES", "Process manager initialized (capacity=32)");

    return OZAYN_OK;
}

void ozayn_process_manager_shutdown(ozayn_process_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Reap any remaining children */
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        /* reaped */
    }

    int active = ozayn_process_manager_active_count(mgr);
    if (active > 0) {
        LOG_WARN("PROCESSES", "Shutting down with %d active process(es)", active);
    }

    mgr->initialized = 0;
    mgr->process_count = 0;

    LOG_INFO("PROCESSES", "Process manager shut down");
}

/* ---- Binding ---- */

void ozayn_process_manager_set_events(ozayn_process_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_process_manager_set_recovery(ozayn_process_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ---- Create and start process ---- */

ozayn_process_t *ozayn_process_manager_create(
    ozayn_process_manager_t *mgr,
    const char *executable,
    char *const argv[])
{
    if (!mgr || !mgr->initialized) {
        LOG_ERROR("PROCESSES", "Process manager not initialized");
        return NULL;
    }
    if (!executable || !argv) {
        LOG_WARN("PROCESSES", "NULL executable or argv");
        return NULL;
    }

    /* Check capacity */
    if (mgr->process_count >= 32) {
        LOG_WARN("PROCESSES", "Process capacity reached (32) — rejecting %s",
                 executable);
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_RESOURCE,
                                 OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION,
                                 "PROCESSES",
                                 "Process capacity reached");
        }
        return NULL;
    }

    /* Allocate record */
    ozayn_process_t *proc = &mgr->processes[mgr->process_count];
    memset(proc, 0, sizeof(ozayn_process_t));
    proc->id         = mgr->next_id++;
    proc->pid        = -1;
    proc->state      = OZAYN_PROC_CREATED;
    proc->exit_code  = 0;
    proc->exit_signal = 0;
    proc->created_at = time(NULL);

    mgr->process_count++;

    LOG_INFO("PROCESSES", "Process #%u created: %s",
             proc->id, executable);

    /* Transition to STARTING */
    proc->state = OZAYN_PROC_STARTING;

    /* Fork */
    pid_t pid = fork();

    if (pid < 0) {
        /* fork failed */
        proc->state = OZAYN_PROC_FAILED;
        proc->finished_at = time(NULL);
        LOG_ERROR("PROCESSES", "Process #%u: fork failed", proc->id);

        publish_proc_event(mgr, OZAYN_EVENT_PROCESS_FAILED, proc);

        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_SYSTEM,
                                 OZAYN_LOG_ERROR,
                                 OZAYN_SCOPE_OPERATION,
                                 "PROCESSES",
                                 "fork() failed");
        }
        return proc;
    }

    if (pid == 0) {
        /* ---- Child process ---- */
        execvp(executable, (char *const *)argv);

        /* If execvp returns, it failed */
        fprintf(stderr, "[OZAYN] execvp failed: %s\n", executable);
        _exit(127);
    }

    /* ---- Parent process ---- */
    proc->pid = pid;
    proc->state = OZAYN_PROC_RUNNING;
    proc->started_at = time(NULL);

    LOG_INFO("PROCESSES", "Process #%u started: PID %d",
             proc->id, (int)pid);

    publish_proc_event(mgr, OZAYN_EVENT_PROCESS_STARTED, proc);

    return proc;
}

/* ---- Terminate process ---- */

ozayn_result_t ozayn_process_manager_terminate(
    ozayn_process_manager_t *mgr,
    uint32_t process_id)
{
    if (!mgr || !mgr->initialized) return OZAYN_ERR_NULL;

    ozayn_process_t *proc = ozayn_process_manager_get(mgr, process_id);
    if (!proc) {
        LOG_WARN("PROCESSES", "Cannot terminate: process #%u not found",
                 process_id);
        return OZAYN_ERR;
    }

    if (proc->state != OZAYN_PROC_RUNNING) {
        LOG_WARN("PROCESSES", "Cannot terminate process #%u: state is %s",
                 process_id, ozayn_process_state_name(proc->state));
        return OZAYN_ERR_STATE;
    }

    LOG_INFO("PROCESSES", "Process #%u (PID %d): sending SIGTERM",
             proc->id, (int)proc->pid);

    /* Try graceful termination first */
    if (kill(proc->pid, SIGTERM) != 0) {
        LOG_WARN("PROCESSES", "Process #%u: SIGTERM failed, trying SIGKILL",
                 proc->id);
        kill(proc->pid, SIGKILL);
    }

    /* Brief wait for process to exit */
    int status;
    int waited = 0;
    for (int i = 0; i < 10; i++) {
        pid_t result = waitpid(proc->pid, &status, WNOHANG);
        if (result > 0) {
            /* Process exited */
            if (WIFSIGNALED(status)) {
                proc->exit_signal = WTERMSIG(status);
            } else if (WIFEXITED(status)) {
                proc->exit_code = WEXITSTATUS(status);
            }
            proc->state = OZAYN_PROC_TERMINATED;
            proc->finished_at = time(NULL);
            waited = 1;
            break;
        }
        /* Not yet — small delay */
        struct timespec ts = { 0, 10000000 }; /* 10ms */
        nanosleep(&ts, NULL);
    }

    if (!waited) {
        /* Still running — force kill */
        LOG_WARN("PROCESSES", "Process #%u: SIGTERM timeout, sending SIGKILL",
                 proc->id);
        kill(proc->pid, SIGKILL);
        waitpid(proc->pid, &status, 0);

        if (WIFSIGNALED(status)) {
            proc->exit_signal = WTERMSIG(status);
        }
        proc->state = OZAYN_PROC_TERMINATED;
        proc->finished_at = time(NULL);
    }

    LOG_INFO("PROCESSES", "Process #%u terminated (signal=%d, exit=%d)",
             proc->id, proc->exit_signal, proc->exit_code);

    publish_proc_event(mgr, OZAYN_EVENT_PROCESS_EXITED, proc);

    return OZAYN_OK;
}

/* ---- Reap exited processes (non-blocking) ---- */

int ozayn_process_manager_reap(ozayn_process_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    int reaped = 0;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ozayn_process_t *proc = ozayn_process_manager_get_by_pid(mgr, pid);
        if (!proc) continue;

        reaped++;

        if (WIFEXITED(status)) {
            proc->exit_code = WEXITSTATUS(status);
            proc->exit_signal = 0;
            proc->state = OZAYN_PROC_EXITED;
            LOG_INFO("PROCESSES", "Process #%u (PID %d) exited: code=%d",
                     proc->id, (int)pid, proc->exit_code);
        } else if (WIFSIGNALED(status)) {
            proc->exit_signal = WTERMSIG(status);
            proc->exit_code = 0;
            proc->state = OZAYN_PROC_TERMINATED;
            LOG_INFO("PROCESSES", "Process #%u (PID %d) terminated: signal=%d",
                     proc->id, (int)pid, proc->exit_signal);
        } else {
            proc->state = OZAYN_PROC_FAILED;
            LOG_WARN("PROCESSES", "Process #%u (PID %d): unexpected wait status",
                     proc->id, (int)pid);
        }

        proc->finished_at = time(NULL);
        publish_proc_event(mgr, OZAYN_EVENT_PROCESS_EXITED, proc);
    }

    return reaped;
}

/* ---- Query ---- */

ozayn_process_t *ozayn_process_manager_get(
    ozayn_process_manager_t *mgr,
    uint32_t process_id)
{
    if (!mgr || !mgr->initialized) return NULL;

    for (int i = 0; i < mgr->process_count; i++) {
        if (mgr->processes[i].id == process_id) {
            return &mgr->processes[i];
        }
    }
    return NULL;
}

int ozayn_process_manager_active_count(const ozayn_process_manager_t *mgr) {
    if (!mgr) return 0;

    int count = 0;
    for (int i = 0; i < mgr->process_count; i++) {
        if (mgr->processes[i].state == OZAYN_PROC_CREATED ||
            mgr->processes[i].state == OZAYN_PROC_STARTING ||
            mgr->processes[i].state == OZAYN_PROC_RUNNING) {
            count++;
        }
    }
    return count;
}
