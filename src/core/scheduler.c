#include "scheduler.h"
#include "tasks.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * NAMES
 * ================================================================ */

const char *ozayn_sched_priority_name(ozayn_sched_priority_t priority) {
    switch (priority) {
        case OZAYN_SCHED_PRIORITY_BACKGROUND: return "BACKGROUND";
        case OZAYN_SCHED_PRIORITY_LOW:        return "LOW";
        case OZAYN_SCHED_PRIORITY_NORMAL:     return "NORMAL";
        case OZAYN_SCHED_PRIORITY_HIGH:       return "HIGH";
        case OZAYN_SCHED_PRIORITY_CRITICAL:   return "CRITICAL";
    }
    return "UNKNOWN";
}

const char *ozayn_sched_state_name(ozayn_sched_state_t state) {
    switch (state) {
        case OZAYN_SCHED_STATE_NONE:     return "NONE";
        case OZAYN_SCHED_STATE_READY:    return "READY";
        case OZAYN_SCHED_STATE_RUNNING:  return "RUNNING";
        case OZAYN_SCHED_STATE_WAITING:  return "WAITING";
        case OZAYN_SCHED_STATE_BLOCKED:  return "BLOCKED";
    }
    return "UNKNOWN";
}

const char *ozayn_sched_wait_reason_name(ozayn_sched_wait_reason_t reason) {
    switch (reason) {
        case OZAYN_SCHED_WAIT_NONE:     return "none";
        case OZAYN_SCHED_WAIT_EVENT:    return "EVENT";
        case OZAYN_SCHED_WAIT_RESOURCE: return "RESOURCE";
        case OZAYN_SCHED_WAIT_TIME:     return "TIME";
    }
    return "UNKNOWN";
}

/* ================================================================
 * EVENT HELPERS
 * ================================================================ */

static void publish_sched_event(ozayn_scheduler_manager_t *mgr,
                                ozayn_event_type_t type,
                                const ozayn_sched_entry_t *entry) {
    if (mgr && mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             type, OZAYN_SRC_CORE, (void *)entry);
    }
}

/* ================================================================
 * ENTRY MANAGEMENT
 * ================================================================ */

static ozayn_sched_entry_t *find_entry(ozayn_sched_entry_t *arr, int count, uint32_t task_id) {
    for (int i = 0; i < count; i++) {
        if (arr[i].active && arr[i].task_id == task_id) {
            return &arr[i];
        }
    }
    return NULL;
}

static const ozayn_sched_entry_t *find_entry_const(const ozayn_sched_entry_t *arr,
                                                     int count, uint32_t task_id) {
    for (int i = 0; i < count; i++) {
        if (arr[i].active && arr[i].task_id == task_id) {
            return &arr[i];
        }
    }
    return NULL;
}

static ozayn_sched_entry_t *find_free_slot(ozayn_sched_entry_t *arr, int max) {
    for (int i = 0; i < max; i++) {
        if (!arr[i].active) {
            return &arr[i];
        }
    }
    return NULL;
}

static void remove_entry(ozayn_sched_entry_t *arr, int *count, ozayn_sched_entry_t *entry) {
    (void)arr;
    (void)count;
    entry->active = 0;
}

static void compact_array(ozayn_sched_entry_t *arr, int max, int *count) {
    int write = 0;
    for (int read = 0; read < max; read++) {
        if (arr[read].active) {
            if (write != read) {
                arr[write] = arr[read];
                memset(&arr[read], 0, sizeof(ozayn_sched_entry_t));
            }
            write++;
        }
    }
    for (int i = write; i < max; i++) {
        memset(&arr[i], 0, sizeof(ozayn_sched_entry_t));
    }
    *count = write;
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

ozayn_result_t ozayn_scheduler_init(ozayn_scheduler_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_scheduler_manager_t));
    mgr->enabled = enabled;
    mgr->aging_enabled = 1;
    mgr->max_tasks_per_source = 16;
    mgr->initialized = 1;

    LOG_INFO("SCHEDULER", "Scheduler initialized (enabled=%s, aging=%s, max_per_source=%d)",
             enabled ? "yes" : "no",
             mgr->aging_enabled ? "yes" : "no",
             mgr->max_tasks_per_source);

    return OZAYN_OK;
}

void ozayn_scheduler_shutdown(ozayn_scheduler_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Warn about active work */
    if (mgr->stats.current_running > 0) {
        LOG_WARN("SCHEDULER", "%d task(s) still running at shutdown", mgr->stats.current_running);
    }
    if (mgr->stats.current_ready > 0) {
        LOG_WARN("SCHEDULER", "%d task(s) still ready at shutdown", mgr->stats.current_ready);
    }
    if (mgr->stats.current_waiting > 0) {
        LOG_WARN("SCHEDULER", "%d task(s) still waiting at shutdown", mgr->stats.current_waiting);
    }

    mgr->running = NULL;
    mgr->ready_count = 0;
    mgr->waiting_count = 0;
    mgr->blocked_count = 0;
    mgr->initialized = 0;

    LOG_INFO("SCHEDULER", "Scheduler shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_scheduler_set_task_mgr(ozayn_scheduler_manager_t *mgr, void *task_mgr) {
    if (mgr) mgr->task_mgr = task_mgr;
}

void ozayn_scheduler_set_events(ozayn_scheduler_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_scheduler_set_recovery(ozayn_scheduler_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

void ozayn_scheduler_set_resource_mgr(ozayn_scheduler_manager_t *mgr, void *resource_mgr) {
    if (mgr) mgr->resource_mgr = resource_mgr;
}

/* ================================================================
 * CONFIGURATION
 * ================================================================ */

void ozayn_scheduler_set_aging(ozayn_scheduler_manager_t *mgr, int enabled) {
    if (mgr) mgr->aging_enabled = enabled;
}

void ozayn_scheduler_set_max_tasks_per_source(ozayn_scheduler_manager_t *mgr, int max) {
    if (mgr && max > 0) mgr->max_tasks_per_source = max;
}

/* ================================================================
 * TASK SUBMISSION
 * ================================================================ */

ozayn_result_t ozayn_scheduler_submit(ozayn_scheduler_manager_t *mgr,
                                       uint32_t task_id,
                                       ozayn_sched_priority_t priority,
                                       const char *source) {
    if (!mgr) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Check capacity */
    if (mgr->ready_count >= OZAYN_SCHEDULER_MAX_READY) {
        LOG_WARN("SCHEDULER", "Ready queue full — rejecting task %u", task_id);
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_RESOURCE, OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION, "SCHEDULER",
                                 "Ready queue full");
        }
        return OZAYN_ERR;
    }

    /* Check per-source quota */
    if (source && mgr->max_tasks_per_source > 0) {
        int source_count = 0;
        for (int i = 0; i < mgr->ready_count; i++) {
            if (mgr->ready[i].active && strcmp(mgr->ready[i].source, source) == 0)
                source_count++;
        }
        for (int i = 0; i < mgr->waiting_count; i++) {
            if (mgr->waiting[i].active && strcmp(mgr->waiting[i].source, source) == 0)
                source_count++;
        }
        for (int i = 0; i < mgr->blocked_count; i++) {
            if (mgr->blocked[i].active && strcmp(mgr->blocked[i].source, source) == 0)
                source_count++;
        }
        if (source_count >= mgr->max_tasks_per_source) {
            LOG_WARN("SCHEDULER", "Source '%s' hit quota (%d) — rejecting task %u",
                     source, mgr->max_tasks_per_source, task_id);
            return OZAYN_ERR;
        }
    }

    /* Find free slot */
    ozayn_sched_entry_t *entry = find_free_slot(mgr->ready, OZAYN_SCHEDULER_MAX_READY);
    if (!entry) return OZAYN_ERR;

    memset(entry, 0, sizeof(ozayn_sched_entry_t));
    entry->active = 1;
    entry->task_id = task_id;
    entry->base_priority = priority;
    entry->effective_priority = priority;
    entry->sched_state = OZAYN_SCHED_STATE_READY;
    entry->wait_reason = OZAYN_SCHED_WAIT_NONE;
    if (source) strncpy(entry->source, source, OZAYN_SCHEDULER_MAX_SOURCE_LEN - 1);
    entry->submitted_at = time(NULL);
    entry->last_run_at = 0;
    entry->wait_started_at = 0;
    entry->wait_count = 0;
    entry->run_count = 0;
    entry->age_boost = 0;

    mgr->ready_count++;
    mgr->stats.total_submitted++;

    LOG_INFO("SCHEDULER", "Task %u submitted (priority=%s, source=%s)",
             task_id, ozayn_sched_priority_name(priority),
             source ? source : "none");

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_READY, entry);

    return OZAYN_OK;
}

/* ================================================================
 * TASK CONTROL
 * ================================================================ */

ozayn_result_t ozayn_scheduler_cancel(ozayn_scheduler_manager_t *mgr, uint32_t task_id) {
    if (!mgr) return OZAYN_ERR_NULL;

    /* Try ready queue */
    ozayn_sched_entry_t *entry = find_entry(mgr->ready, mgr->ready_count, task_id);
    if (entry) {
        LOG_INFO("SCHEDULER", "Task %u cancelled (was READY)", task_id);
        publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_CANCELLED, entry);
        remove_entry(mgr->ready, &mgr->ready_count, entry);
        compact_array(mgr->ready, OZAYN_SCHEDULER_MAX_READY, &mgr->ready_count);
        mgr->stats.total_cancelled++;
        return OZAYN_OK;
    }

    /* Try waiting list */
    entry = find_entry(mgr->waiting, mgr->waiting_count, task_id);
    if (entry) {
        LOG_INFO("SCHEDULER", "Task %u cancelled (was WAITING)", task_id);
        publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_CANCELLED, entry);
        remove_entry(mgr->waiting, &mgr->waiting_count, entry);
        compact_array(mgr->waiting, OZAYN_SCHEDULER_MAX_WAITING, &mgr->waiting_count);
        mgr->stats.total_cancelled++;
        return OZAYN_OK;
    }

    /* Try blocked list */
    entry = find_entry(mgr->blocked, mgr->blocked_count, task_id);
    if (entry) {
        LOG_INFO("SCHEDULER", "Task %u cancelled (was BLOCKED)", task_id);
        publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_CANCELLED, entry);
        remove_entry(mgr->blocked, &mgr->blocked_count, entry);
        compact_array(mgr->blocked, OZAYN_SCHEDULER_MAX_BLOCKED, &mgr->blocked_count);
        mgr->stats.total_cancelled++;
        return OZAYN_OK;
    }

    /* If running, just mark — will be cleaned up on next tick */
    if (mgr->running && mgr->running->task_id == task_id) {
        LOG_WARN("SCHEDULER", "Task %u cancel requested (currently RUNNING) — deferred", task_id);
        /* The running entry will be cleaned up after dispatch returns */
        return OZAYN_OK;
    }

    return OZAYN_ERR; /* not found */
}

ozayn_result_t ozayn_scheduler_wake(ozayn_scheduler_manager_t *mgr, uint32_t task_id) {
    if (!mgr) return OZAYN_ERR_NULL;

    /* Only waiting or blocked tasks can be woken */
    ozayn_sched_entry_t *entry = find_entry(mgr->waiting, mgr->waiting_count, task_id);
    if (!entry) {
        entry = find_entry(mgr->blocked, mgr->blocked_count, task_id);
        if (entry) {
            remove_entry(mgr->blocked, &mgr->blocked_count, entry);
            compact_array(mgr->blocked, OZAYN_SCHEDULER_MAX_BLOCKED, &mgr->blocked_count);
        } else {
            return OZAYN_ERR; /* not found or not in wakable state */
        }
    } else {
        remove_entry(mgr->waiting, &mgr->waiting_count, entry);
        compact_array(mgr->waiting, OZAYN_SCHEDULER_MAX_WAITING, &mgr->waiting_count);
    }

    /* Move to ready queue */
    if (mgr->ready_count >= OZAYN_SCHEDULER_MAX_READY) {
        LOG_ERROR("SCHEDULER", "Ready queue full — cannot wake task %u", task_id);
        return OZAYN_ERR;
    }

    ozayn_sched_entry_t *ready_slot = find_free_slot(mgr->ready, OZAYN_SCHEDULER_MAX_READY);
    if (!ready_slot) return OZAYN_ERR;

    *ready_slot = *entry;
    ready_slot->sched_state = OZAYN_SCHED_STATE_READY;
    ready_slot->wait_reason = OZAYN_SCHED_WAIT_NONE;
    ready_slot->wait_started_at = 0;
    memset(entry, 0, sizeof(ozayn_sched_entry_t));

    mgr->ready_count++;

    LOG_INFO("SCHEDULER", "Task %u woken -> READY (was %s)",
             task_id, ozayn_sched_state_name(entry->sched_state));

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_RESUMED, ready_slot);

    return OZAYN_OK;
}

ozayn_result_t ozayn_scheduler_block(ozayn_scheduler_manager_t *mgr, uint32_t task_id,
                                      ozayn_sched_wait_reason_t reason) {
    if (!mgr) return OZAYN_ERR_NULL;

    ozayn_sched_entry_t *entry = find_entry(mgr->ready, mgr->ready_count, task_id);
    if (!entry) return OZAYN_ERR;

    /* Move to blocked list */
    if (mgr->blocked_count >= OZAYN_SCHEDULER_MAX_BLOCKED) {
        LOG_WARN("SCHEDULER", "Blocked list full — cannot block task %u", task_id);
        return OZAYN_ERR;
    }

    ozayn_sched_entry_t *blocked_slot = find_free_slot(mgr->blocked, OZAYN_SCHEDULER_MAX_BLOCKED);
    if (!blocked_slot) return OZAYN_ERR;

    *blocked_slot = *entry;
    blocked_slot->sched_state = OZAYN_SCHED_STATE_BLOCKED;
    blocked_slot->wait_reason = reason;
    blocked_slot->wait_started_at = time(NULL);
    blocked_slot->wait_count++;
    memset(entry, 0, sizeof(ozayn_sched_entry_t));

    compact_array(mgr->ready, OZAYN_SCHEDULER_MAX_READY, &mgr->ready_count);
    mgr->blocked_count++;
    mgr->stats.total_blocked++;

    LOG_INFO("SCHEDULER", "Task %u -> BLOCKED (reason=%s)",
             task_id, ozayn_sched_wait_reason_name(reason));

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_BLOCKED, blocked_slot);

    return OZAYN_OK;
}

ozayn_result_t ozayn_scheduler_wait(ozayn_scheduler_manager_t *mgr, uint32_t task_id,
                                     ozayn_sched_wait_reason_t reason) {
    if (!mgr) return OZAYN_ERR_NULL;

    ozayn_sched_entry_t *entry = find_entry(mgr->ready, mgr->ready_count, task_id);
    if (!entry) return OZAYN_ERR;

    /* Move to waiting list */
    if (mgr->waiting_count >= OZAYN_SCHEDULER_MAX_WAITING) {
        LOG_WARN("SCHEDULER", "Waiting list full — cannot wait task %u", task_id);
        return OZAYN_ERR;
    }

    ozayn_sched_entry_t *wait_slot = find_free_slot(mgr->waiting, OZAYN_SCHEDULER_MAX_WAITING);
    if (!wait_slot) return OZAYN_ERR;

    *wait_slot = *entry;
    wait_slot->sched_state = OZAYN_SCHED_STATE_WAITING;
    wait_slot->wait_reason = reason;
    wait_slot->wait_started_at = time(NULL);
    wait_slot->wait_count++;
    memset(entry, 0, sizeof(ozayn_sched_entry_t));

    compact_array(mgr->ready, OZAYN_SCHEDULER_MAX_READY, &mgr->ready_count);
    mgr->waiting_count++;
    mgr->stats.total_waited++;

    LOG_INFO("SCHEDULER", "Task %u -> WAITING (reason=%s)",
             task_id, ozayn_sched_wait_reason_name(reason));

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_WAITING, wait_slot);

    return OZAYN_OK;
}

/* ================================================================
 * PRIORITY MANAGEMENT
 * ================================================================ */

ozayn_result_t ozayn_scheduler_set_priority(ozayn_scheduler_manager_t *mgr,
                                             uint32_t task_id,
                                             ozayn_sched_priority_t priority) {
    if (!mgr) return OZAYN_ERR_NULL;

    /* Find entry in any list */
    ozayn_sched_entry_t *entry = find_entry(mgr->ready, mgr->ready_count, task_id);
    if (!entry) entry = find_entry(mgr->waiting, mgr->waiting_count, task_id);
    if (!entry) entry = find_entry(mgr->blocked, mgr->blocked_count, task_id);
    if (!entry) return OZAYN_ERR;

    ozayn_sched_priority_t old = entry->base_priority;
    entry->base_priority = priority;
    entry->effective_priority = priority;
    entry->age_boost = 0;

    LOG_INFO("SCHEDULER", "Task %u priority: %s -> %s",
             task_id, ozayn_sched_priority_name(old),
             ozayn_sched_priority_name(priority));

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_PRIORITY_CHANGED, entry);

    return OZAYN_OK;
}

ozayn_sched_priority_t ozayn_scheduler_get_priority(const ozayn_scheduler_manager_t *mgr,
                                                     uint32_t task_id) {
    if (!mgr) return OZAYN_SCHED_PRIORITY_NORMAL;

    const ozayn_sched_entry_t *entry = find_entry_const(mgr->ready, mgr->ready_count, task_id);
    if (!entry) entry = find_entry_const(mgr->waiting, mgr->waiting_count, task_id);
    if (!entry) entry = find_entry_const(mgr->blocked, mgr->blocked_count, task_id);
    if (!entry) return OZAYN_SCHED_PRIORITY_NORMAL;

    return entry->effective_priority;
}

/* ================================================================
 * AGING — prevent starvation
 * ================================================================ */

static void apply_aging(ozayn_scheduler_manager_t *mgr) {
    if (!mgr->aging_enabled) return;

    time_t now = time(NULL);

    /* Age waiting tasks */
    for (int i = 0; i < mgr->waiting_count; i++) {
        ozayn_sched_entry_t *e = &mgr->waiting[i];
        if (!e->active) continue;

        int wait_seconds = (int)(now - e->wait_started_at);
        int age_boost = wait_seconds / OZAYN_SCHEDULER_AGING_INTERVAL;
        if (age_boost > OZAYN_SCHEDULER_AGING_MAX_BOOST)
            age_boost = OZAYN_SCHEDULER_AGING_MAX_BOOST;

        if (age_boost > e->age_boost) {
            int old_effective = (int)e->effective_priority;
            e->age_boost = age_boost;
            int new_effective = (int)e->base_priority + age_boost;
            if (new_effective > OZAYN_SCHED_PRIORITY_CRITICAL)
                new_effective = OZAYN_SCHED_PRIORITY_CRITICAL;
            e->effective_priority = (ozayn_sched_priority_t)new_effective;

            if (new_effective != old_effective) {
                LOG_DEBUG("SCHEDULER", "Task %u aged: %s -> %s (wait=%ds)",
                          e->task_id,
                          ozayn_sched_priority_name((ozayn_sched_priority_t)old_effective),
                          ozayn_sched_priority_name(e->effective_priority),
                          wait_seconds);
                mgr->stats.starvation_preventions++;
            }
        }
    }

    /* Also age ready tasks that have been waiting in the queue */
    for (int i = 0; i < mgr->ready_count; i++) {
        ozayn_sched_entry_t *e = &mgr->ready[i];
        if (!e->active) continue;

        int wait_seconds = (int)(now - e->submitted_at);
        int age_boost = wait_seconds / OZAYN_SCHEDULER_AGING_INTERVAL;
        if (age_boost > OZAYN_SCHEDULER_AGING_MAX_BOOST)
            age_boost = OZAYN_SCHEDULER_AGING_MAX_BOOST;

        if (age_boost > e->age_boost) {
            e->age_boost = age_boost;
            int new_effective = (int)e->base_priority + age_boost;
            if (new_effective > OZAYN_SCHED_PRIORITY_CRITICAL)
                new_effective = OZAYN_SCHED_PRIORITY_CRITICAL;
            e->effective_priority = (ozayn_sched_priority_t)new_effective;
        }
    }
}

/* ================================================================
 * EXECUTION — tick and dispatch
 * ================================================================ */

/* Find highest priority ready task */
static ozayn_sched_entry_t *select_highest_priority(ozayn_scheduler_manager_t *mgr) {
    ozayn_sched_entry_t *best = NULL;

    for (int i = 0; i < mgr->ready_count; i++) {
        ozayn_sched_entry_t *e = &mgr->ready[i];
        if (!e->active) continue;

        if (!best || e->effective_priority > best->effective_priority ||
            (e->effective_priority == best->effective_priority &&
             e->submitted_at < best->submitted_at)) {
            best = e;
        }
    }

    return best;
}

int ozayn_scheduler_tick(ozayn_scheduler_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    /* Apply aging */
    apply_aging(mgr);

    /* If nothing running, try to dispatch */
    if (!mgr->running || !mgr->running->active) {
        return ozayn_scheduler_dispatch(mgr);
    }

    return 0;
}

int ozayn_scheduler_dispatch(ozayn_scheduler_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    /* Find highest priority ready task */
    ozayn_sched_entry_t *selected = select_highest_priority(mgr);
    if (!selected) return 0;

    /* Remove from ready queue */
    /* Mark as running */
    static ozayn_sched_entry_t running_entry;
    running_entry = *selected;
    running_entry.sched_state = OZAYN_SCHED_STATE_RUNNING;
    running_entry.last_run_at = time(NULL);
    running_entry.run_count++;
    remove_entry(mgr->ready, &mgr->ready_count, selected);
    compact_array(mgr->ready, OZAYN_SCHEDULER_MAX_READY, &mgr->ready_count);
    mgr->running = &running_entry;

    publish_sched_event(mgr, OZAYN_EVENT_SCHED_TASK_STARTED, &running_entry);

    LOG_INFO("SCHEDULER", "Dispatched task %u (priority=%s, run #%d)",
             running_entry.task_id,
             ozayn_sched_priority_name(running_entry.effective_priority),
             running_entry.run_count);

    /* Execute task via Task Manager */
    int executed = 0;
    if (mgr->task_mgr) {
        ozayn_task_manager_t *tm = (ozayn_task_manager_t *)mgr->task_mgr;
        ozayn_task_t *task = ozayn_task_manager_get(tm, running_entry.task_id);
        if (task) {
            /* Look up handler */
            const ozayn_task_entry_t *reg = tm->registry;
            for (int i = 0; i < tm->registry_size; i++) {
                if (reg[i].type == task->type && reg[i].handler) {
                    /* Transition task to RUNNING */
                    task->state = OZAYN_TASK_RUNNING;
                    task->started_at = time(NULL);

                    /* Execute */
                    ozayn_task_result_t result = reg[i].handler(task, tm);

                    task->finished_at = time(NULL);
                    if (result == OZAYN_TASK_RESULT_SUCCESS) {
                        task->state = OZAYN_TASK_COMPLETED;
                        mgr->stats.total_completed++;
                        if (mgr->events) {
                            ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                                 OZAYN_EVENT_TASK_COMPLETED,
                                                 OZAYN_SRC_CORE, (void *)task);
                        }
                    } else {
                        task->state = OZAYN_TASK_FAILED;
                        mgr->stats.total_failed++;
                        if (mgr->events) {
                            ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                                 OZAYN_EVENT_TASK_FAILED,
                                                 OZAYN_SRC_CORE, (void *)task);
                        }
                        if (mgr->recovery) {
                            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                                 OZAYN_ERRCAT_RUNTIME, OZAYN_LOG_ERROR,
                                                 OZAYN_SCOPE_OPERATION, "SCHEDULER",
                                                 "Task execution failed");
                        }
                    }

                    executed = 1;
                    break;
                }
            }
        }
    }

    /* Clean up running entry */
    mgr->stats.total_executed++;
    mgr->running = NULL;

    return executed;
}

/* ================================================================
 * QUERY
 * ================================================================ */

const ozayn_sched_entry_t *ozayn_scheduler_find(const ozayn_scheduler_manager_t *mgr,
                                                 uint32_t task_id) {
    if (!mgr) return NULL;

    const ozayn_sched_entry_t *entry = find_entry_const(mgr->ready, mgr->ready_count, task_id);
    if (entry) return entry;
    entry = find_entry_const(mgr->waiting, mgr->waiting_count, task_id);
    if (entry) return entry;
    entry = find_entry_const(mgr->blocked, mgr->blocked_count, task_id);
    if (entry) return entry;

    /* Check running */
    if (mgr->running && mgr->running->active && mgr->running->task_id == task_id) {
        return mgr->running;
    }

    return NULL;
}

int ozayn_scheduler_ready_count(const ozayn_scheduler_manager_t *mgr) {
    return mgr ? mgr->ready_count : 0;
}

int ozayn_scheduler_waiting_count(const ozayn_scheduler_manager_t *mgr) {
    return mgr ? mgr->waiting_count : 0;
}

int ozayn_scheduler_blocked_count(const ozayn_scheduler_manager_t *mgr) {
    return mgr ? mgr->blocked_count : 0;
}

ozayn_sched_stats_t ozayn_scheduler_stats(const ozayn_scheduler_manager_t *mgr) {
    ozayn_sched_stats_t zero = { 0 };
    if (!mgr) return zero;
    ozayn_sched_stats_t stats = mgr->stats;
    /* Live counts */
    stats.current_ready = mgr->ready_count;
    stats.current_waiting = mgr->waiting_count;
    stats.current_blocked = mgr->blocked_count;
    stats.current_running = (mgr->running && mgr->running->active) ? 1 : 0;
    return stats;
}
