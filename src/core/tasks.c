#include "tasks.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/*
 * Task manager implementation.
 *
 * Static array of task records. Synchronous execution.
 * Handlers run to completion when submitted. No async scheduling.
 */

/* Forward declarations of demo handlers */
static ozayn_task_result_t handle_demo(ozayn_task_t *task, void *ctx);
static ozayn_task_result_t handle_demo_fail(ozayn_task_t *task, void *ctx);

/* ---- Built-in task registry ---- */

static const ozayn_task_entry_t builtin_registry[] = {
    { OZAYN_TASK_DEMO,      handle_demo,      "DEMO"      },
    { OZAYN_TASK_DEMO_FAIL, handle_demo_fail, "DEMO_FAIL" },
};

static const int builtin_registry_size =
    (int)(sizeof(builtin_registry) / sizeof(builtin_registry[0]));

/* ---- Names ---- */

const char *ozayn_task_type_name(ozayn_task_type_t type) {
    switch (type) {
        case OZAYN_TASK_NONE:      return "NONE";
        case OZAYN_TASK_DEMO:      return "DEMO";
        case OZAYN_TASK_DEMO_FAIL: return "DEMO_FAIL";
    }
    return "UNKNOWN";
}

const char *ozayn_task_source_name(ozayn_task_source_t src) {
    switch (src) {
        case OZAYN_TASK_SRC_CORE:    return "CORE";
        case OZAYN_TASK_SRC_COMMAND: return "COMMAND";
    }
    return "UNKNOWN";
}

const char *ozayn_task_state_name(ozayn_task_state_t state) {
    switch (state) {
        case OZAYN_TASK_CREATED:   return "CREATED";
        case OZAYN_TASK_QUEUED:    return "QUEUED";
        case OZAYN_TASK_RUNNING:   return "RUNNING";
        case OZAYN_TASK_COMPLETED: return "COMPLETED";
        case OZAYN_TASK_FAILED:    return "FAILED";
        case OZAYN_TASK_CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

const char *ozayn_task_result_name(ozayn_task_result_t result) {
    switch (result) {
        case OZAYN_TASK_RESULT_SUCCESS: return "SUCCESS";
        case OZAYN_TASK_RESULT_FAILURE: return "FAILURE";
    }
    return "UNKNOWN";
}

/* ---- State transition validation ---- */

static int is_valid_task_transition(ozayn_task_state_t from, ozayn_task_state_t to) {
    switch (from) {
        case OZAYN_TASK_CREATED:
            return to == OZAYN_TASK_QUEUED;
        case OZAYN_TASK_QUEUED:
            return to == OZAYN_TASK_RUNNING || to == OZAYN_TASK_CANCELLED;
        case OZAYN_TASK_RUNNING:
            return to == OZAYN_TASK_COMPLETED ||
                   to == OZAYN_TASK_FAILED ||
                   to == OZAYN_TASK_CANCELLED;
        case OZAYN_TASK_COMPLETED:
        case OZAYN_TASK_FAILED:
        case OZAYN_TASK_CANCELLED:
            return 0; /* terminal states */
    }
    return 0;
}

/* ---- Transition task state ---- */

static ozayn_result_t task_transition(ozayn_task_t *task, ozayn_task_state_t to) {
    if (!task) return OZAYN_ERR_NULL;
    if (!is_valid_task_transition(task->state, to)) {
        LOG_WARN("TASKS", "Task #%u: invalid transition %s -> %s",
                 task->id,
                 ozayn_task_state_name(task->state),
                 ozayn_task_state_name(to));
        return OZAYN_ERR_STATE;
    }
    task->state = to;
    return OZAYN_OK;
}

/* ---- Publish event helper ---- */

static void publish_task_event(ozayn_task_manager_t *mgr,
                               ozayn_event_type_t event_type,
                               ozayn_task_t *task) {
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             event_type,
                             OZAYN_SRC_CORE,
                             task);
    }
}

/* ---- Lookup handler ---- */

static ozayn_task_handler_t lookup_handler(const ozayn_task_manager_t *mgr,
                                            ozayn_task_type_t type) {
    for (int i = 0; i < mgr->registry_size; i++) {
        if (mgr->registry[i].type == type) {
            return mgr->registry[i].handler;
        }
    }
    return NULL;
}

/* ---- Lifecycle ---- */

ozayn_result_t ozayn_task_manager_init(ozayn_task_manager_t *mgr) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_task_manager_t));

    mgr->registry      = builtin_registry;
    mgr->registry_size = builtin_registry_size;
    mgr->task_count    = 0;
    mgr->next_id       = 1;
    mgr->initialized   = 1;

    LOG_INFO("TASKS", "Task manager initialized (%d task types registered)",
             mgr->registry_size);

    return OZAYN_OK;
}

void ozayn_task_manager_shutdown(ozayn_task_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Warn about unfinished tasks */
    int active = ozayn_task_manager_active_count(mgr);
    if (active > 0) {
        LOG_WARN("TASKS", "Shutting down with %d active task(s)", active);
    }

    mgr->initialized = 0;
    mgr->task_count = 0;
    mgr->registry = NULL;
    mgr->registry_size = 0;

    LOG_INFO("TASKS", "Task manager shut down");
}

/* ---- Binding ---- */

void ozayn_task_manager_set_events(ozayn_task_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_task_manager_set_recovery(ozayn_task_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ---- Submit task ---- */

ozayn_task_t *ozayn_task_manager_submit(ozayn_task_manager_t *mgr,
                                         ozayn_task_type_t type,
                                         ozayn_task_source_t source) {
    if (!mgr || !mgr->initialized) {
        LOG_ERROR("TASKS", "Task manager not initialized");
        return NULL;
    }

    /* Validate type */
    if (type == OZAYN_TASK_NONE) {
        LOG_WARN("TASKS", "Cannot submit task with type NONE");
        return NULL;
    }

    /* Check handler exists */
    ozayn_task_handler_t handler = lookup_handler(mgr, type);
    if (!handler) {
        LOG_WARN("TASKS", "No handler for task type %s",
                 ozayn_task_type_name(type));
        return NULL;
    }

    /* Check capacity */
    if (mgr->task_count >= 64) {
        LOG_WARN("TASKS", "Task capacity reached (64) — rejecting %s",
                 ozayn_task_type_name(type));
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_RESOURCE,
                                 OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION,
                                 "TASKS",
                                 "Task capacity reached");
        }
        return NULL;
    }

    /* Create task */
    ozayn_task_t *task = &mgr->tasks[mgr->task_count];
    memset(task, 0, sizeof(ozayn_task_t));
    task->id         = mgr->next_id++;
    task->type       = type;
    task->source     = source;
    task->state      = OZAYN_TASK_CREATED;
    task->progress   = -1; /* indeterminate */
    task->created_at = time(NULL);
    task->started_at = 0;
    task->finished_at = 0;

    mgr->task_count++;

    LOG_INFO("TASKS", "Task #%u created: %s (source=%s)",
             task->id,
             ozayn_task_type_name(type),
             ozayn_task_source_name(source));

    publish_task_event(mgr, OZAYN_EVENT_TASK_CREATED, task);

    /* Transition: CREATED → QUEUED */
    if (task_transition(task, OZAYN_TASK_QUEUED) != OZAYN_OK) {
        return task;
    }

    /* Transition: QUEUED → RUNNING */
    if (task_transition(task, OZAYN_TASK_RUNNING) != OZAYN_OK) {
        return task;
    }

    task->started_at = time(NULL);
    LOG_INFO("TASKS", "Task #%u started", task->id);

    /* Execute handler synchronously */
    ozayn_task_result_t result = handler(task, mgr);

    task->finished_at = time(NULL);

    if (result == OZAYN_TASK_RESULT_SUCCESS) {
        task_transition(task, OZAYN_TASK_COMPLETED);
        LOG_INFO("TASKS", "Task #%u completed: %s (progress=%d%%)",
                 task->id,
                 ozayn_task_type_name(type),
                 task->progress);
        publish_task_event(mgr, OZAYN_EVENT_TASK_COMPLETED, task);
    } else {
        task_transition(task, OZAYN_TASK_FAILED);
        LOG_ERROR("TASKS", "Task #%u failed: %s",
                  task->id,
                  ozayn_task_type_name(type));
        publish_task_event(mgr, OZAYN_EVENT_TASK_FAILED, task);

        /* Report to error recovery */
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_RUNTIME,
                                 OZAYN_LOG_ERROR,
                                 OZAYN_SCOPE_OPERATION,
                                 "TASKS",
                                 "Task execution failed");
        }
    }

    return task;
}

/* ---- Cancel task ---- */

ozayn_result_t ozayn_task_manager_cancel(ozayn_task_manager_t *mgr,
                                          uint32_t task_id) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_NULL;

    ozayn_task_t *task = ozayn_task_manager_get(mgr, task_id);
    if (!task) {
        LOG_WARN("TASKS", "Cannot cancel: task #%u not found", task_id);
        return OZAYN_ERR;
    }

    if (task->state == OZAYN_TASK_COMPLETED ||
        task->state == OZAYN_TASK_FAILED ||
        task->state == OZAYN_TASK_CANCELLED) {
        LOG_WARN("TASKS", "Cannot cancel task #%u: already %s",
                 task_id, ozayn_task_state_name(task->state));
        return OZAYN_ERR_STATE;
    }

    task_transition(task, OZAYN_TASK_CANCELLED);
    task->finished_at = time(NULL);

    LOG_INFO("TASKS", "Task #%u cancelled", task_id);
    publish_task_event(mgr, OZAYN_EVENT_TASK_CANCELLED, task);

    return OZAYN_OK;
}

/* ---- Query ---- */

ozayn_task_t *ozayn_task_manager_get(ozayn_task_manager_t *mgr, uint32_t task_id) {
    if (!mgr || !mgr->initialized) return NULL;

    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i].id == task_id) {
            return &mgr->tasks[i];
        }
    }
    return NULL;
}

int ozayn_task_manager_active_count(const ozayn_task_manager_t *mgr) {
    if (!mgr) return 0;

    int count = 0;
    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i].state != OZAYN_TASK_COMPLETED &&
            mgr->tasks[i].state != OZAYN_TASK_FAILED &&
            mgr->tasks[i].state != OZAYN_TASK_CANCELLED) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * Demo handlers
 * ================================================================ */

/* ---- DEMO handler — succeeds with progress ---- */

static ozayn_task_result_t handle_demo(ozayn_task_t *task, void *ctx) {
    (void)ctx;

    LOG_INFO("DEMO", "Task #%u: starting demo work", task->id);

    /* Simulate progress */
    task->progress = 25;
    LOG_DEBUG("DEMO", "Task #%u: progress %d%%", task->id, task->progress);

    task->progress = 50;
    LOG_DEBUG("DEMO", "Task #%u: progress %d%%", task->id, task->progress);

    task->progress = 75;
    LOG_DEBUG("DEMO", "Task #%u: progress %d%%", task->id, task->progress);

    task->progress = 100;
    LOG_DEBUG("DEMO", "Task #%u: progress %d%%", task->id, task->progress);

    LOG_INFO("DEMO", "Task #%u: demo work complete", task->id);

    return OZAYN_TASK_RESULT_SUCCESS;
}

/* ---- DEMO_FAIL handler — always fails ---- */

static ozayn_task_result_t handle_demo_fail(ozayn_task_t *task, void *ctx) {
    (void)ctx;

    LOG_INFO("DEMO_FAIL", "Task #%u: starting (will fail)", task->id);

    task->progress = 30;
    LOG_DEBUG("DEMO_FAIL", "Task #%u: progress %d%%", task->id, task->progress);

    LOG_ERROR("DEMO_FAIL", "Task #%u: simulated failure", task->id);

    return OZAYN_TASK_RESULT_FAILURE;
}
