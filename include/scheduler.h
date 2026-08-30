#ifndef OZAYN_SCHEDULER_H
#define OZAYN_SCHEDULER_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * scheduler.h — Scheduler & Priority Engine (Stage 17).
 *
 * Answers: "Which work should run now, which should wait,
 *           and how important is each piece of work?"
 *
 * The Scheduler decides execution order. Task Manager owns task lifecycle.
 * This separation is critical.
 *
 * Architecture:
 *   Task Manager (creates/tracks tasks)
 *        ↓
 *   Scheduler (decides execution order)
 *        ↓
 *   Execution (dispatches to handler)
 */

/* ---- Constants ---- */
#define OZAYN_SCHEDULER_MAX_READY       128
#define OZAYN_SCHEDULER_MAX_WAITING      64
#define OZAYN_SCHEDULER_MAX_BLOCKED      32
#define OZAYN_SCHEDULER_MAX_SOURCE_LEN   32
#define OZAYN_SCHEDULER_AGING_INTERVAL  10   /* seconds before priority boost */
#define OZAYN_SCHEDULER_AGING_MAX_BOOST   2   /* max priority levels from aging */

/* ---- Scheduling priority ---- */
typedef enum {
    OZAYN_SCHED_PRIORITY_BACKGROUND = 0,
    OZAYN_SCHED_PRIORITY_LOW        = 1,
    OZAYN_SCHED_PRIORITY_NORMAL     = 2,
    OZAYN_SCHED_PRIORITY_HIGH       = 3,
    OZAYN_SCHED_PRIORITY_CRITICAL   = 4,
} ozayn_sched_priority_t;

/* ---- Scheduling state ---- */
typedef enum {
    OZAYN_SCHED_STATE_NONE       = 0,
    OZAYN_SCHED_STATE_READY      = 1,
    OZAYN_SCHED_STATE_RUNNING    = 2,
    OZAYN_SCHED_STATE_WAITING    = 3,
    OZAYN_SCHED_STATE_BLOCKED    = 4,
} ozayn_sched_state_t;

/* ---- Wait reason ---- */
typedef enum {
    OZAYN_SCHED_WAIT_NONE       = 0,
    OZAYN_SCHED_WAIT_EVENT      = 1,  /* waiting for an event */
    OZAYN_SCHED_WAIT_RESOURCE   = 2,  /* waiting for a resource */
    OZAYN_SCHED_WAIT_TIME       = 3,  /* sleeping for a duration */
} ozayn_sched_wait_reason_t;

/* ---- Scheduling entry (per-task scheduling metadata) ---- */
typedef struct {
    int                       active;
    uint32_t                  task_id;          /* reference to task in Task Manager */
    ozayn_sched_priority_t    base_priority;    /* assigned priority */
    ozayn_sched_priority_t    effective_priority; /* priority after aging */
    ozayn_sched_state_t       sched_state;
    ozayn_sched_wait_reason_t wait_reason;
    char                      source[OZAYN_SCHEDULER_MAX_SOURCE_LEN]; /* who submitted */
    time_t                    submitted_at;
    time_t                    last_run_at;
    time_t                    wait_started_at;
    int                       wait_count;       /* times moved to waiting */
    int                       run_count;        /* times executed */
    int                       age_boost;        /* current aging boost level */
} ozayn_sched_entry_t;

/* ---- Scheduling statistics ---- */
typedef struct {
    int total_submitted;
    int total_executed;
    int total_completed;
    int total_failed;
    int total_cancelled;
    int total_waited;
    int total_blocked;
    int total_ready;
    int current_running;
    int current_ready;
    int current_waiting;
    int current_blocked;
    int queue_length[5];  /* per-priority queue length */
    int starvation_preventions;
} ozayn_sched_stats_t;

/* ---- Scheduler manager ---- */
typedef struct {
    int                        enabled;
    int                        initialized;
    /* Ready queues — one per priority level */
    ozayn_sched_entry_t        ready[OZAYN_SCHEDULER_MAX_READY];
    int                        ready_count;
    /* Waiting list */
    ozayn_sched_entry_t        waiting[OZAYN_SCHEDULER_MAX_WAITING];
    int                        waiting_count;
    /* Blocked list */
    ozayn_sched_entry_t        blocked[OZAYN_SCHEDULER_MAX_BLOCKED];
    int                        blocked_count;
    /* Currently running */
    ozayn_sched_entry_t       *running;
    /* Statistics */
    ozayn_sched_stats_t        stats;
    /* Bindings */
    void                      *task_mgr;    /* ozayn_task_manager_t* */
    void                      *events;      /* ozayn_event_engine_t* */
    void                      *recovery;    /* ozayn_recovery_t* */
    void                      *resource_mgr; /* ozayn_resource_manager_t* */
    /* Configuration */
    int                        aging_enabled;
    int                        max_tasks_per_source;
} ozayn_scheduler_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_scheduler_init(ozayn_scheduler_manager_t *mgr, int enabled);
void           ozayn_scheduler_shutdown(ozayn_scheduler_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_scheduler_set_task_mgr(ozayn_scheduler_manager_t *mgr, void *task_mgr);
void ozayn_scheduler_set_events(ozayn_scheduler_manager_t *mgr, void *events);
void ozayn_scheduler_set_recovery(ozayn_scheduler_manager_t *mgr, void *recovery);
void ozayn_scheduler_set_resource_mgr(ozayn_scheduler_manager_t *mgr, void *resource_mgr);

/* ---- Configuration ---- */
void ozayn_scheduler_set_aging(ozayn_scheduler_manager_t *mgr, int enabled);
void ozayn_scheduler_set_max_tasks_per_source(ozayn_scheduler_manager_t *mgr, int max);

/* ---- Task submission ---- */
ozayn_result_t ozayn_scheduler_submit(ozayn_scheduler_manager_t *mgr,
                                       uint32_t task_id,
                                       ozayn_sched_priority_t priority,
                                       const char *source);

/* ---- Task control ---- */
ozayn_result_t ozayn_scheduler_cancel(ozayn_scheduler_manager_t *mgr, uint32_t task_id);
ozayn_result_t ozayn_scheduler_wake(ozayn_scheduler_manager_t *mgr, uint32_t task_id);
ozayn_result_t ozayn_scheduler_block(ozayn_scheduler_manager_t *mgr, uint32_t task_id,
                                      ozayn_sched_wait_reason_t reason);
ozayn_result_t ozayn_scheduler_wait(ozayn_scheduler_manager_t *mgr, uint32_t task_id,
                                     ozayn_sched_wait_reason_t reason);

/* ---- Priority management ---- */
ozayn_result_t ozayn_scheduler_set_priority(ozayn_scheduler_manager_t *mgr,
                                             uint32_t task_id,
                                             ozayn_sched_priority_t priority);
ozayn_sched_priority_t ozayn_scheduler_get_priority(const ozayn_scheduler_manager_t *mgr,
                                                     uint32_t task_id);

/* ---- Execution ---- */
int ozayn_scheduler_tick(ozayn_scheduler_manager_t *mgr);
int ozayn_scheduler_dispatch(ozayn_scheduler_manager_t *mgr);

/* ---- Query ---- */
const ozayn_sched_entry_t *ozayn_scheduler_find(const ozayn_scheduler_manager_t *mgr,
                                                 uint32_t task_id);
int ozayn_scheduler_ready_count(const ozayn_scheduler_manager_t *mgr);
int ozayn_scheduler_waiting_count(const ozayn_scheduler_manager_t *mgr);
int ozayn_scheduler_blocked_count(const ozayn_scheduler_manager_t *mgr);
ozayn_sched_stats_t ozayn_scheduler_stats(const ozayn_scheduler_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_sched_priority_name(ozayn_sched_priority_t priority);
const char *ozayn_sched_state_name(ozayn_sched_state_t state);
const char *ozayn_sched_wait_reason_name(ozayn_sched_wait_reason_t reason);

#endif
