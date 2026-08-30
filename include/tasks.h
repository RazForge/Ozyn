#ifndef OZAYN_TASKS_H
#define OZAYN_TASKS_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * ozayn_tasks.h — Task manager.
 *
 * Manages units of work above individual commands.
 * A command is an instruction. A task is managed work with lifecycle.
 *
 * Command: "Do this."        (immediate, transient)
 * Task:    "This work exists." (tracked, persistent across cycles)
 */

/* ---- Task types ---- */
typedef enum {
    OZAYN_TASK_NONE          = 0,
    OZAYN_TASK_DEMO          = 1,
    OZAYN_TASK_DEMO_FAIL     = 2,
} ozayn_task_type_t;

/* ---- Task source ---- */
typedef enum {
    OZAYN_TASK_SRC_CORE    = 0,
    OZAYN_TASK_SRC_COMMAND = 1,
} ozayn_task_source_t;

/* ---- Task state ---- */
typedef enum {
    OZAYN_TASK_CREATED   = 0,
    OZAYN_TASK_QUEUED    = 1,
    OZAYN_TASK_RUNNING   = 2,
    OZAYN_TASK_COMPLETED = 3,
    OZAYN_TASK_FAILED    = 4,
    OZAYN_TASK_CANCELLED = 5,
} ozayn_task_state_t;

/* ---- Task result ---- */
typedef enum {
    OZAYN_TASK_RESULT_SUCCESS = 0,
    OZAYN_TASK_RESULT_FAILURE = 1,
} ozayn_task_result_t;

/* ---- Task structure ---- */
typedef struct {
    uint32_t            id;
    ozayn_task_type_t   type;
    ozayn_task_source_t source;
    ozayn_task_state_t  state;
    int                 progress;     /* -1 = indeterminate, 0–100 = percentage */
    int                 priority;     /* scheduler priority (0=background, 4=critical) */
    int                 cancel_flag;  /* cooperative cancellation flag */
    time_t              created_at;
    time_t              started_at;
    time_t              finished_at;
} ozayn_task_t;

/* ---- Handler function type ---- */
typedef ozayn_task_result_t (*ozayn_task_handler_t)(ozayn_task_t *task, void *ctx);

/* ---- Registry entry ---- */
typedef struct {
    ozayn_task_type_t   type;
    ozayn_task_handler_t handler;
    const char         *name;
} ozayn_task_entry_t;

/* ---- Task engine ---- */
typedef struct {
    const ozayn_task_entry_t *registry;
    int                      registry_size;
    ozayn_task_t             tasks[64];
    int                      task_count;
    void                    *events;   /* ozayn_event_engine_t* */
    void                    *recovery; /* ozayn_recovery_t* */
    uint32_t                 next_id;
    int                      initialized;
} ozayn_task_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_task_manager_init(ozayn_task_manager_t *mgr);
void           ozayn_task_manager_shutdown(ozayn_task_manager_t *mgr);

/* ---- Binding ---- */
void ozayn_task_manager_set_events(ozayn_task_manager_t *mgr, void *events);
void ozayn_task_manager_set_recovery(ozayn_task_manager_t *mgr, void *recovery);

/* ---- Submit task ---- */
ozayn_task_t *ozayn_task_manager_submit(ozayn_task_manager_t *mgr,
                                         ozayn_task_type_t type,
                                         ozayn_task_source_t source);

/* ---- Cancel task ---- */
ozayn_result_t ozayn_task_manager_cancel(ozayn_task_manager_t *mgr,
                                          uint32_t task_id);

/* ---- Query ---- */
ozayn_task_t  *ozayn_task_manager_get(ozayn_task_manager_t *mgr, uint32_t task_id);
int            ozayn_task_manager_active_count(const ozayn_task_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_task_type_name(ozayn_task_type_t type);
const char *ozayn_task_source_name(ozayn_task_source_t src);
const char *ozayn_task_state_name(ozayn_task_state_t state);
const char *ozayn_task_result_name(ozayn_task_result_t result);

#endif
