#ifndef OZAYN_PROCESSES_H
#define OZAYN_PROCESSES_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/*
 * ozayn_processes.h — Process manager.
 *
 * Controlled boundary between OZAYN Core and Linux process system.
 * Wraps fork/exec/waitpid behind OZAYN abstractions.
 *
 * Task: "This work is being managed."
 * Process: "This Linux program is executing."
 */

/* ---- Process states ---- */
typedef enum {
    OZAYN_PROC_CREATED   = 0,
    OZAYN_PROC_STARTING  = 1,
    OZAYN_PROC_RUNNING   = 2,
    OZAYN_PROC_EXITED    = 3,
    OZAYN_PROC_FAILED    = 4,
    OZAYN_PROC_TERMINATED = 5,
} ozayn_process_state_t;

/* ---- Process record ---- */
typedef struct {
    uint32_t                id;          /* internal OZAYN process ID */
    pid_t                   pid;         /* Linux PID */
    ozayn_process_state_t   state;
    int                     exit_code;   /* normal exit code */
    int                     exit_signal; /* signal that terminated, 0 if none */
    time_t                  created_at;
    time_t                  started_at;
    time_t                  finished_at;
} ozayn_process_t;

/* ---- Process manager ---- */
typedef struct {
    ozayn_process_t  processes[32];
    int              process_count;
    uint32_t         next_id;
    void            *events;    /* ozayn_event_engine_t* */
    void            *recovery;  /* ozayn_recovery_t* */
    int              initialized;
} ozayn_process_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_process_manager_init(ozayn_process_manager_t *mgr);
void           ozayn_process_manager_shutdown(ozayn_process_manager_t *mgr);

/* ---- Binding ---- */
void ozayn_process_manager_set_events(ozayn_process_manager_t *mgr, void *events);
void ozayn_process_manager_set_recovery(ozayn_process_manager_t *mgr, void *recovery);

/* ---- Create and start process ---- */
ozayn_process_t *ozayn_process_manager_create(
    ozayn_process_manager_t *mgr,
    const char *executable,
    char *const argv[]);

/* ---- Terminate process ---- */
ozayn_result_t ozayn_process_manager_terminate(
    ozayn_process_manager_t *mgr,
    uint32_t process_id);

/* ---- Reap exited processes (non-blocking, call from Runtime loop) ---- */
int ozayn_process_manager_reap(ozayn_process_manager_t *mgr);

/* ---- Query ---- */
ozayn_process_t *ozayn_process_manager_get(
    ozayn_process_manager_t *mgr,
    uint32_t process_id);
ozayn_process_t *ozayn_process_manager_get_by_pid(
    ozayn_process_manager_t *mgr,
    pid_t pid);
int ozayn_process_manager_active_count(const ozayn_process_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_process_state_name(ozayn_process_state_t state);

#endif
