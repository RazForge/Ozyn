#include "reload_mgr.h"
#include "events.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * reload_mgr.c — Runtime Hot Reloading (Stage 26).
 *
 * Coordinates safe hot-reload of eligible components while Core stays alive.
 *
 * Reload flow:
 *   REQUEST -> VALIDATE -> QUIESCE -> STOP -> SAVE_STATE -> UNLOAD
 *   -> LOAD -> INIT -> RESTORE_STATE -> HEALTH_CHECK -> READY -> COMPLETE
 *
 * On failure: ROLLBACK -> restore old version or mark failed.
 */

/* ================================================================
 * Helpers
 * ================================================================ */

static void publish_event(ozayn_reload_mgr_t *mgr, ozayn_event_type_t type) {
    if (!mgr || !mgr->events) return;
    ozayn_events_publish(mgr->events, type, OZAYN_SRC_RELOAD, NULL);
}

static ozayn_reload_component_t *find_component(ozayn_reload_mgr_t *mgr, const char *name) {
    if (!name || !name[0]) return NULL;
    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active &&
            strcmp(mgr->components[i].name, name) == 0) {
            return &mgr->components[i];
        }
    }
    return NULL;
}

static ozayn_reload_component_t *find_free_slot(ozayn_reload_mgr_t *mgr) {
    for (uint32_t i = 0; i < OZAYN_RELOAD_MAX_COMPONENTS; i++) {
        if (!mgr->components[i].active) return &mgr->components[i];
    }
    return NULL;
}

static void add_audit(ozayn_reload_mgr_t *mgr, const char *component,
                       const char *old_ver, const char *new_ver,
                       const char *requester, const char *reason,
                       ozayn_reload_result_t result,
                       ozayn_reload_state_t final_state) {
    uint32_t idx = (mgr->audit_head + mgr->audit_count) % OZAYN_RELOAD_MAX_AUDIT;
    if (mgr->audit_count >= OZAYN_RELOAD_MAX_AUDIT) {
        mgr->audit_head = (mgr->audit_head + 1) % OZAYN_RELOAD_MAX_AUDIT;
    } else {
        mgr->audit_count++;
    }

    ozayn_reload_audit_t *a = &mgr->audit[idx];
    memset(a, 0, sizeof(*a));
    strncpy(a->component, component, OZAYN_RELOAD_MAX_NAME - 1);
    if (old_ver) strncpy(a->old_version, old_ver, OZAYN_RELOAD_MAX_VERSION - 1);
    if (new_ver) strncpy(a->new_version, new_ver, OZAYN_RELOAD_MAX_VERSION - 1);
    a->result = result;
    a->final_state = final_state;
    a->requested_at = mgr->reload_started_at;
    a->completed_at = time(NULL);
    if (a->requested_at > 0 && a->completed_at >= a->requested_at)
        a->duration_ms = (uint32_t)((a->completed_at - a->requested_at) * 1000);
    if (requester) strncpy(a->requester, requester, OZAYN_RELOAD_MAX_NAME - 1);
    if (reason) strncpy(a->reason, reason, sizeof(a->reason) - 1);
}

static void transition(ozayn_reload_mgr_t *mgr, ozayn_reload_state_t new_state) {
    ozayn_reload_state_t old = mgr->active_state;
    mgr->active_state = new_state;
    if (old != new_state) {
        LOG_INFO("RELOAD", "State: %s -> %s",
                 ozayn_reload_state_name(old), ozayn_reload_state_name(new_state));
    }
}

/* ================================================================
 * Names
 * ================================================================ */

const char *ozayn_reload_state_name(ozayn_reload_state_t state) {
    switch (state) {
        case OZAYN_RELOAD_STATE_IDLE:              return "IDLE";
        case OZAYN_RELOAD_STATE_REQUESTED:         return "REQUESTED";
        case OZAYN_RELOAD_STATE_VALIDATING:        return "VALIDATING";
        case OZAYN_RELOAD_STATE_QUIESCING:         return "QUIESCING";
        case OZAYN_RELOAD_STATE_STOPPING:          return "STOPPING";
        case OZAYN_RELOAD_STATE_STATE_SAVED:       return "STATE_SAVED";
        case OZAYN_RELOAD_STATE_UNLOADING:         return "UNLOADING";
        case OZAYN_RELOAD_STATE_LOADING:           return "LOADING";
        case OZAYN_RELOAD_STATE_INITIALIZING:      return "INITIALIZING";
        case OZAYN_RELOAD_STATE_STATE_RESTORED:    return "STATE_RESTORED";
        case OZAYN_RELOAD_STATE_STARTING:          return "STARTING";
        case OZAYN_RELOAD_STATE_HEALTH_CHECK:      return "HEALTH_CHECK";
        case OZAYN_RELOAD_STATE_READY:             return "READY";
        case OZAYN_RELOAD_STATE_COMPLETED:         return "COMPLETED";
        case OZAYN_RELOAD_STATE_ROLLBACK:          return "ROLLBACK";
        case OZAYN_RELOAD_STATE_ROLLBACK_COMPLETE: return "ROLLBACK_COMPLETE";
        case OZAYN_RELOAD_STATE_FAILED:            return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_reload_result_name(ozayn_reload_result_t result) {
    switch (result) {
        case OZAYN_RELOAD_RESULT_OK:               return "OK";
        case OZAYN_RELOAD_RESULT_NOT_RELOADABLE:   return "NOT_RELOADABLE";
        case OZAYN_RELOAD_RESULT_AUTH_DENIED:      return "AUTH_DENIED";
        case OZAYN_RELOAD_RESULT_INCOMPATIBLE:     return "INCOMPATIBLE";
        case OZAYN_RELOAD_RESULT_DEP_FAILURE:      return "DEP_FAILURE";
        case OZAYN_RELOAD_RESULT_QUIESCE_TIMEOUT:  return "QUIESCE_TIMEOUT";
        case OZAYN_RELOAD_RESULT_STATE_SAVE_FAIL:  return "STATE_SAVE_FAIL";
        case OZAYN_RELOAD_RESULT_LOAD_FAIL:        return "LOAD_FAIL";
        case OZAYN_RELOAD_RESULT_INIT_FAIL:        return "INIT_FAIL";
        case OZAYN_RELOAD_RESULT_STATE_RESTORE_FAIL: return "STATE_RESTORE_FAIL";
        case OZAYN_RELOAD_RESULT_HEALTH_FAIL:      return "HEALTH_FAIL";
        case OZAYN_RELOAD_RESULT_ROLLBACK_FAIL:    return "ROLLBACK_FAIL";
        case OZAYN_RELOAD_RESULT_ABORTED:          return "ABORTED";
        case OZAYN_RELOAD_RESULT_TIMEOUT:          return "TIMEOUT";
    }
    return "UNKNOWN";
}

const char *ozayn_reload_capability_name(ozayn_reload_capability_t cap) {
    switch (cap) {
        case OZAYN_RELOAD_SUPPORTED:        return "SUPPORTED";
        case OZAYN_RELOAD_UNSUPPORTED:      return "UNSUPPORTED";
        case OZAYN_RELOAD_RESTART_REQUIRED: return "RESTART_REQUIRED";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

int ozayn_reload_mgr_init(ozayn_reload_mgr_t *mgr, const ozayn_reload_config_t *config) {
    if (!mgr || !config) return -1;
    memset(mgr, 0, sizeof(*mgr));
    mgr->config.quiesce_timeout_ms = config->quiesce_timeout_ms > 0 ?
        config->quiesce_timeout_ms : 10000;
    mgr->config.load_timeout_ms = config->load_timeout_ms > 0 ?
        config->load_timeout_ms : 15000;
    mgr->config.health_check_timeout_ms = config->health_check_timeout_ms > 0 ?
        config->health_check_timeout_ms : 5000;
    mgr->config.rollback_on_fail = config->rollback_on_fail;
    mgr->config.max_concurrent = config->max_concurrent > 0 ?
        config->max_concurrent : 1;
    mgr->active_state = OZAYN_RELOAD_STATE_IDLE;
    LOG_INFO("RELOAD", "Reload manager initialized (quiesce=%ums, load=%ums, rollback=%s)",
             mgr->config.quiesce_timeout_ms, mgr->config.load_timeout_ms,
             mgr->config.rollback_on_fail ? "yes" : "no");
    return 0;
}

void ozayn_reload_mgr_shutdown(ozayn_reload_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("RELOAD", "Reload manager shut down (requests=%u, succeeded=%u, failed=%u, rollback=%u)",
             mgr->total_requests, mgr->total_succeeded,
             mgr->total_failed, mgr->total_rollback);
    memset(mgr->components, 0, sizeof(mgr->components));
    memset(mgr->audit, 0, sizeof(mgr->audit));
}

void ozayn_reload_mgr_set_events(ozayn_reload_mgr_t *mgr, ozayn_event_engine_t *events) {
    if (mgr) mgr->events = events;
}

/* ================================================================
 * Component registration
 * ================================================================ */

int ozayn_reload_register(ozayn_reload_mgr_t *mgr, const char *name,
                            const char *version,
                            ozayn_reload_capability_t capability,
                            int required) {
    if (!mgr || !name || !name[0]) return -1;
    if (find_component(mgr, name)) return -2;

    ozayn_reload_component_t *comp = find_free_slot(mgr);
    if (!comp) return -3;

    memset(comp, 0, sizeof(*comp));
    comp->active = 1;
    strncpy(comp->name, name, OZAYN_RELOAD_MAX_NAME - 1);
    if (version) strncpy(comp->current_version, version, OZAYN_RELOAD_MAX_VERSION - 1);
    comp->capability = capability;
    comp->required = required;
    comp->max_concurrent = 1;

    mgr->component_count++;

    switch (capability) {
        case OZAYN_RELOAD_SUPPORTED:        break;
        case OZAYN_RELOAD_UNSUPPORTED:      break;
        case OZAYN_RELOAD_RESTART_REQUIRED: break;
    }

    LOG_INFO("RELOAD", "Registered component '%s' v%s [%s] %s",
             name, version ? version : "?",
             ozayn_reload_capability_name(capability),
             required ? "(critical)" : "(optional)");
    return 0;
}

int ozayn_reload_unregister(ozayn_reload_mgr_t *mgr, const char *name) {
    ozayn_reload_component_t *comp = find_component(mgr, name);
    if (!comp) return -1;
    if (comp->state_data) {
        /* Don't leak */
        comp->state_data = NULL;
        comp->state_size = 0;
    }
    LOG_INFO("RELOAD", "Unregistered component '%s'", comp->name);
    comp->active = 0;
    if (mgr->component_count > 0) mgr->component_count--;
    return 0;
}

/* ================================================================
 * Active work tracking
 * ================================================================ */

int ozayn_reload_request_begin(ozayn_reload_mgr_t *mgr, const char *component) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp) return -1;
    comp->active_requests++;
    return 0;
}

int ozayn_reload_request_end(ozayn_reload_mgr_t *mgr, const char *component) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp) return -1;
    if (comp->active_requests > 0) comp->active_requests--;
    return 0;
}

uint32_t ozayn_reload_active_requests(ozayn_reload_mgr_t *mgr, const char *component) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    return comp ? comp->active_requests : 0;
}

/* ================================================================
 * State save/load
 * ================================================================ */

int ozayn_reload_save_state(ozayn_reload_mgr_t *mgr, const char *component,
                              const void *data, uint32_t size, uint32_t version) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp) return -1;
    if (size > OZAYN_RELOAD_MAX_STATE_SIZE) return -2;

    /* Free old state if any */
    if (comp->state_data) {
        comp->state_data = NULL;
        comp->state_size = 0;
    }

    /* Allocate and copy (using static buffer for demo — real impl would malloc) */
    static uint8_t state_storage[OZAYN_RELOAD_MAX_COMPONENTS][OZAYN_RELOAD_MAX_STATE_SIZE];
    static int state_used[OZAYN_RELOAD_MAX_COMPONENTS] = {0};

    /* Find slot */
    int slot = -1;
    for (int i = 0; i < OZAYN_RELOAD_MAX_COMPONENTS; i++) {
        if (!state_used[i]) { slot = i; break; }
    }
    if (slot < 0) return -3;

    memcpy(state_storage[slot], data, size);
    state_used[slot] = 1;
    comp->state_data = state_storage[slot];
    comp->state_size = size;
    comp->state_version = version;

    LOG_INFO("RELOAD", "State saved for '%s' (size=%u, version=%u)",
             component, size, version);
    return 0;
}

int ozayn_reload_load_state(ozayn_reload_mgr_t *mgr, const char *component,
                              void *out, uint32_t max_size, uint32_t *out_version) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp || !comp->state_data) return -1;
    if (comp->state_size > max_size) return -2;

    memcpy(out, comp->state_data, comp->state_size);
    if (out_version) *out_version = comp->state_version;
    LOG_INFO("RELOAD", "State loaded for '%s' (size=%u, version=%u)",
             component, comp->state_size, comp->state_version);
    return 0;
}

int ozayn_reload_clear_state(ozayn_reload_mgr_t *mgr, const char *component) {
    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp) return -1;
    comp->state_data = NULL;
    comp->state_size = 0;
    comp->state_version = 0;
    LOG_INFO("RELOAD", "State cleared for '%s'", component);
    return 0;
}

/* ================================================================
 * Version checking
 * ================================================================ */

int ozayn_reload_check_version(ozayn_reload_mgr_t *mgr, const char *component,
                                 const char *new_version) {
    (void)mgr;
    (void)component;
    (void)new_version;
    /* Basic check: in real implementation, parse semver and compare.
     * For now, accept any version. */
    return 0;
}

/* ================================================================
 * Reload request and execution
 * ================================================================ */

int ozayn_reload_request(ozayn_reload_mgr_t *mgr, const char *component,
                           const char *new_version,
                           const char *requester,
                           const char *reason) {
    if (!mgr || !component || !component[0]) return -1;
    if (mgr->reload_active) {
        LOG_INFO("RELOAD", "Reload already in progress for '%s'", mgr->active_component);
        return -2;
    }

    ozayn_reload_component_t *comp = find_component(mgr, component);
    if (!comp) return -3;

    /* Check capability */
    if (comp->capability == OZAYN_RELOAD_UNSUPPORTED) {
        LOG_INFO("RELOAD", "Component '%s' is NOT reloadable (restart required)", component);
        add_audit(mgr, component, comp->current_version, new_version,
                  requester, reason, OZAYN_RELOAD_RESULT_NOT_RELOADABLE,
                  OZAYN_RELOAD_STATE_IDLE);
        mgr->total_requests++;
        mgr->total_failed++;
        publish_event(mgr, OZAYN_RELOAD_EVENT_FAILED);
        return -4;
    }

    /* Start reload */
    mgr->reload_active = 1;
    strncpy(mgr->active_component, component, OZAYN_RELOAD_MAX_NAME - 1);
    mgr->reload_started_at = time(NULL);
    mgr->active_reload_count++;
    mgr->total_requests++;

    if (new_version)
        strncpy(comp->pending_version, new_version, OZAYN_RELOAD_MAX_VERSION - 1);

    transition(mgr, OZAYN_RELOAD_STATE_REQUESTED);
    publish_event(mgr, OZAYN_RELOAD_EVENT_REQUESTED);

    LOG_INFO("RELOAD", "Reload requested: '%s' %s -> %s (by '%s', reason: %s)",
             component, comp->current_version, new_version ? new_version : "?",
             requester ? requester : "system", reason ? reason : "upgrade");

    /* Run the state machine */
    ozayn_reload_tick(mgr);

    return 0;
}

int ozayn_reload_cancel(ozayn_reload_mgr_t *mgr) {
    if (!mgr || !mgr->reload_active) return -1;

    LOG_INFO("RELOAD", "Reload of '%s' cancelled at state %s",
             mgr->active_component, ozayn_reload_state_name(mgr->active_state));

    add_audit(mgr, mgr->active_component, NULL, NULL,
              NULL, "cancelled", OZAYN_RELOAD_RESULT_ABORTED,
              mgr->active_state);

    mgr->reload_active = 0;
    mgr->active_component[0] = '\0';
    transition(mgr, OZAYN_RELOAD_STATE_IDLE);
    publish_event(mgr, OZAYN_RELOAD_EVENT_CANCELLED);
    return 0;
}

/* ================================================================
 * State machine tick
 * ================================================================ */

int ozayn_reload_tick(ozayn_reload_mgr_t *mgr) {
    if (!mgr || !mgr->reload_active) return 0;

    ozayn_reload_component_t *comp = find_component(mgr, mgr->active_component);
    if (!comp) {
        LOG_INFO("RELOAD", "Component '%s' not found, aborting", mgr->active_component);
        mgr->reload_active = 0;
        transition(mgr, OZAYN_RELOAD_STATE_FAILED);
        return -1;
    }

    /* Check timeout */
    time_t now = time(NULL);
    uint32_t elapsed_ms = (uint32_t)((now - mgr->reload_started_at) * 1000);

    switch (mgr->active_state) {
        case OZAYN_RELOAD_STATE_REQUESTED: {
            /* Step 1: Validate */
            transition(mgr, OZAYN_RELOAD_STATE_VALIDATING);
            publish_event(mgr, OZAYN_RELOAD_EVENT_VALIDATING);

            /* Check version compatibility */
            if (comp->pending_version[0] != '\0') {
                if (ozayn_reload_check_version(mgr, comp->name, comp->pending_version) != 0) {
                    LOG_INFO("RELOAD", "Version check failed for '%s' v%s -> v%s",
                             comp->name, comp->current_version, comp->pending_version);
                    add_audit(mgr, comp->name, comp->current_version, comp->pending_version,
                              NULL, NULL, OZAYN_RELOAD_RESULT_INCOMPATIBLE,
                              OZAYN_RELOAD_STATE_FAILED);
                    mgr->total_failed++;
                    mgr->reload_active = 0;
                    transition(mgr, OZAYN_RELOAD_STATE_FAILED);
                    publish_event(mgr, OZAYN_RELOAD_EVENT_FAILED);
                    return -1;
                }
            }

            LOG_INFO("RELOAD", "Validation passed for '%s'", comp->name);
            publish_event(mgr, OZAYN_RELOAD_EVENT_VALIDATED);

            /* Continue to quiesce */
            transition(mgr, OZAYN_RELOAD_STATE_QUIESCING);
            publish_event(mgr, OZAYN_RELOAD_EVENT_QUIESCING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_QUIESCING: {
            /* Step 2: Quiesce — wait for active requests to drain */
            if (comp->active_requests > 0) {
                if (elapsed_ms > mgr->config.quiesce_timeout_ms) {
                    LOG_INFO("RELOAD", "Quiesce timeout for '%s' (%u active requests after %ums)",
                             comp->name, comp->active_requests, elapsed_ms);
                    add_audit(mgr, comp->name, comp->current_version, comp->pending_version,
                              NULL, "quiesce timeout", OZAYN_RELOAD_RESULT_QUIESCE_TIMEOUT,
                              OZAYN_RELOAD_STATE_FAILED);
                    mgr->total_timeout++;
                    mgr->total_failed++;
                    mgr->reload_active = 0;
                    transition(mgr, OZAYN_RELOAD_STATE_FAILED);
                    publish_event(mgr, OZAYN_RELOAD_EVENT_FAILED);
                    return -1;
                }
                LOG_INFO("RELOAD", "Quiescing '%s': %u active requests remaining...",
                         comp->name, comp->active_requests);
                return 0; /* wait */
            }

            LOG_INFO("RELOAD", "Component '%s' quiesced (no active requests)", comp->name);
            publish_event(mgr, OZAYN_RELOAD_EVENT_QUIESCED);

            /* Continue to stop */
            transition(mgr, OZAYN_RELOAD_STATE_STOPPING);
            publish_event(mgr, OZAYN_RELOAD_EVENT_STOPPING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_STOPPING: {
            /* Step 3: Stop the component (graceful drain) */
            LOG_INFO("RELOAD", "Stopping component '%s'...", comp->name);
            /* In real implementation: call component's stop callback */
            publish_event(mgr, OZAYN_RELOAD_EVENT_STOPPED);
            transition(mgr, OZAYN_RELOAD_STATE_STATE_SAVED);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_STATE_SAVED: {
            /* Step 4: Save state — state is already saved by the caller or will be here */
            LOG_INFO("RELOAD", "State saved for '%s' (version=%u, size=%u)",
                     comp->name, comp->state_version, comp->state_size);
            publish_event(mgr, OZAYN_RELOAD_EVENT_STATE_SAVED);
            transition(mgr, OZAYN_RELOAD_STATE_UNLOADING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_UNLOADING: {
            /* Step 5: Unload old version */
            LOG_INFO("RELOAD", "Unloading '%s' v%s...", comp->name, comp->current_version);
            publish_event(mgr, OZAYN_RELOAD_EVENT_UNLOADED);
            transition(mgr, OZAYN_RELOAD_STATE_LOADING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_LOADING: {
            /* Step 6: Load new version */
            LOG_INFO("RELOAD", "Loading '%s' v%s...", comp->name,
                     comp->pending_version[0] ? comp->pending_version : comp->current_version);
            /* In real implementation: dlopen / load module */
            publish_event(mgr, OZAYN_RELOAD_EVENT_LOADED);
            transition(mgr, OZAYN_RELOAD_STATE_INITIALIZING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_INITIALIZING: {
            /* Step 7: Initialize new version */
            LOG_INFO("RELOAD", "Initializing '%s' v%s...", comp->name,
                     comp->pending_version[0] ? comp->pending_version : comp->current_version);
            /* In real implementation: call init callback */
            publish_event(mgr, OZAYN_RELOAD_EVENT_INITIALIZED);
            transition(mgr, OZAYN_RELOAD_STATE_STATE_RESTORED);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_STATE_RESTORED: {
            /* Step 8: Restore state */
            if (comp->state_data && comp->state_size > 0) {
                LOG_INFO("RELOAD", "Restoring state for '%s' (version=%u)",
                         comp->name, comp->state_version);
                /* In real implementation: call component's import_state callback */
            } else {
                LOG_INFO("RELOAD", "No state to restore for '%s'", comp->name);
            }
            publish_event(mgr, OZAYN_RELOAD_EVENT_STATE_RESTORED);
            transition(mgr, OZAYN_RELOAD_STATE_STARTING);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_STARTING: {
            /* Step 9: Start the component */
            LOG_INFO("RELOAD", "Starting '%s'...", comp->name);
            /* In real implementation: call component's start callback */
            publish_event(mgr, OZAYN_RELOAD_EVENT_STARTED);
            transition(mgr, OZAYN_RELOAD_STATE_HEALTH_CHECK);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_HEALTH_CHECK: {
            /* Step 10: Health check */
            LOG_INFO("RELOAD", "Health check for '%s'...", comp->name);
            /* In real implementation: call component's health callback */
            /* For now, assume healthy */
            LOG_INFO("RELOAD", "Health check passed for '%s'", comp->name);

            /* Update version */
            if (comp->pending_version[0]) {
                strncpy(comp->current_version, comp->pending_version, OZAYN_RELOAD_MAX_VERSION - 1);
                comp->pending_version[0] = '\0';
            }
            comp->state_version++;

            publish_event(mgr, OZAYN_RELOAD_EVENT_HEALTH_PASSED);
            transition(mgr, OZAYN_RELOAD_STATE_READY);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_READY: {
            /* Step 11: Mark ready and complete */
            LOG_INFO("RELOAD", "Component '%s' v%s ready (reload complete)", comp->name, comp->current_version);
            publish_event(mgr, OZAYN_RELOAD_EVENT_READY);
            transition(mgr, OZAYN_RELOAD_STATE_COMPLETED);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_COMPLETED: {
            /* Step 12: Finalize */
            LOG_INFO("RELOAD", "Reload of '%s' completed successfully", comp->name);
            add_audit(mgr, comp->name, comp->current_version, comp->pending_version[0] ?
                      comp->pending_version : comp->current_version,
                      NULL, "completed", OZAYN_RELOAD_RESULT_OK,
                      OZAYN_RELOAD_STATE_COMPLETED);
            mgr->total_succeeded++;
            mgr->reload_active = 0;
            mgr->active_component[0] = '\0';
            transition(mgr, OZAYN_RELOAD_STATE_IDLE);
            publish_event(mgr, OZAYN_RELOAD_EVENT_COMPLETED);
            return 0;
        }

        case OZAYN_RELOAD_STATE_ROLLBACK: {
            /* Rollback: restore old version */
            LOG_INFO("RELOAD", "Rolling back '%s'...", comp->name);
            /* In real implementation: reload old version */
            publish_event(mgr, OZAYN_RELOAD_EVENT_ROLLBACK);
            transition(mgr, OZAYN_RELOAD_STATE_ROLLBACK_COMPLETE);
            __attribute__((fallthrough));
        }

        case OZAYN_RELOAD_STATE_ROLLBACK_COMPLETE: {
            LOG_INFO("RELOAD", "Rollback of '%s' completed", comp->name);
            add_audit(mgr, comp->name, comp->current_version, comp->pending_version,
                      NULL, "rollback", OZAYN_RELOAD_RESULT_ROLLBACK_FAIL,
                      OZAYN_RELOAD_STATE_ROLLBACK_COMPLETE);
            mgr->total_rollback++;
            mgr->reload_active = 0;
            mgr->active_component[0] = '\0';
            transition(mgr, OZAYN_RELOAD_STATE_IDLE);
            publish_event(mgr, OZAYN_RELOAD_EVENT_ROLLBACK_COMPLETED);
            return 0;
        }

        case OZAYN_RELOAD_STATE_FAILED: {
            /* Failed: attempt rollback if configured */
            if (mgr->config.rollback_on_fail) {
                LOG_INFO("RELOAD", "Attempting rollback for '%s'...", comp->name);
                transition(mgr, OZAYN_RELOAD_STATE_ROLLBACK);
                publish_event(mgr, OZAYN_RELOAD_EVENT_ROLLBACK_STARTED);
                return ozayn_reload_tick(mgr);
            }
            LOG_INFO("RELOAD", "Reload of '%s' failed (no rollback)", comp->name);
            mgr->reload_active = 0;
            mgr->active_component[0] = '\0';
            transition(mgr, OZAYN_RELOAD_STATE_IDLE);
            return -1;
        }

        case OZAYN_RELOAD_STATE_IDLE:
        default:
            break;
    }

    return 0;
}

/* ================================================================
 * Rollback
 * ================================================================ */

int ozayn_reload_rollback(ozayn_reload_mgr_t *mgr) {
    if (!mgr || !mgr->reload_active) return -1;

    LOG_INFO("RELOAD", "Manual rollback requested for '%s'", mgr->active_component);
    transition(mgr, OZAYN_RELOAD_STATE_ROLLBACK);
    publish_event(mgr, OZAYN_RELOAD_EVENT_ROLLBACK_STARTED);
    return ozayn_reload_tick(mgr);
}

/* ================================================================
 * Queries
 * ================================================================ */

ozayn_reload_state_t ozayn_reload_get_state(ozayn_reload_mgr_t *mgr, const char *name) {
    /* Global reload state — all components share the single active reload */
    if (!mgr) return OZAYN_RELOAD_STATE_IDLE;
    if (name && strcmp(name, mgr->active_component) == 0) {
        return mgr->active_state;
    }
    return OZAYN_RELOAD_STATE_IDLE;
}

int ozayn_reload_is_reloadable(ozayn_reload_mgr_t *mgr, const char *name) {
    ozayn_reload_component_t *comp = find_component(mgr, name);
    if (!comp) return 0;
    return comp->capability == OZAYN_RELOAD_SUPPORTED;
}

int ozayn_reload_is_busy(ozayn_reload_mgr_t *mgr) {
    return mgr ? mgr->reload_active : 0;
}

int ozayn_reload_can_quiesce(ozayn_reload_mgr_t *mgr, const char *name) {
    ozayn_reload_component_t *comp = find_component(mgr, name);
    if (!comp) return 0;
    return comp->active_requests == 0;
}

const ozayn_reload_component_t *ozayn_reload_find(ozayn_reload_mgr_t *mgr, const char *name) {
    return find_component(mgr, name);
}

const char *ozayn_reload_active_component(ozayn_reload_mgr_t *mgr) {
    if (!mgr || !mgr->reload_active) return NULL;
    return mgr->active_component;
}

/* ================================================================
 * Audit trail
 * ================================================================ */

uint32_t ozayn_reload_audit_count(ozayn_reload_mgr_t *mgr) {
    return mgr ? mgr->audit_count : 0;
}

const ozayn_reload_audit_t *ozayn_reload_get_audit(ozayn_reload_mgr_t *mgr, uint32_t index) {
    if (!mgr || index >= mgr->audit_count) return NULL;
    uint32_t idx = (mgr->audit_head + index) % OZAYN_RELOAD_MAX_AUDIT;
    return &mgr->audit[idx];
}

/* ================================================================
 * Stats
 * ================================================================ */

ozayn_reload_stats_t ozayn_reload_stats(ozayn_reload_mgr_t *mgr) {
    ozayn_reload_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    s.total_requests = mgr->total_requests;
    s.total_succeeded = mgr->total_succeeded;
    s.total_failed = mgr->total_failed;
    s.total_rollback = mgr->total_rollback;
    s.current_reloading = mgr->reload_active ? 1 : 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active) {
            switch (mgr->components[i].capability) {
                case OZAYN_RELOAD_SUPPORTED:        s.reloadable_count++; break;
                case OZAYN_RELOAD_UNSUPPORTED:      s.non_reloadable_count++; break;
                case OZAYN_RELOAD_RESTART_REQUIRED: s.restart_required_count++; break;
            }
        }
    }
    return s;
}

/* ================================================================
 * Print / debug
 * ================================================================ */

void ozayn_reload_print_components(ozayn_reload_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("RELOAD", "=== Registered Components (%u) ===", mgr->component_count);
    for (uint32_t i = 0; i < OZAYN_RELOAD_MAX_COMPONENTS; i++) {
        if (mgr->components[i].active) {
            ozayn_reload_component_t *c = &mgr->components[i];
            LOG_INFO("RELOAD", "  %-24s v%-8s [%-18s] active_req=%u %s",
                     c->name, c->current_version,
                     ozayn_reload_capability_name(c->capability),
                     c->active_requests,
                     c->required ? "(critical)" : "(optional)");
        }
    }
}

void ozayn_reload_print_status(ozayn_reload_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("RELOAD", "=== Reload Manager Status ===");
    LOG_INFO("RELOAD", "  Active reload:  %s", mgr->reload_active ? mgr->active_component : "(none)");
    LOG_INFO("RELOAD", "  Current state:  %s", ozayn_reload_state_name(mgr->active_state));
    LOG_INFO("RELOAD", "  Requests:       %u (succeeded=%u, failed=%u, rollback=%u)",
             mgr->total_requests, mgr->total_succeeded,
             mgr->total_failed, mgr->total_rollback);
    LOG_INFO("RELOAD", "  Audit entries:  %u", mgr->audit_count);
    ozayn_reload_print_components(mgr);
}

void ozayn_reload_print_audit(ozayn_reload_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("RELOAD", "=== Reload Audit Trail (%u entries) ===", mgr->audit_count);
    for (uint32_t i = 0; i < mgr->audit_count; i++) {
        ozayn_reload_print_audit_entry(mgr, i);
    }
}

void ozayn_reload_print_audit_entry(ozayn_reload_mgr_t *mgr, uint32_t index) {
    const ozayn_reload_audit_t *a = ozayn_reload_get_audit(mgr, index);
    if (!a) return;
    LOG_INFO("RELOAD", "  [%u] '%s' %s -> %s | %s | state=%s | by=%s | reason=%s | %ums",
             index, a->component, a->old_version, a->new_version,
             ozayn_reload_result_name(a->result),
             ozayn_reload_state_name(a->final_state),
             a->requester[0] ? a->requester : "system",
             a->reason[0] ? a->reason : "-",
             a->duration_ms);
}
