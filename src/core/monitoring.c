#include "monitoring.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * NAMES
 * ================================================================ */

const char *ozayn_health_state_name(ozayn_health_state_t state) {
    switch (state) {
        case OZAYN_HEALTH_UNKNOWN:  return "UNKNOWN";
        case OZAYN_HEALTH_HEALTHY:  return "HEALTHY";
        case OZAYN_HEALTH_DEGRADED: return "DEGRADED";
        case OZAYN_HEALTH_UNHEALTHY: return "UNHEALTHY";
        case OZAYN_HEALTH_FAILED:   return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_metric_type_name(ozayn_metric_type_t type) {
    switch (type) {
        case OZAYN_METRIC_COUNTER: return "COUNTER";
        case OZAYN_METRIC_GAUGE:   return "GAUGE";
    }
    return "UNKNOWN";
}

const char *ozayn_severity_name(ozayn_severity_t severity) {
    switch (severity) {
        case OZAYN_SEVERITY_INFO:     return "INFO";
        case OZAYN_SEVERITY_WARNING:  return "WARNING";
        case OZAYN_SEVERITY_ERROR:    return "ERROR";
        case OZAYN_SEVERITY_CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char *ozayn_incident_state_name(ozayn_incident_state_t state) {
    switch (state) {
        case OZAYN_INCIDENT_DETECTED:      return "DETECTED";
        case OZAYN_INCIDENT_OPEN:          return "OPEN";
        case OZAYN_INCIDENT_INVESTIGATING: return "INVESTIGATING";
        case OZAYN_INCIDENT_RECOVERING:    return "RECOVERING";
        case OZAYN_INCIDENT_RESOLVED:      return "RESOLVED";
    }
    return "UNKNOWN";
}

const char *ozayn_component_name(ozayn_component_id_t id) {
    switch (id) {
        case OZAYN_COMP_CORE:            return "CORE";
        case OZAYN_COMP_RUNTIME:         return "RUNTIME";
        case OZAYN_COMP_EVENT_ENGINE:    return "EVENT_ENGINE";
        case OZAYN_COMP_COMMAND_ENGINE:  return "COMMAND_ENGINE";
        case OZAYN_COMP_TASK_MANAGER:    return "TASK_MANAGER";
        case OZAYN_COMP_PROCESS_MANAGER: return "PROCESS_MANAGER";
        case OZAYN_COMP_MODULE_MANAGER:  return "MODULE_MANAGER";
        case OZAYN_COMP_PLUGIN_MANAGER:  return "PLUGIN_MANAGER";
        case OZAYN_COMP_IPC:             return "IPC";
        case OZAYN_COMP_CONFIGURATION:   return "CONFIGURATION";
        case OZAYN_COMP_LOGGER:          return "LOGGER";
        case OZAYN_COMP_ERROR_RECOVERY:  return "ERROR_RECOVERY";
        case OZAYN_COMP_SECURITY:        return "SECURITY";
        case OZAYN_COMP_AUTHORIZATION:   return "AUTHORIZATION";
        case OZAYN_COMP_RESOURCE_MANAGER: return "RESOURCE_MANAGER";
        case OZAYN_COMP_SCHEDULER:       return "SCHEDULER";
    }
    return "UNKNOWN";
}

/* ================================================================
 * EVENT HELPERS
 * ================================================================ */

static void publish_monitor_event(ozayn_monitoring_manager_t *mgr,
                                  ozayn_event_type_t type,
                                  void *payload) {
    if (mgr && mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             type, OZAYN_SRC_CORE, payload);
    }
}

/* ================================================================
 * HEALTH RECORD HELPERS
 * ================================================================ */

static ozayn_health_record_t *find_health_record(ozayn_monitoring_manager_t *mgr,
                                                  ozayn_component_id_t component) {
    for (int i = 0; i < mgr->health_count; i++) {
        if (mgr->health[i].active && mgr->health[i].component == component) {
            return &mgr->health[i];
        }
    }
    return NULL;
}

static ozayn_health_record_t *alloc_health_record(ozayn_monitoring_manager_t *mgr) {
    for (int i = 0; i < OZAYN_MONITOR_MAX_COMPONENTS; i++) {
        if (!mgr->health[i].active) {
            return &mgr->health[i];
        }
    }
    return NULL;
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

ozayn_result_t ozayn_monitoring_init(ozayn_monitoring_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_monitoring_manager_t));
    mgr->enabled = enabled;
    mgr->initialized = 1;

    /* Initialize all health records as inactive */
    for (int i = 0; i < OZAYN_MONITOR_MAX_COMPONENTS; i++) {
        mgr->health[i].active = 0;
    }

    /* Initialize all metric records as inactive */
    for (int i = 0; i < OZAYN_MONITOR_MAX_METRICS; i++) {
        mgr->metrics[i].active = 0;
    }

    /* Initialize all incident records as inactive */
    for (int i = 0; i < OZAYN_MONITOR_MAX_INCIDENTS; i++) {
        mgr->incidents[i].active = 0;
    }

    mgr->next_incident_id = 1;

    /* Register all components as UNKNOWN by default */
    for (int i = 0; i <= OZAYN_COMP_SCHEDULER; i++) {
        ozayn_health_record_t *rec = alloc_health_record(mgr);
        if (rec) {
            rec->active = 1;
            rec->component = (ozayn_component_id_t)i;
            rec->state = OZAYN_HEALTH_UNKNOWN;
            rec->severity = OZAYN_SEVERITY_INFO;
            rec->reason[0] = '\0';
            rec->last_check = 0;
            rec->state_changed_at = time(NULL);
            rec->check_count = 0;
            rec->failure_count = 0;
            mgr->health_count++;
        }
    }

    LOG_INFO("MONITORING", "Monitoring engine initialized (enabled=%s, components=%d)",
             enabled ? "yes" : "no", mgr->health_count);

    return OZAYN_OK;
}

void ozayn_monitoring_shutdown(ozayn_monitoring_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Log final stats */
    ozayn_monitor_stats_t s = mgr->stats;
    LOG_INFO("MONITORING", "Monitoring shut down (checks=%d, changes=%d, incidents=%d)",
             s.total_checks, s.health_changes, s.incidents_created);

    mgr->initialized = 0;
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_monitoring_set_events(ozayn_monitoring_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_monitoring_set_recovery(ozayn_monitoring_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ================================================================
 * HEALTH REPORTING
 * ================================================================ */

ozayn_result_t ozayn_monitoring_report_health(ozayn_monitoring_manager_t *mgr,
                                               ozayn_component_id_t component,
                                               ozayn_health_state_t state,
                                               ozayn_severity_t severity,
                                               const char *reason) {
    if (!mgr) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    ozayn_health_record_t *rec = find_health_record(mgr, component);
    if (!rec) {
        /* Allocate new record */
        rec = alloc_health_record(mgr);
        if (!rec) {
            LOG_WARN("MONITORING", "No free health record for component %d", (int)component);
            return OZAYN_ERR;
        }
        rec->active = 1;
        rec->component = component;
        rec->state = OZAYN_HEALTH_UNKNOWN;
        rec->check_count = 0;
        rec->failure_count = 0;
        mgr->health_count++;
    }

    ozayn_health_state_t old_state = rec->state;

    rec->state = state;
    rec->severity = severity;
    rec->last_check = time(NULL);
    rec->check_count++;

    if (reason) {
        strncpy(rec->reason, reason, OZAYN_MONITOR_MAX_REASON_LEN - 1);
        rec->reason[OZAYN_MONITOR_MAX_REASON_LEN - 1] = '\0';
    }

    if (state != OZAYN_HEALTH_HEALTHY) {
        rec->failure_count++;
    }

    mgr->stats.total_checks++;

    if (old_state != state) {
        rec->state_changed_at = time(NULL);
        mgr->stats.health_changes++;

        LOG_INFO("MONITORING", "%s health: %s -> %s (severity=%s, reason=%s)",
                 ozayn_component_name(component),
                 ozayn_health_state_name(old_state),
                 ozayn_health_state_name(state),
                 ozayn_severity_name(severity),
                 reason ? reason : "none");

        publish_monitor_event(mgr, OZAYN_EVENT_HEALTH_CHANGED, rec);
    }

    return OZAYN_OK;
}

/* ================================================================
 * HEALTH PROVIDERS (active checks)
 * ================================================================ */

ozayn_result_t ozayn_monitoring_register_provider(ozayn_monitoring_manager_t *mgr,
                                                    ozayn_component_id_t component,
                                                    ozayn_health_check_fn check_fn,
                                                    void *ctx) {
    if (!mgr) return OZAYN_ERR_NULL;
    if (!check_fn) return OZAYN_ERR_NULL;

    int idx = (int)component;
    if (idx < 0 || idx >= OZAYN_MONITOR_MAX_COMPONENTS) return OZAYN_ERR;

    mgr->providers[idx] = check_fn;
    mgr->provider_ctx[idx] = ctx;

    LOG_DEBUG("MONITORING", "Health provider registered for %s",
              ozayn_component_name(component));

    return OZAYN_OK;
}

/* ================================================================
 * COLLECTION (called from runtime loop)
 * ================================================================ */

int ozayn_monitoring_collect(ozayn_monitoring_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    int checks_performed = 0;

    /* Run active health checks for registered providers */
    for (int i = 0; i < OZAYN_MONITOR_MAX_COMPONENTS; i++) {
        if (mgr->providers[i]) {
            ozayn_health_state_t state = mgr->providers[i](
                (ozayn_component_id_t)i, mgr->provider_ctx[i]);

            ozayn_health_record_t *rec = find_health_record(mgr, (ozayn_component_id_t)i);
            if (rec && rec->state != state) {
                ozayn_severity_t sev = OZAYN_SEVERITY_INFO;
                if (state == OZAYN_HEALTH_DEGRADED) sev = OZAYN_SEVERITY_WARNING;
                else if (state == OZAYN_HEALTH_UNHEALTHY) sev = OZAYN_SEVERITY_ERROR;
                else if (state == OZAYN_HEALTH_FAILED) sev = OZAYN_SEVERITY_CRITICAL;

                ozayn_monitoring_report_health(mgr, (ozayn_component_id_t)i,
                                                state, sev, "active check");
            }
            checks_performed++;
        }
    }

    return checks_performed;
}

/* ================================================================
 * METRICS
 * ================================================================ */

ozayn_result_t ozayn_monitoring_register_metric(ozayn_monitoring_manager_t *mgr,
                                                  const char *name,
                                                  ozayn_metric_type_t type,
                                                  ozayn_component_id_t component) {
    if (!mgr || !name) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Check duplicate */
    for (int i = 0; i < mgr->metric_count; i++) {
        if (mgr->metrics[i].active && strcmp(mgr->metrics[i].name, name) == 0) {
            return OZAYN_ERR_STATE; /* already registered */
        }
    }

    /* Find free slot */
    ozayn_metric_record_t *m = NULL;
    for (int i = 0; i < OZAYN_MONITOR_MAX_METRICS; i++) {
        if (!mgr->metrics[i].active) {
            m = &mgr->metrics[i];
            break;
        }
    }
    if (!m) return OZAYN_ERR;

    memset(m, 0, sizeof(ozayn_metric_record_t));
    m->active = 1;
    strncpy(m->name, name, OZAYN_MONITOR_MAX_NAME_LEN - 1);
    m->name[OZAYN_MONITOR_MAX_NAME_LEN - 1] = '\0';
    m->type = type;
    m->component = component;
    m->value = 0;
    m->min_value = 0;
    m->max_value = 0;
    m->last_updated = time(NULL);
    mgr->metric_count++;
    mgr->stats.metrics_registered++;

    LOG_DEBUG("MONITORING", "Metric registered: %s (%s, %s)",
              name, ozayn_metric_type_name(type), ozayn_component_name(component));

    return OZAYN_OK;
}

ozayn_result_t ozayn_monitoring_update_metric(ozayn_monitoring_manager_t *mgr,
                                                const char *name,
                                                int64_t value) {
    if (!mgr || !name) return OZAYN_ERR_NULL;

    for (int i = 0; i < mgr->metric_count; i++) {
        if (mgr->metrics[i].active && strcmp(mgr->metrics[i].name, name) == 0) {
            mgr->metrics[i].value = value;
            mgr->metrics[i].last_updated = time(NULL);

            if (value < mgr->metrics[i].min_value || mgr->metrics[i].last_updated == 0)
                mgr->metrics[i].min_value = value;
            if (value > mgr->metrics[i].max_value)
                mgr->metrics[i].max_value = value;

            mgr->stats.metrics_updated++;
            return OZAYN_OK;
        }
    }

    return OZAYN_ERR; /* metric not found */
}

ozayn_result_t ozayn_monitoring_increment_metric(ozayn_monitoring_manager_t *mgr,
                                                   const char *name,
                                                   int64_t delta) {
    if (!mgr || !name) return OZAYN_ERR_NULL;

    for (int i = 0; i < mgr->metric_count; i++) {
        if (mgr->metrics[i].active && strcmp(mgr->metrics[i].name, name) == 0) {
            mgr->metrics[i].value += delta;
            mgr->metrics[i].last_updated = time(NULL);
            mgr->stats.metrics_updated++;
            return OZAYN_OK;
        }
    }

    return OZAYN_ERR;
}

const ozayn_metric_record_t *ozayn_monitoring_get_metric(
    const ozayn_monitoring_manager_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;

    for (int i = 0; i < mgr->metric_count; i++) {
        if (mgr->metrics[i].active && strcmp(mgr->metrics[i].name, name) == 0) {
            return &mgr->metrics[i];
        }
    }
    return NULL;
}

int ozayn_monitoring_metric_count(const ozayn_monitoring_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_MONITOR_MAX_METRICS; i++) {
        if (mgr->metrics[i].active) count++;
    }
    return count;
}

/* ================================================================
 * INCIDENTS
 * ================================================================ */

ozayn_result_t ozayn_monitoring_create_incident(ozayn_monitoring_manager_t *mgr,
                                                  ozayn_component_id_t component,
                                                  ozayn_severity_t severity,
                                                  const char *reason) {
    if (!mgr) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Find free slot */
    ozayn_incident_t *inc = NULL;
    for (int i = 0; i < OZAYN_MONITOR_MAX_INCIDENTS; i++) {
        if (!mgr->incidents[i].active) {
            inc = &mgr->incidents[i];
            break;
        }
    }
    if (!inc) return OZAYN_ERR;

    memset(inc, 0, sizeof(ozayn_incident_t));
    inc->active = 1;
    inc->id = mgr->next_incident_id++;
    inc->component = component;
    inc->state = OZAYN_INCIDENT_DETECTED;
    inc->severity = severity;
    if (reason) {
        strncpy(inc->reason, reason, OZAYN_MONITOR_MAX_REASON_LEN - 1);
        inc->reason[OZAYN_MONITOR_MAX_REASON_LEN - 1] = '\0';
    }
    inc->detected_at = time(NULL);
    inc->resolved_at = 0;
    mgr->incident_count++;
    mgr->stats.incidents_created++;

    LOG_INFO("MONITORING", "Incident #%u created: %s severity=%s reason=%s",
             inc->id, ozayn_component_name(component),
             ozayn_severity_name(severity),
             reason ? reason : "none");

    publish_monitor_event(mgr, OZAYN_EVENT_INCIDENT_CREATED, inc);

    return OZAYN_OK;
}

ozayn_result_t ozayn_monitoring_resolve_incident(ozayn_monitoring_manager_t *mgr,
                                                   uint32_t incident_id) {
    if (!mgr) return OZAYN_ERR_NULL;

    for (int i = 0; i < mgr->incident_count; i++) {
        if (mgr->incidents[i].active && mgr->incidents[i].id == incident_id) {
            mgr->incidents[i].state = OZAYN_INCIDENT_RESOLVED;
            mgr->incidents[i].resolved_at = time(NULL);
            mgr->stats.incidents_resolved++;

            LOG_INFO("MONITORING", "Incident #%u resolved", incident_id);
            publish_monitor_event(mgr, OZAYN_EVENT_INCIDENT_RESOLVED, &mgr->incidents[i]);

            return OZAYN_OK;
        }
    }

    return OZAYN_ERR;
}

const ozayn_incident_t *ozayn_monitoring_get_incident(
    const ozayn_monitoring_manager_t *mgr, uint32_t incident_id) {
    if (!mgr) return NULL;

    for (int i = 0; i < mgr->incident_count; i++) {
        if (mgr->incidents[i].active && mgr->incidents[i].id == incident_id) {
            return &mgr->incidents[i];
        }
    }
    return NULL;
}

int ozayn_monitoring_open_incident_count(const ozayn_monitoring_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->incident_count; i++) {
        if (mgr->incidents[i].active &&
            mgr->incidents[i].state != OZAYN_INCIDENT_RESOLVED) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * QUERY
 * ================================================================ */

ozayn_health_state_t ozayn_monitoring_get_health(
    const ozayn_monitoring_manager_t *mgr, ozayn_component_id_t component) {
    if (!mgr) return OZAYN_HEALTH_UNKNOWN;

    for (int i = 0; i < mgr->health_count; i++) {
        if (mgr->health[i].active && mgr->health[i].component == component) {
            return mgr->health[i].state;
        }
    }
    return OZAYN_HEALTH_UNKNOWN;
}

ozayn_health_state_t ozayn_monitoring_overall_health(
    const ozayn_monitoring_manager_t *mgr) {
    if (!mgr) return OZAYN_HEALTH_UNKNOWN;

    int has_failed = 0;
    int has_unhealthy = 0;
    int has_degraded = 0;

    for (int i = 0; i < mgr->health_count; i++) {
        if (!mgr->health[i].active) continue;
        switch (mgr->health[i].state) {
            case OZAYN_HEALTH_FAILED:    has_failed = 1; break;
            case OZAYN_HEALTH_UNHEALTHY: has_unhealthy = 1; break;
            case OZAYN_HEALTH_DEGRADED:  has_degraded = 1; break;
            default: break;
        }
    }

    if (has_failed) return OZAYN_HEALTH_FAILED;
    if (has_unhealthy) return OZAYN_HEALTH_UNHEALTHY;
    if (has_degraded) return OZAYN_HEALTH_DEGRADED;
    return OZAYN_HEALTH_HEALTHY;
}

ozayn_monitor_stats_t ozayn_monitoring_stats(const ozayn_monitoring_manager_t *mgr) {
    ozayn_monitor_stats_t zero = { 0 };
    if (!mgr) return zero;
    return mgr->stats;
}

int ozayn_monitoring_is_enabled(const ozayn_monitoring_manager_t *mgr) {
    return mgr ? mgr->enabled : 0;
}
