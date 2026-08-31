#include "service_lifecycle.h"
#include "events.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * service_lifecycle.c — Service Lifecycle Management (Stage 24).
 *
 * Manages the full lifecycle of individual services:
 *   - State machine: UNREGISTERED → REGISTERED → INITIALIZING → RUNNING → DRAINING → STOPPED
 *   - Restart policies: never, on-failure, always
 *   - Restart budget: max N restarts within a time window
 *   - Health monitoring: track healthy/degraded/unhealthy
 *   - Graceful drain with configurable timeout
 *   - Service groups: start/stop multiple services together
 *   - Failure tracking
 */

/* ================================================================
 * Helpers
 * ================================================================ */

static void publish_event(ozayn_svc_lc_manager_t *mgr,
                           ozayn_event_type_t type,
                           void *payload) {
    if (!mgr || !mgr->events) return;
    ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                         type, OZAYN_SRC_SVC_LC, payload);
}

static int name_valid(const char *name) {
    return name && name[0] != '\0';
}

static ozayn_svc_record_t *find_slot(ozayn_svc_lc_manager_t *mgr) {
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (!mgr->services[i].active) return &mgr->services[i];
    }
    return NULL;
}

static ozayn_svc_record_t *find_service(ozayn_svc_lc_manager_t *mgr, const char *name) {
    if (!name_valid(name)) return NULL;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            strcmp(mgr->services[i].name, name) == 0) {
            return &mgr->services[i];
        }
    }
    return NULL;
}

static ozayn_svc_group_t *find_group(ozayn_svc_lc_manager_t *mgr, const char *name) {
    if (!name_valid(name)) return NULL;
    for (uint32_t i = 0; i < mgr->config.max_groups; i++) {
        if (mgr->groups[i].active &&
            strcmp(mgr->groups[i].name, name) == 0) {
            return &mgr->groups[i];
        }
    }
    return NULL;
}

static time_t now_sec(void) {
    return time(NULL);
}

/* ================================================================
 * Names
 * ================================================================ */

const char *ozayn_svc_state_name(ozayn_svc_state_t state) {
    switch (state) {
        case OZAYN_SVC_STATE_UNREGISTERED: return "UNREGISTERED";
        case OZAYN_SVC_STATE_REGISTERED:   return "REGISTERED";
        case OZAYN_SVC_STATE_INITIALIZING: return "INITIALIZING";
        case OZAYN_SVC_STATE_RUNNING:      return "RUNNING";
        case OZAYN_SVC_STATE_DRAINING:     return "DRAINING";
        case OZAYN_SVC_STATE_STOPPING:     return "STOPPING";
        case OZAYN_SVC_STATE_STOPPED:      return "STOPPED";
        case OZAYN_SVC_STATE_FAILED:       return "FAILED";
        case OZAYN_SVC_STATE_RESTARTING:   return "RESTARTING";
        case OZAYN_SVC_STATE_SUSPENDED:    return "SUSPENDED";
    }
    return "UNKNOWN";
}

const char *ozayn_svc_restart_policy_name(ozayn_svc_restart_policy_t policy) {
    switch (policy) {
        case OZAYN_SVC_RESTART_NEVER:       return "NEVER";
        case OZAYN_SVC_RESTART_ON_FAILURE:  return "ON_FAILURE";
        case OZAYN_SVC_RESTART_ALWAYS:      return "ALWAYS";
    }
    return "UNKNOWN";
}

const char *ozayn_svc_health_name(ozayn_svc_health_t health) {
    switch (health) {
        case OZAYN_SVC_HEALTH_UNKNOWN:   return "UNKNOWN";
        case OZAYN_SVC_HEALTH_HEALTHY:   return "HEALTHY";
        case OZAYN_SVC_HEALTH_DEGRADED:  return "DEGRADED";
        case OZAYN_SVC_HEALTH_UNHEALTHY: return "UNHEALTHY";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

int ozayn_svc_lc_init(ozayn_svc_lc_manager_t *mgr, const ozayn_svc_lc_config_t *config) {
    if (!mgr || !config) return -1;
    memset(mgr, 0, sizeof(*mgr));
    mgr->config.max_services = config->max_services > 0 ?
        config->max_services : OZAYN_SVC_MAX_SERVICES;
    mgr->config.max_groups = config->max_groups > 0 ?
        config->max_groups : OZAYN_SVC_MAX_GROUPS;
    if (mgr->config.max_services > OZAYN_SVC_MAX_SERVICES)
        mgr->config.max_services = OZAYN_SVC_MAX_SERVICES;
    if (mgr->config.max_groups > OZAYN_SVC_MAX_GROUPS)
        mgr->config.max_groups = OZAYN_SVC_MAX_GROUPS;
    LOG_INFO("SVC_LC", "Service lifecycle manager initialized (max_services=%u, max_groups=%u)",
             mgr->config.max_services, mgr->config.max_groups);
    return 0;
}

void ozayn_svc_lc_shutdown(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return;
    uint32_t running = 0;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            mgr->services[i].state == OZAYN_SVC_STATE_RUNNING) {
            running++;
        }
    }
    /* Force-stop all running services */
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            mgr->services[i].state != OZAYN_SVC_STATE_STOPPED &&
            mgr->services[i].state != OZAYN_SVC_STATE_UNREGISTERED) {
            mgr->services[i].state = OZAYN_SVC_STATE_STOPPED;
            mgr->services[i].stopped_at = now_sec();
        }
    }
    LOG_INFO("SVC_LC", "Service lifecycle manager shut down (running=%u, restarts=%u, failures=%u)",
             running, mgr->total_restarts, mgr->total_failures);
    memset(mgr->services, 0, sizeof(mgr->services));
    memset(mgr->groups, 0, sizeof(mgr->groups));
}

void ozayn_svc_lc_set_events(ozayn_svc_lc_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

/* ================================================================
 * Registration
 * ================================================================ */

int ozayn_svc_lc_register(ozayn_svc_lc_manager_t *mgr, const ozayn_svc_config_t *config) {
    if (!mgr || !config || !name_valid(config->name)) return -1;
    if (find_service(mgr, config->name)) return -2; /* already registered */

    ozayn_svc_record_t *svc = find_slot(mgr);
    if (!svc) return -3; /* no slot */

    memset(svc, 0, sizeof(*svc));
    svc->active = 1;
    strncpy(svc->name, config->name, OZAYN_SVC_MAX_NAME - 1);
    if (config->version)
        strncpy(svc->version, config->version, OZAYN_SVC_MAX_VERSION - 1);
    svc->state = OZAYN_SVC_STATE_REGISTERED;
    svc->restart_policy = config->restart_policy;
    svc->health = OZAYN_SVC_HEALTH_UNKNOWN;
    svc->required = config->required;
    svc->max_restarts = config->max_restarts;
    svc->restart_window_ms = config->restart_window_ms > 0 ?
        config->restart_window_ms : OZAYN_SVC_MAX_RESTART_WINDOW_MS;
    svc->drain_timeout_ms = config->drain_timeout_ms > 0 ?
        config->drain_timeout_ms : 5000;
    svc->health_check_ms = config->health_check_ms;

    LOG_INFO("SVC_LC", "Registered service '%s' v%s (restart=%s, required=%s)",
             svc->name, svc->version,
             ozayn_svc_restart_policy_name(svc->restart_policy),
             svc->required ? "yes" : "no");
    publish_event(mgr, OZAYN_SVC_LC_EVENT_REGISTERED, NULL);
    return 0;
}

int ozayn_svc_lc_unregister(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;
    LOG_INFO("SVC_LC", "Unregistered service '%s'", svc->name);
    svc->active = 0;
    return 0;
}

/* ================================================================
 * State transitions
 * ================================================================ */

int ozayn_svc_lc_start(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;

    if (svc->state == OZAYN_SVC_STATE_RUNNING) return 0; /* already running */

    if (svc->state != OZAYN_SVC_STATE_REGISTERED &&
        svc->state != OZAYN_SVC_STATE_STOPPED &&
        svc->state != OZAYN_SVC_STATE_FAILED &&
        svc->state != OZAYN_SVC_STATE_RESTARTING) {
        LOG_INFO("SVC_LC", "Cannot start '%s' in state %s",
                 svc->name, ozayn_svc_state_name(svc->state));
        return -2;
    }

    svc->state = OZAYN_SVC_STATE_INITIALIZING;
    LOG_INFO("SVC_LC", "Starting service '%s' v%s...", svc->name, svc->version);
    publish_event(mgr, OZAYN_SVC_LC_EVENT_STARTING, NULL);

    /* In real implementation, this would call the service's init callback.
     * For now, transition directly to RUNNING. */
    svc->state = OZAYN_SVC_STATE_RUNNING;
    svc->health = OZAYN_SVC_HEALTH_HEALTHY;
    svc->started_at = now_sec();
    svc->failure_count = 0;

    LOG_INFO("SVC_LC", "Service '%s' started (state=%s, health=%s)",
             svc->name, ozayn_svc_state_name(svc->state),
             ozayn_svc_health_name(svc->health));
    publish_event(mgr, OZAYN_SVC_LC_EVENT_STARTED, NULL);
    return 0;
}

int ozayn_svc_lc_stop(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;

    if (svc->state == OZAYN_SVC_STATE_STOPPED ||
        svc->state == OZAYN_SVC_STATE_UNREGISTERED) return 0;

    /* Graceful drain phase */
    svc->state = OZAYN_SVC_STATE_DRAINING;
    LOG_INFO("SVC_LC", "Draining service '%s' (timeout=%ums)...",
             svc->name, svc->drain_timeout_ms);
    publish_event(mgr, OZAYN_SVC_LC_EVENT_DRAINING, NULL);

    /* In real implementation, wait for in-flight work to complete up to drain_timeout_ms.
     * For now, transition directly to STOPPED. */
    svc->state = OZAYN_SVC_STATE_STOPPED;
    svc->stopped_at = now_sec();

    LOG_INFO("SVC_LC", "Service '%s' stopped", svc->name);
    publish_event(mgr, OZAYN_SVC_LC_EVENT_STOPPED, NULL);
    return 0;
}

int ozayn_svc_lc_restart(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;

    /* Check restart budget */
    if (!ozayn_svc_lc_can_restart(mgr, name)) {
        LOG_INFO("SVC_LC", "Service '%s' exceeded restart budget (%u in window)",
                 svc->name, svc->failures_in_window);
        svc->state = OZAYN_SVC_STATE_FAILED;
        publish_event(mgr, OZAYN_SVC_LC_EVENT_RESTART_FAILED, NULL);
        return -2;
    }

    LOG_INFO("SVC_LC", "Restarting service '%s' (restart #%u)...",
             svc->name, svc->restart_count + 1);
    svc->state = OZAYN_SVC_STATE_RESTARTING;
    publish_event(mgr, OZAYN_SVC_LC_EVENT_RESTARTING, NULL);

    svc->restart_count++;
    svc->failures_in_window++;
    mgr->total_restarts++;

    /* Stop then start */
    ozayn_svc_lc_stop(mgr, name);
    ozayn_svc_lc_start(mgr, name);

    return 0;
}

int ozayn_svc_lc_suspend(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;
    if (svc->state != OZAYN_SVC_STATE_RUNNING) return -2;

    svc->state = OZAYN_SVC_STATE_SUSPENDED;
    LOG_INFO("SVC_LC", "Service '%s' suspended", svc->name);
    publish_event(mgr, OZAYN_SVC_LC_EVENT_SUSPENDED, NULL);
    return 0;
}

int ozayn_svc_lc_resume(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;
    if (svc->state != OZAYN_SVC_STATE_SUSPENDED) return -2;

    svc->state = OZAYN_SVC_STATE_RUNNING;
    LOG_INFO("SVC_LC", "Service '%s' resumed", svc->name);
    publish_event(mgr, OZAYN_SVC_LC_EVENT_RESUMED, NULL);
    return 0;
}

/* ================================================================
 * Health
 * ================================================================ */

int ozayn_svc_lc_set_health(ozayn_svc_lc_manager_t *mgr, const char *name,
                              ozayn_svc_health_t health) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;

    ozayn_svc_health_t old = svc->health;
    svc->health = health;
    svc->last_health_check = now_sec();

    if (old != health) {
        LOG_INFO("SVC_LC", "Service '%s' health: %s -> %s",
                 svc->name, ozayn_svc_health_name(old), ozayn_svc_health_name(health));
        publish_event(mgr, OZAYN_SVC_LC_EVENT_HEALTH_CHANGED, NULL);

        /* If unhealthy and running, track failure */
        if (health == OZAYN_SVC_HEALTH_UNHEALTHY &&
            svc->state == OZAYN_SVC_STATE_RUNNING) {
            svc->failure_count++;
            mgr->total_failures++;

            /* Track failures within window for restart budget */
            time_t now = now_sec();
            if (svc->first_failure_in_window == 0) {
                svc->first_failure_in_window = now;
                svc->failures_in_window = 1;
            } else if ((now - svc->first_failure_in_window) * 1000 <=
                       svc->restart_window_ms) {
                svc->failures_in_window++;
            } else {
                svc->first_failure_in_window = now;
                svc->failures_in_window = 1;
            }

            /* Auto-restart if policy allows */
            if (svc->restart_policy == OZAYN_SVC_RESTART_ON_FAILURE ||
                svc->restart_policy == OZAYN_SVC_RESTART_ALWAYS) {
                LOG_INFO("SVC_LC", "Auto-restarting unhealthy service '%s'", svc->name);
                ozayn_svc_lc_restart(mgr, name);
            } else {
                svc->state = OZAYN_SVC_STATE_FAILED;
                publish_event(mgr, OZAYN_SVC_LC_EVENT_FAILED, NULL);
            }
        }
    }
    return 0;
}

int ozayn_svc_lc_check_health(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return -1;
    if (svc->state != OZAYN_SVC_STATE_RUNNING) return 0;

    svc->last_health_check = now_sec();
    /* In real implementation, call the service's health callback.
     * For now, just mark as healthy. */
    if (svc->health == OZAYN_SVC_HEALTH_UNKNOWN)
        svc->health = OZAYN_SVC_HEALTH_HEALTHY;

    return 0;
}

int ozayn_svc_lc_check_all(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return -1;
    uint32_t checked = 0;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            mgr->services[i].state == OZAYN_SVC_STATE_RUNNING) {
            ozayn_svc_lc_check_health(mgr, mgr->services[i].name);
            checked++;
        }
    }
    publish_event(mgr, OZAYN_SVC_LC_EVENT_HEALTH_CHECK, NULL);
    LOG_INFO("SVC_LC", "Health check complete: %u services checked", checked);
    return 0;
}

/* ================================================================
 * Groups
 * ================================================================ */

int ozayn_svc_lc_group_create(ozayn_svc_lc_manager_t *mgr, const char *group_name) {
    if (!mgr || !name_valid(group_name)) return -1;
    if (find_group(mgr, group_name)) return -2;

    for (uint32_t i = 0; i < mgr->config.max_groups; i++) {
        if (!mgr->groups[i].active) {
            memset(&mgr->groups[i], 0, sizeof(ozayn_svc_group_t));
            mgr->groups[i].active = 1;
            strncpy(mgr->groups[i].name, group_name, OZAYN_SVC_MAX_GROUP_NAME - 1);
            LOG_INFO("SVC_LC", "Created service group '%s'", group_name);
            return 0;
        }
    }
    return -3; /* no slot */
}

int ozayn_svc_lc_group_add(ozayn_svc_lc_manager_t *mgr, const char *group_name,
                             const char *service_name) {
    ozayn_svc_group_t *grp = find_group(mgr, group_name);
    if (!grp) return -1;
    if (!find_service(mgr, service_name)) return -2;

    if (grp->service_count >= OZAYN_SVC_MAX_PER_GROUP) return -3;

    /* Check not already in group */
    for (uint32_t i = 0; i < grp->service_count; i++) {
        if (strcmp(grp->services[i], service_name) == 0) return 0;
    }

    strncpy(grp->services[grp->service_count], service_name, OZAYN_SVC_MAX_NAME - 1);
    grp->service_count++;
    LOG_INFO("SVC_LC", "Added '%s' to group '%s'", service_name, group_name);
    return 0;
}

int ozayn_svc_lc_group_start(ozayn_svc_lc_manager_t *mgr, const char *group_name) {
    ozayn_svc_group_t *grp = find_group(mgr, group_name);
    if (!grp) return -1;

    LOG_INFO("SVC_LC", "Starting group '%s' (%u services)...", group_name, grp->service_count);
    int started = 0;
    for (uint32_t i = 0; i < grp->service_count; i++) {
        if (ozayn_svc_lc_start(mgr, grp->services[i]) == 0) started++;
    }
    LOG_INFO("SVC_LC", "Group '%s': %d/%u started", group_name, started, grp->service_count);
    return 0;
}

int ozayn_svc_lc_group_stop(ozayn_svc_lc_manager_t *mgr, const char *group_name) {
    ozayn_svc_group_t *grp = find_group(mgr, group_name);
    if (!grp) return -1;

    LOG_INFO("SVC_LC", "Stopping group '%s' (%u services)...", group_name, grp->service_count);
    int stopped = 0;
    for (uint32_t i = 0; i < grp->service_count; i++) {
        if (ozayn_svc_lc_stop(mgr, grp->services[i]) == 0) stopped++;
    }
    LOG_INFO("SVC_LC", "Group '%s': %d/%u stopped", group_name, stopped, grp->service_count);
    return 0;
}

/* ================================================================
 * Queries
 * ================================================================ */

const ozayn_svc_record_t *ozayn_svc_lc_find(ozayn_svc_lc_manager_t *mgr, const char *name) {
    return find_service(mgr, name);
}

ozayn_svc_state_t ozayn_svc_lc_get_state(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    return svc ? svc->state : OZAYN_SVC_STATE_UNREGISTERED;
}

int ozayn_svc_lc_is_running(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    return svc && svc->state == OZAYN_SVC_STATE_RUNNING;
}

int ozayn_svc_lc_can_restart(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) return 0;
    if (svc->max_restarts == 0) return 1; /* unlimited */
    if (svc->failures_in_window < svc->max_restarts) return 1;

    /* Check if window has expired */
    time_t now = now_sec();
    if (svc->first_failure_in_window > 0 &&
        (now - svc->first_failure_in_window) * 1000 > svc->restart_window_ms) {
        svc->failures_in_window = 0;
        svc->first_failure_in_window = 0;
        return 1;
    }
    return 0;
}

uint32_t ozayn_svc_lc_running_count(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            mgr->services[i].state == OZAYN_SVC_STATE_RUNNING) count++;
    }
    return count;
}

uint32_t ozayn_svc_lc_failed_count(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            mgr->services[i].state == OZAYN_SVC_STATE_FAILED) count++;
    }
    return count;
}

/* ================================================================
 * Stats
 * ================================================================ */

ozayn_svc_lc_stats_t ozayn_svc_lc_stats(ozayn_svc_lc_manager_t *mgr) {
    ozayn_svc_lc_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    s.total_restarts = mgr->total_restarts;
    s.total_failures = mgr->total_failures;

    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active) {
            s.total_services++;
            switch (mgr->services[i].state) {
                case OZAYN_SVC_STATE_RUNNING:
                case OZAYN_SVC_STATE_SUSPENDED:
                    s.running++;
                    break;
                case OZAYN_SVC_STATE_STOPPED:
                case OZAYN_SVC_STATE_UNREGISTERED:
                    s.stopped++;
                    break;
                case OZAYN_SVC_STATE_FAILED:
                    s.failed++;
                    break;
                default:
                    break;
            }
        }
    }

    for (uint32_t i = 0; i < mgr->config.max_groups; i++) {
        if (mgr->groups[i].active) s.total_groups++;
    }
    return s;
}

/* ================================================================
 * Print / debug
 * ================================================================ */

void ozayn_svc_lc_print_status(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return;
    LOG_INFO("SVC_LC", "=== Service Lifecycle Status ===");
    LOG_INFO("SVC_LC", "  Services: %u", ozayn_svc_lc_running_count(mgr));
    LOG_INFO("SVC_LC", "  Groups:   %u", (unsigned)mgr->config.max_groups);
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active) {
            ozayn_svc_record_t *svc = &mgr->services[i];
            LOG_INFO("SVC_LC", "  %-24s v%-8s %-12s health=%-10s restarts=%u",
                     svc->name, svc->version,
                     ozayn_svc_state_name(svc->state),
                     ozayn_svc_health_name(svc->health),
                     svc->restart_count);
        }
    }
}

void ozayn_svc_lc_print_service(ozayn_svc_lc_manager_t *mgr, const char *name) {
    ozayn_svc_record_t *svc = find_service(mgr, name);
    if (!svc) {
        LOG_INFO("SVC_LC", "Service '%s' not found", name ? name : "(null)");
        return;
    }
    LOG_INFO("SVC_LC", "=== Service: %s ===", svc->name);
    LOG_INFO("SVC_LC", "  Version:      %s", svc->version);
    LOG_INFO("SVC_LC", "  State:        %s", ozayn_svc_state_name(svc->state));
    LOG_INFO("SVC_LC", "  Health:       %s", ozayn_svc_health_name(svc->health));
    LOG_INFO("SVC_LC", "  Restart:      %s (max=%u, window=%ums)",
             ozayn_svc_restart_policy_name(svc->restart_policy),
             svc->max_restarts, svc->restart_window_ms);
    LOG_INFO("SVC_LC", "  Drain timeout:%ums", svc->drain_timeout_ms);
    LOG_INFO("SVC_LC", "  Required:     %s", svc->required ? "yes" : "no");
    LOG_INFO("SVC_LC", "  Restarts:     %u", svc->restart_count);
    LOG_INFO("SVC_LC", "  Failures:     %u", svc->failure_count);
    if (svc->started_at > 0) {
        LOG_INFO("SVC_LC", "  Started at:   %ld", (long)svc->started_at);
    }
    if (svc->stopped_at > 0) {
        LOG_INFO("SVC_LC", "  Stopped at:   %ld", (long)svc->stopped_at);
    }
}

void ozayn_svc_lc_print_groups(ozayn_svc_lc_manager_t *mgr) {
    if (!mgr) return;
    LOG_INFO("SVC_LC", "=== Service Groups ===");
    for (uint32_t i = 0; i < mgr->config.max_groups; i++) {
        if (mgr->groups[i].active) {
            ozayn_svc_group_t *grp = &mgr->groups[i];
            LOG_INFO("SVC_LC", "  '%s' (%u services):", grp->name, grp->service_count);
            for (uint32_t j = 0; j < grp->service_count; j++) {
                ozayn_svc_record_t *svc = find_service(mgr, grp->services[j]);
                LOG_INFO("SVC_LC", "    - %s [%s]", grp->services[j],
                         svc ? ozayn_svc_state_name(svc->state) : "NOT_FOUND");
            }
        }
    }
}
