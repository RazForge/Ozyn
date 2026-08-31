#include "lifecycle.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/*
 * lifecycle.c — Startup & Shutdown Coordinator (Stage 22).
 *
 * Orchestrates the initialization and teardown of all Core components
 * through a defined phase order, dependency-aware sequencing, criticality
 * handling, and graceful shutdown with timeout/escalation support.
 */

/* ---------- Name helpers ---------- */

const char *ozayn_lc_phase_name(ozayn_lc_phase_t phase) {
    switch (phase) {
        case OZAYN_LC_PHASE_BOOTSTRAP:        return "BOOTSTRAP";
        case OZAYN_LC_PHASE_FOUNDATION:       return "FOUNDATION";
        case OZAYN_LC_PHASE_SECURITY:         return "SECURITY";
        case OZAYN_LC_PHASE_RUNTIME_SERVICES: return "RUNTIME_SERVICES";
        case OZAYN_LC_PHASE_MODULES_PLUGINS:  return "MODULES_PLUGINS";
        case OZAYN_LC_PHASE_MONITORING:       return "MONITORING";
        case OZAYN_LC_PHASE_READY:            return "READY";
        case OZAYN_LC_PHASE_COUNT:            return "COUNT";
    }
    return "UNKNOWN";
}

const char *ozayn_lc_state_name(ozayn_lc_state_t state) {
    switch (state) {
        case OZAYN_LC_STATE_CREATED:   return "CREATED";
        case OZAYN_LC_STATE_STARTING:  return "STARTING";
        case OZAYN_LC_STATE_RUNNING:   return "RUNNING";
        case OZAYN_LC_STATE_STOPPING:  return "STOPPING";
        case OZAYN_LC_STATE_STOPPED:   return "STOPPED";
        case OZAYN_LC_STATE_FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_lc_shutdown_reason_name(ozayn_lc_shutdown_reason_t reason) {
    switch (reason) {
        case OZAYN_LC_SHUTDOWN_NORMAL:        return "NORMAL";
        case OZAYN_LC_SHUTDOWN_USER_REQUEST:  return "USER_REQUEST";
        case OZAYN_LC_SHUTDOWN_SYSTEM:        return "SYSTEM";
        case OZAYN_LC_SHUTDOWN_CONFIG_CHANGE: return "CONFIG_CHANGE";
        case OZAYN_LC_SHUTDOWN_FATAL_ERROR:   return "FATAL_ERROR";
        case OZAYN_LC_SHUTDOWN_SECURITY:      return "SECURITY";
        case OZAYN_LC_SHUTDOWN_STARTUP_FAIL:  return "STARTUP_FAIL";
        case OZAYN_LC_SHUTDOWN_RESTART:       return "RESTART";
    }
    return "UNKNOWN";
}

const char *ozayn_lc_criticality_name(ozayn_lc_criticality_t crit) {
    switch (crit) {
        case OZAYN_LC_CRITICALITY_REQUIRED:    return "REQUIRED";
        case OZAYN_LC_CRITICALITY_OPTIONAL:    return "OPTIONAL";
        case OZAYN_LC_CRITICALITY_BEST_EFFORT: return "BEST_EFFORT";
    }
    return "UNKNOWN";
}

const char *ozayn_lc_component_state_name(ozayn_lc_component_state_t state) {
    switch (state) {
        case OZAYN_LC_COMP_CREATED:      return "CREATED";
        case OZAYN_LC_COMP_INITIALIZING: return "INITIALIZING";
        case OZAYN_LC_COMP_INITIALIZED:  return "INITIALIZED";
        case OZAYN_LC_COMP_STARTING:     return "STARTING";
        case OZAYN_LC_COMP_STARTED:      return "STARTED";
        case OZAYN_LC_COMP_STOPPING:     return "STOPPING";
        case OZAYN_LC_COMP_STOPPED:      return "STOPPED";
        case OZAYN_LC_COMP_FAILED:       return "FAILED";
        case OZAYN_LC_COMP_DISABLED:     return "DISABLED";
    }
    return "UNKNOWN";
}

/* ---------- Internal: publish lifecycle event ---------- */

static void publish_event(ozayn_lc_t *lc, int event_type, void *payload) {
    if (!lc || !lc->events) return;
    ozayn_events_publish((ozayn_event_engine_t *)lc->events,
                         (ozayn_event_type_t)event_type,
                         OZAYN_SRC_CORE, payload);
}

/* ---------- Internal: state transition ---------- */

static int transition(ozayn_lc_t *lc, ozayn_lc_state_t to) {
    if (!lc) return -1;

    ozayn_lc_state_t from = lc->state;
    int valid = 0;

    switch (from) {
        case OZAYN_LC_STATE_CREATED:
            valid = (to == OZAYN_LC_STATE_STARTING || to == OZAYN_LC_STATE_FAILED);
            break;
        case OZAYN_LC_STATE_STARTING:
            valid = (to == OZAYN_LC_STATE_RUNNING || to == OZAYN_LC_STATE_STOPPING || to == OZAYN_LC_STATE_FAILED);
            break;
        case OZAYN_LC_STATE_RUNNING:
            valid = (to == OZAYN_LC_STATE_STOPPING || to == OZAYN_LC_STATE_FAILED);
            break;
        case OZAYN_LC_STATE_STOPPING:
            valid = (to == OZAYN_LC_STATE_STOPPED || to == OZAYN_LC_STATE_FAILED);
            break;
        case OZAYN_LC_STATE_STOPPED:
        case OZAYN_LC_STATE_FAILED:
            valid = (to == OZAYN_LC_STATE_CREATED || to == OZAYN_LC_STATE_STARTING);
            break;
    }

    if (!valid) {
        LOG_ERROR("LIFECYCLE", "Invalid state transition: %s -> %s",
                  ozayn_lc_state_name(from), ozayn_lc_state_name(to));
        return -1;
    }

    lc->state = to;
    publish_event(lc, OZAYN_LC_EVENT_STATE_CHANGED, NULL);
    return 0;
}

/* ---------- Internal: build init order ---------- */

static void build_init_order(ozayn_lc_t *lc) {
    lc->init_order_count = 0;

    for (ozayn_lc_phase_t phase = OZAYN_LC_PHASE_BOOTSTRAP; phase < OZAYN_LC_PHASE_READY; phase++) {
        for (int i = 0; i < lc->component_count; i++) {
            ozayn_lc_component_t *comp = &lc->components[i];
            if (!comp->active) continue;
            if (comp->phase != phase) continue;
            if (comp->state == OZAYN_LC_COMP_INITIALIZED ||
                comp->state == OZAYN_LC_COMP_STARTED ||
                comp->state == OZAYN_LC_COMP_DISABLED) continue;

            lc->init_order[lc->init_order_count++] = i;
        }
    }

    LOG_DEBUG("LIFECYCLE", "Build init order: %d components", lc->init_order_count);
}

/* ---------- Internal: build shutdown order (reverse) ---------- */

static void build_shutdown_order(ozayn_lc_t *lc) {
    lc->shutdown_order_count = 0;

    /* Shutdown in reverse phase order, components that were initialized last go first */
    for (int phase = (int)OZAYN_LC_PHASE_READY - 1; phase >= (int)OZAYN_LC_PHASE_BOOTSTRAP; phase--) {
        for (int i = lc->component_count - 1; i >= 0; i--) {
            ozayn_lc_component_t *comp = &lc->components[i];
            if (!comp->active) continue;
            if ((int)comp->phase != phase) continue;
            if (comp->state != OZAYN_LC_COMP_STARTED &&
                comp->state != OZAYN_LC_COMP_INITIALIZED) continue;

            lc->shutdown_order[lc->shutdown_order_count++] = i;
        }
    }

    LOG_DEBUG("LIFECYCLE", "Build shutdown order: %d components", lc->shutdown_order_count);
}

/* ---------- Init ---------- */

int ozayn_lc_init(ozayn_lc_t *lc, const ozayn_lc_config_t *cfg) {
    if (!lc) return -1;

    memset(lc, 0, sizeof(ozayn_lc_t));

    /* Defaults */
    if (cfg) {
        lc->config = *cfg;
    } else {
        lc->config.startup_timeout_ms   = 30000;
        lc->config.component_timeout_ms = 5000;
        lc->config.shutdown_timeout_ms  = 15000;
    }

    lc->state = OZAYN_LC_STATE_CREATED;
    lc->shutdown_reason = OZAYN_LC_SHUTDOWN_NORMAL;
    lc->current_phase = OZAYN_LC_PHASE_BOOTSTRAP;
    lc->initialized = 1;

    LOG_INFO("LIFECYCLE", "Startup coordinator initialized");
    return 0;
}

/* ---------- Shutdown ---------- */

void ozayn_lc_shutdown(ozayn_lc_t *lc) {
    if (!lc || !lc->initialized) return;

    LOG_INFO("LIFECYCLE", "Startup coordinator shut down");
    lc->initialized = 0;
}

/* ---------- Binding ---------- */

void ozayn_lc_set_runtime(ozayn_lc_t *lc, void *runtime) {
    if (lc) lc->runtime = runtime;
}

void ozayn_lc_set_events(ozayn_lc_t *lc, void *events) {
    if (lc) lc->events = events;
}

void ozayn_lc_set_recovery(ozayn_lc_t *lc, void *recovery) {
    if (lc) lc->recovery = recovery;
}

/* ---------- Component registration ---------- */

int ozayn_lc_register(ozayn_lc_t *lc, const ozayn_lc_component_t *comp) {
    if (!lc || !comp || !comp->name[0]) return -1;
    if (lc->component_count >= OZAYN_LC_MAX_COMPONENTS) {
        LOG_WARN("LIFECYCLE", "Component limit reached — cannot register '%s'", comp->name);
        return -1;
    }

    /* Check for duplicate name */
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].active &&
            strcmp(lc->components[i].name, comp->name) == 0) {
            LOG_WARN("LIFECYCLE", "Component '%s' already registered", comp->name);
            return -1;
        }
    }

    lc->components[lc->component_count] = *comp;
    lc->components[lc->component_count].state = OZAYN_LC_COMP_CREATED;
    lc->component_count++;

    LOG_DEBUG("LIFECYCLE", "Registered component '%s' (phase=%s, criticality=%s)",
              comp->name, ozayn_lc_phase_name(comp->phase),
              ozayn_lc_criticality_name(comp->criticality));
    return 0;
}

int ozayn_lc_register_simple(ozayn_lc_t *lc,
                             const char *name,
                             ozayn_lc_phase_t phase,
                             ozayn_lc_criticality_t criticality,
                             ozayn_lc_init_fn init_fn,
                             void *context) {
    if (!lc || !name) return -1;

    ozayn_lc_component_t comp;
    memset(&comp, 0, sizeof(comp));
    comp.active = 1;
    snprintf(comp.name, sizeof(comp.name), "%s", name);
    comp.phase = phase;
    comp.criticality = criticality;
    comp.state = OZAYN_LC_COMP_CREATED;
    comp.init_fn = init_fn;
    comp.context = context;

    return ozayn_lc_register(lc, &comp);
}

/* ---------- Startup ---------- */

int ozayn_lc_startup(ozayn_lc_t *lc) {
    if (!lc || !lc->initialized) return -1;

    LOG_INFO("LIFECYCLE", "=== OZAYN Core Startup ===");
    lc->startup_time = time(NULL);

    if (transition(lc, OZAYN_LC_STATE_STARTING) != 0) {
        LOG_ERROR("LIFECYCLE", "Cannot start: state is %s", ozayn_lc_state_name(lc->state));
        return -1;
    }

    publish_event(lc, OZAYN_LC_EVENT_INIT_BEGAN, NULL);

    /* Build the initialization order */
    build_init_order(lc);

    /* Iterate through phases */
    int success_count = 0;
    int fail_count = 0;

    for (ozayn_lc_phase_t phase = OZAYN_LC_PHASE_BOOTSTRAP; phase < OZAYN_LC_PHASE_READY; phase++) {
        lc->current_phase = phase;
        LOG_INFO("LIFECYCLE", "[PHASE %d/%d] %s",
                 phase + 1, OZAYN_LC_PHASE_READY, ozayn_lc_phase_name((ozayn_lc_phase_t)phase));

        for (int i = 0; i < lc->init_order_count; i++) {
            int idx = lc->init_order[i];
            ozayn_lc_component_t *comp = &lc->components[idx];

            if (comp->phase != (ozayn_lc_phase_t)phase) continue;
            if (comp->state == OZAYN_LC_COMP_INITIALIZED ||
                comp->state == OZAYN_LC_COMP_STARTED ||
                comp->state == OZAYN_LC_COMP_DISABLED) continue;

            LOG_INFO("LIFECYCLE", "  Initializing '%s'...", comp->name);
            comp->state = OZAYN_LC_COMP_INITIALIZING;
            comp->init_time = time(NULL);

            /* Call init function if provided */
            int result = 0;
            if (comp->init_fn) {
                result = comp->init_fn(comp->context);
            }

            comp->init_result = result;

            if (result == 0) {
                comp->state = OZAYN_LC_COMP_INITIALIZED;
                LOG_INFO("LIFECYCLE", "  '%s' initialized [OK]", comp->name);
                success_count++;
            } else {
                comp->state = OZAYN_LC_COMP_FAILED;
                fail_count++;
                LOG_WARN("LIFECYCLE", "  '%s' initialization FAILED (result=%d)", comp->name, result);

                publish_event(lc, OZAYN_LC_EVENT_COMPONENT_FAILED, NULL);

                /* Handle based on criticality */
                if (comp->criticality == OZAYN_LC_CRITICALITY_REQUIRED) {
                    LOG_ERROR("LIFECYCLE", "CRITICAL component '%s' failed — aborting startup",
                              comp->name);

                    /* Rollback already-initialized components in this phase and before */
                    LOG_INFO("LIFECYCLE", "=== Startup Rollback ===");
                    publish_event(lc, OZAYN_LC_EVENT_STARTUP_ROLLBACK, NULL);

                    /* Stop components started in current phase */
                    for (int j = i - 1; j >= 0; j--) {
                        int rollback_idx = lc->init_order[j];
                        ozayn_lc_component_t *rb = &lc->components[rollback_idx];
                        if (rb->phase == phase &&
                            (rb->state == OZAYN_LC_COMP_INITIALIZED ||
                             rb->state == OZAYN_LC_COMP_STARTED)) {
                            LOG_INFO("LIFECYCLE", "  Rolling back '%s'", rb->name);
                            if (rb->stop_fn) rb->stop_fn(rb->context);
                            rb->state = OZAYN_LC_COMP_STOPPED;
                            rb->stop_time = time(NULL);
                        }
                    }

                    /* Shutdown earlier phases */
                    for (int prev_phase = (int)phase - 1; prev_phase >= (int)OZAYN_LC_PHASE_BOOTSTRAP; prev_phase--) {
                        for (int p = 0; p < lc->component_count; p++) {
                            ozayn_lc_component_t *rb = &lc->components[p];
                            if (rb->active && rb->phase == (ozayn_lc_phase_t)prev_phase &&
                                (rb->state == OZAYN_LC_COMP_INITIALIZED ||
                                 rb->state == OZAYN_LC_COMP_STARTED)) {
                                LOG_INFO("LIFECYCLE", "  Shutting down '%s' (phase %d)", rb->name, prev_phase);
                                if (rb->shutdown_fn) rb->shutdown_fn(rb->context);
                                rb->state = OZAYN_LC_COMP_STOPPED;
                                rb->stop_time = time(NULL);
                            }
                        }
                    }

                    transition(lc, OZAYN_LC_STATE_FAILED);
                    lc->shutdown_reason = OZAYN_LC_SHUTDOWN_STARTUP_FAIL;
                    lc->components_failed = fail_count;
                    return -1;
                } else if (comp->criticality == OZAYN_LC_CRITICALITY_OPTIONAL) {
                    LOG_INFO("LIFECYCLE", "  Optional component '%s' disabled — continuing", comp->name);
                    comp->state = OZAYN_LC_COMP_DISABLED;
                    lc->components_disabled++;
                } else {
                    LOG_INFO("LIFECYCLE", "  Best-effort component '%s' failed — ignoring", comp->name);
                }
            }
        }

        LOG_INFO("LIFECYCLE", "[PHASE %d/%d] %s complete",
                 phase + 1, OZAYN_LC_PHASE_READY, ozayn_lc_phase_name((ozayn_lc_phase_t)phase));
        publish_event(lc, OZAYN_LC_EVENT_INIT_PHASE_COMPLETE, NULL);
    }

    /* Start initialized components */
    LOG_INFO("LIFECYCLE", "=== Starting Components ===");
    for (int i = 0; i < lc->component_count; i++) {
        ozayn_lc_component_t *comp = &lc->components[i];
        if (!comp->active) continue;
        if (comp->state != OZAYN_LC_COMP_INITIALIZED) continue;

        comp->state = OZAYN_LC_COMP_STARTING;
        comp->start_time = time(NULL);

        int result = 0;
        if (comp->start_fn) {
            result = comp->start_fn(comp->context);
        }

        if (result == 0) {
            comp->state = OZAYN_LC_COMP_STARTED;
            LOG_DEBUG("LIFECYCLE", "  '%s' started", comp->name);
        } else {
            comp->state = OZAYN_LC_COMP_FAILED;
            LOG_WARN("LIFECYCLE", "  '%s' start FAILED", comp->name);

            if (comp->criticality == OZAYN_LC_CRITICALITY_REQUIRED) {
                LOG_ERROR("LIFECYCLE", "Required component '%s' failed to start", comp->name);
                transition(lc, OZAYN_LC_STATE_FAILED);
                lc->shutdown_reason = OZAYN_LC_SHUTDOWN_STARTUP_FAIL;
                return -1;
            }
        }
    }

    /* Readiness check */
    int ready = ozayn_lc_readiness_check(lc);
    if (ready) {
        publish_event(lc, OZAYN_LC_EVENT_READINESS_PASSED, NULL);
    } else {
        publish_event(lc, OZAYN_LC_EVENT_READINESS_FAILED, NULL);
        LOG_WARN("LIFECYCLE", "Readiness check failed — but continuing");
    }

    /* Mark READY */
    lc->current_phase = OZAYN_LC_PHASE_READY;
    lc->ready_time = time(NULL);
    lc->components_initialized = success_count;
    lc->components_started = 0;
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].state == OZAYN_LC_COMP_STARTED) lc->components_started++;
    }

    publish_event(lc, OZAYN_LC_EVENT_INIT_COMPLETE, NULL);
    publish_event(lc, OZAYN_LC_EVENT_ONLINE, NULL);

    if (transition(lc, OZAYN_LC_STATE_RUNNING) != 0) {
        LOG_ERROR("LIFECYCLE", "Failed to transition to RUNNING");
        return -1;
    }

    LOG_INFO("LIFECYCLE", "=== OZAYN CORE ONLINE ===");
    LOG_INFO("LIFECYCLE", "  Components: %d registered, %d initialized, %d started, %d disabled, %d failed",
             lc->component_count, success_count, lc->components_started,
             lc->components_disabled, fail_count);

    return 0;
}

/* ---------- Shutdown request ---------- */

int ozayn_lc_request_shutdown(ozayn_lc_t *lc, ozayn_lc_shutdown_reason_t reason) {
    if (!lc) return -1;

    if (lc->state == OZAYN_LC_STATE_STOPPING) {
        LOG_WARN("LIFECYCLE", "Shutdown already in progress — ignoring duplicate request");
        return 0;
    }

    if (lc->state == OZAYN_LC_STATE_STOPPED) {
        LOG_WARN("LIFECYCLE", "Already stopped — ignoring shutdown request");
        return 0;
    }

    lc->shutdown_reason = reason;
    LOG_INFO("LIFECYCLE", "Shutdown requested: %s", ozayn_lc_shutdown_reason_name(reason));

    publish_event(lc, OZAYN_LC_EVENT_SHUTDOWN_REQUESTED, NULL);

    if (lc->state == OZAYN_LC_STATE_RUNNING) {
        return ozayn_lc_perform_shutdown(lc);
    }

    if (lc->state == OZAYN_LC_STATE_STARTING) {
        LOG_INFO("LIFECYCLE", "Shutdown during startup — transitioning to STOPPING");
        transition(lc, OZAYN_LC_STATE_STOPPING);
        return ozayn_lc_perform_shutdown(lc);
    }

    return 0;
}

/* ---------- Perform shutdown ---------- */

int ozayn_lc_perform_shutdown(ozayn_lc_t *lc) {
    if (!lc) return -1;

    lc->shutdown_time = time(NULL);
    LOG_INFO("LIFECYCLE", "=== OZAYN Core Shutdown ===");
    LOG_INFO("LIFECYCLE", "Reason: %s", ozayn_lc_shutdown_reason_name(lc->shutdown_reason));

    if (transition(lc, OZAYN_LC_STATE_STOPPING) != 0) {
        /* Already stopping or failed — force through */
        if (lc->state != OZAYN_LC_STATE_STOPPING) {
            lc->state = OZAYN_LC_STATE_STOPPING;
        }
    }

    publish_event(lc, OZAYN_LC_EVENT_SHUTDOWN_BEGAN, NULL);

    /* Build shutdown order (reverse) */
    build_shutdown_order(lc);

    /* Stop components in reverse order */
    int stopped_count = 0;
    for (int i = 0; i < lc->shutdown_order_count; i++) {
        int idx = lc->shutdown_order[i];
        ozayn_lc_component_t *comp = &lc->components[idx];

        if (comp->state == OZAYN_LC_COMP_STOPPED ||
            comp->state == OZAYN_LC_COMP_DISABLED ||
            comp->state == OZAYN_LC_COMP_FAILED) continue;

        LOG_INFO("LIFECYCLE", "  Stopping '%s' (phase=%s)...",
                 comp->name, ozayn_lc_phase_name(comp->phase));
        comp->state = OZAYN_LC_COMP_STOPPING;

        if (comp->stop_fn) comp->stop_fn(comp->context);

        comp->state = OZAYN_LC_COMP_STOPPED;
        comp->stop_time = time(NULL);
        stopped_count++;
    }

    /* Shutdown components (release resources) */
    for (int i = 0; i < lc->component_count; i++) {
        ozayn_lc_component_t *comp = &lc->components[i];
        if (!comp->active) continue;
        if (comp->state == OZAYN_LC_COMP_DISABLED) continue;

        if (comp->shutdown_fn) {
            comp->shutdown_fn(comp->context);
        }
    }

    lc->stopped_time = time(NULL);
    lc->components_started = 0;

    publish_event(lc, OZAYN_LC_EVENT_SHUTDOWN_COMPLETED, NULL);

    if (transition(lc, OZAYN_LC_STATE_STOPPED) != 0) {
        lc->state = OZAYN_LC_STATE_STOPPED;
    }

    LOG_INFO("LIFECYCLE", "=== Shutdown Complete (%d components stopped) ===", stopped_count);
    return 0;
}

/* ---------- Restart ---------- */

int ozayn_lc_restart(ozayn_lc_t *lc) {
    if (!lc) return -1;

    LOG_INFO("LIFECYCLE", "=== Controlled Restart ===");
    publish_event(lc, OZAYN_LC_EVENT_RESTART_REQUESTED, NULL);

    lc->shutdown_reason = OZAYN_LC_SHUTDOWN_RESTART;

    /* Shutdown */
    if (lc->state == OZAYN_LC_STATE_RUNNING || lc->state == OZAYN_LC_STATE_STARTING) {
        ozayn_lc_perform_shutdown(lc);
    }

    /* Reset component states */
    for (int i = 0; i < lc->component_count; i++) {
        lc->components[i].state = OZAYN_LC_COMP_CREATED;
        lc->components[i].init_result = 0;
    }

    /* Reset statistics */
    lc->components_initialized = 0;
    lc->components_started = 0;
    lc->components_failed = 0;
    lc->components_disabled = 0;
    lc->startup_retries++;
    lc->init_order_count = 0;
    lc->shutdown_order_count = 0;

    /* Re-transition to CREATED then startup */
    transition(lc, OZAYN_LC_STATE_CREATED);
    return ozayn_lc_startup(lc);
}

/* ---------- Query ---------- */

ozayn_lc_phase_t ozayn_lc_get_phase(const ozayn_lc_t *lc) {
    if (!lc) return OZAYN_LC_PHASE_BOOTSTRAP;
    return lc->current_phase;
}

ozayn_lc_state_t ozayn_lc_get_state(const ozayn_lc_t *lc) {
    if (!lc) return OZAYN_LC_STATE_CREATED;
    return lc->state;
}

int ozayn_lc_is_running(const ozayn_lc_t *lc) {
    if (!lc) return 0;
    return lc->state == OZAYN_LC_STATE_RUNNING;
}

ozayn_lc_shutdown_reason_t ozayn_lc_get_shutdown_reason(const ozayn_lc_t *lc) {
    if (!lc) return OZAYN_LC_SHUTDOWN_NORMAL;
    return lc->shutdown_reason;
}

const ozayn_lc_component_t *ozayn_lc_get_component(const ozayn_lc_t *lc, const char *name) {
    if (!lc || !name) return NULL;
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].active && strcmp(lc->components[i].name, name) == 0) {
            return &lc->components[i];
        }
    }
    return NULL;
}

const ozayn_lc_component_t *ozayn_lc_get_component_by_index(const ozayn_lc_t *lc, int index) {
    if (!lc || index < 0 || index >= lc->component_count) return NULL;
    if (!lc->components[index].active) return NULL;
    return &lc->components[index];
}

int ozayn_lc_component_count(const ozayn_lc_t *lc) {
    if (!lc) return 0;
    return lc->component_count;
}

int ozayn_lc_component_count_by_phase(const ozayn_lc_t *lc, ozayn_lc_phase_t phase) {
    if (!lc) return 0;
    int count = 0;
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].active && lc->components[i].phase == phase) count++;
    }
    return count;
}

int ozayn_lc_component_count_by_state(const ozayn_lc_t *lc, ozayn_lc_component_state_t state) {
    if (!lc) return 0;
    int count = 0;
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].active && lc->components[i].state == state) count++;
    }
    return count;
}

int ozayn_lc_component_count_by_criticality(const ozayn_lc_t *lc, ozayn_lc_criticality_t crit) {
    if (!lc) return 0;
    int count = 0;
    for (int i = 0; i < lc->component_count; i++) {
        if (lc->components[i].active && lc->components[i].criticality == crit) count++;
    }
    return count;
}

/* ---------- Readiness check ---------- */

int ozayn_lc_readiness_check(ozayn_lc_t *lc) {
    if (!lc) return 0;

    LOG_INFO("LIFECYCLE", "=== Readiness Check ===");

    int all_ready = 1;
    int checked = 0;

    for (int i = 0; i < lc->component_count; i++) {
        ozayn_lc_component_t *comp = &lc->components[i];
        if (!comp->active) continue;
        if (comp->criticality != OZAYN_LC_CRITICALITY_REQUIRED) continue;

        checked++;
        int ready = (comp->state == OZAYN_LC_COMP_STARTED ||
                     comp->state == OZAYN_LC_COMP_INITIALIZED);

        LOG_INFO("LIFECYCLE", "  %-30s %s", comp->name, ready ? "[OK]" : "[NOT READY]");

        if (!ready) all_ready = 0;
    }

    LOG_INFO("LIFECYCLE", "Readiness: %s (%d required components checked)",
             all_ready ? "PASS" : "FAIL", checked);

    return all_ready;
}

/* ---------- Print ---------- */

void ozayn_lc_print_status(const ozayn_lc_t *lc) {
    if (!lc) return;

    LOG_INFO("LIFECYCLE", "=== OZAYN Core Lifecycle ===");
    LOG_INFO("LIFECYCLE", "  State:   %s", ozayn_lc_state_name(lc->state));
    LOG_INFO("LIFECYCLE", "  Phase:   %s", ozayn_lc_phase_name(lc->current_phase));
    LOG_INFO("LIFECYCLE", "  Reason:  %s", ozayn_lc_shutdown_reason_name(lc->shutdown_reason));
    LOG_INFO("LIFECYCLE", "  Components: %d total", lc->component_count);
    LOG_INFO("LIFECYCLE", "  Initialized: %d", lc->components_initialized);
    LOG_INFO("LIFECYCLE", "  Started:     %d", lc->components_started);
    LOG_INFO("LIFECYCLE", "  Disabled:    %d", lc->components_disabled);
    LOG_INFO("LIFECYCLE", "  Failed:      %d", lc->components_failed);
    LOG_INFO("LIFECYCLE", "  Retries:     %d", lc->startup_retries);

    if (lc->startup_time) {
        LOG_INFO("LIFECYCLE", "  Startup:  %s", ctime(&lc->startup_time));
    }
    if (lc->ready_time) {
        LOG_INFO("LIFECYCLE", "  Ready:    %s", ctime(&lc->ready_time));
    }
    if (lc->shutdown_time) {
        LOG_INFO("LIFECYCLE", "  Shutdown: %s", ctime(&lc->shutdown_time));
    }
    if (lc->stopped_time) {
        LOG_INFO("LIFECYCLE,  Stopped:  %s", ctime(&lc->stopped_time));
    }
}

void ozayn_lc_print_components(const ozayn_lc_t *lc) {
    if (!lc) return;

    LOG_INFO("LIFECYCLE", "=== Registered Components (%d) ===", lc->component_count);

    for (ozayn_lc_phase_t phase = OZAYN_LC_PHASE_BOOTSTRAP; phase < OZAYN_LC_PHASE_READY; phase++) {
        int found = 0;
        for (int i = 0; i < lc->component_count; i++) {
            if (lc->components[i].active && lc->components[i].phase == phase) {
                if (!found) {
                    LOG_INFO("LIFECYCLE", "  [%s]", ozayn_lc_phase_name(phase));
                    found = 1;
                }
                const ozayn_lc_component_t *c = &lc->components[i];
                LOG_INFO("LIFECYCLE", "    %-30s %-12s %-10s %s",
                         c->name,
                         ozayn_lc_criticality_name(c->criticality),
                         ozayn_lc_component_state_name(c->state),
                         c->init_result == 0 ? "" : "FAIL");
            }
        }
    }
}

void ozayn_lc_print_startup_log(const ozayn_lc_t *lc) {
    if (!lc) return;

    LOG_INFO("LIFECYCLE", "=== Startup Log ===");

    for (int i = 0; i < lc->component_count; i++) {
        const ozayn_lc_component_t *c = &lc->components[i];
        if (!c->active) continue;

        const char *status;
        switch (c->state) {
            case OZAYN_LC_COMP_STARTED:     status = "STARTED"; break;
            case OZAYN_LC_COMP_INITIALIZED: status = "INITIALIZED"; break;
            case OZAYN_LC_COMP_DISABLED:    status = "DISABLED"; break;
            case OZAYN_LC_COMP_FAILED:      status = "FAILED"; break;
            default:                        status = "NOT STARTED"; break;
        }

        LOG_INFO("LIFECYCLE", "  %-30s -> %s", c->name, status);
    }
}
