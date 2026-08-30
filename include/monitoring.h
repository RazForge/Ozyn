#ifndef OZAYN_MONITORING_H
#define OZAYN_MONITORING_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * monitoring.h — Monitoring & Health Engine (Stage 18).
 *
 * OZAYN's internal observability system.
 * Collects metrics, assesses health, tracks incidents.
 *
 * Monitoring observes. Recovery acts.
 */

/* ---- Constants ---- */
#define OZAYN_MONITOR_MAX_COMPONENTS   16
#define OZAYN_MONITOR_MAX_METRICS      64
#define OZAYN_MONITOR_MAX_INCIDENTS    32
#define OZAYN_MONITOR_MAX_NAME_LEN     32
#define OZAYN_MONITOR_MAX_REASON_LEN   64

/* ---- Health states ---- */
typedef enum {
    OZAYN_HEALTH_UNKNOWN  = 0,
    OZAYN_HEALTH_HEALTHY  = 1,
    OZAYN_HEALTH_DEGRADED = 2,
    OZAYN_HEALTH_UNHEALTHY = 3,
    OZAYN_HEALTH_FAILED   = 4,
} ozayn_health_state_t;

/* ---- Metric types ---- */
typedef enum {
    OZAYN_METRIC_COUNTER = 0,
    OZAYN_METRIC_GAUGE   = 1,
} ozayn_metric_type_t;

/* ---- Severity ---- */
typedef enum {
    OZAYN_SEVERITY_INFO     = 0,
    OZAYN_SEVERITY_WARNING  = 1,
    OZAYN_SEVERITY_ERROR    = 2,
    OZAYN_SEVERITY_CRITICAL = 3,
} ozayn_severity_t;

/* ---- Incident states ---- */
typedef enum {
    OZAYN_INCIDENT_DETECTED     = 0,
    OZAYN_INCIDENT_OPEN         = 1,
    OZAYN_INCIDENT_INVESTIGATING = 2,
    OZAYN_INCIDENT_RECOVERING   = 3,
    OZAYN_INCIDENT_RESOLVED     = 4,
} ozayn_incident_state_t;

/* ---- Component IDs ---- */
typedef enum {
    OZAYN_COMP_CORE           = 0,
    OZAYN_COMP_RUNTIME        = 1,
    OZAYN_COMP_EVENT_ENGINE   = 2,
    OZAYN_COMP_COMMAND_ENGINE = 3,
    OZAYN_COMP_TASK_MANAGER   = 4,
    OZAYN_COMP_PROCESS_MANAGER = 5,
    OZAYN_COMP_MODULE_MANAGER = 6,
    OZAYN_COMP_PLUGIN_MANAGER = 7,
    OZAYN_COMP_IPC            = 8,
    OZAYN_COMP_CONFIGURATION  = 9,
    OZAYN_COMP_LOGGER         = 10,
    OZAYN_COMP_ERROR_RECOVERY = 11,
    OZAYN_COMP_SECURITY       = 12,
    OZAYN_COMP_AUTHORIZATION  = 13,
    OZAYN_COMP_RESOURCE_MANAGER = 14,
    OZAYN_COMP_SCHEDULER      = 15,
} ozayn_component_id_t;

/* ---- Health record (per-component) ---- */
typedef struct {
    int                    active;
    ozayn_component_id_t   component;
    ozayn_health_state_t   state;
    ozayn_severity_t       severity;
    char                   reason[OZAYN_MONITOR_MAX_REASON_LEN];
    time_t                 last_check;
    time_t                 state_changed_at;
    int                    check_count;
    int                    failure_count;
} ozayn_health_record_t;

/* ---- Metric record ---- */
typedef struct {
    int                    active;
    char                   name[OZAYN_MONITOR_MAX_NAME_LEN];
    ozayn_metric_type_t    type;
    ozayn_component_id_t   component;
    int64_t                value;
    int64_t                min_value;
    int64_t                max_value;
    time_t                 last_updated;
} ozayn_metric_record_t;

/* ---- Incident ---- */
typedef struct {
    int                    active;
    uint32_t               id;
    ozayn_component_id_t   component;
    ozayn_incident_state_t state;
    ozayn_severity_t       severity;
    char                   reason[OZAYN_MONITOR_MAX_REASON_LEN];
    time_t                 detected_at;
    time_t                 resolved_at;
} ozayn_incident_t;

/* ---- Monitoring statistics ---- */
typedef struct {
    int total_checks;
    int health_changes;
    int incidents_created;
    int incidents_resolved;
    int metrics_registered;
    int metrics_updated;
} ozayn_monitor_stats_t;

/* ---- Health provider callback ---- */
typedef ozayn_health_state_t (*ozayn_health_check_fn)(ozayn_component_id_t id, void *ctx);

/* ---- Monitoring manager ---- */
typedef struct {
    int                     enabled;
    int                     initialized;
    /* Health records */
    ozayn_health_record_t   health[OZAYN_MONITOR_MAX_COMPONENTS];
    int                     health_count;
    /* Metrics */
    ozayn_metric_record_t   metrics[OZAYN_MONITOR_MAX_METRICS];
    int                     metric_count;
    /* Incidents */
    ozayn_incident_t        incidents[OZAYN_MONITOR_MAX_INCIDENTS];
    int                     incident_count;
    uint32_t                next_incident_id;
    /* Statistics */
    ozayn_monitor_stats_t   stats;
    /* Health providers (optional callbacks) */
    ozayn_health_check_fn   providers[OZAYN_MONITOR_MAX_COMPONENTS];
    void                   *provider_ctx[OZAYN_MONITOR_MAX_COMPONENTS];
    /* Bindings */
    void                   *events;
    void                   *recovery;
} ozayn_monitoring_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_monitoring_init(ozayn_monitoring_manager_t *mgr, int enabled);
void           ozayn_monitoring_shutdown(ozayn_monitoring_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_monitoring_set_events(ozayn_monitoring_manager_t *mgr, void *events);
void ozayn_monitoring_set_recovery(ozayn_monitoring_manager_t *mgr, void *recovery);

/* ---- Health reporting ---- */
ozayn_result_t ozayn_monitoring_report_health(ozayn_monitoring_manager_t *mgr,
                                               ozayn_component_id_t component,
                                               ozayn_health_state_t state,
                                               ozayn_severity_t severity,
                                               const char *reason);

/* ---- Health providers (active checks) ---- */
ozayn_result_t ozayn_monitoring_register_provider(ozayn_monitoring_manager_t *mgr,
                                                    ozayn_component_id_t component,
                                                    ozayn_health_check_fn check_fn,
                                                    void *ctx);

/* ---- Collection (called from runtime loop) ---- */
int  ozayn_monitoring_collect(ozayn_monitoring_manager_t *mgr);

/* ---- Metrics ---- */
ozayn_result_t ozayn_monitoring_register_metric(ozayn_monitoring_manager_t *mgr,
                                                  const char *name,
                                                  ozayn_metric_type_t type,
                                                  ozayn_component_id_t component);
ozayn_result_t ozayn_monitoring_update_metric(ozayn_monitoring_manager_t *mgr,
                                                const char *name,
                                                int64_t value);
ozayn_result_t ozayn_monitoring_increment_metric(ozayn_monitoring_manager_t *mgr,
                                                   const char *name,
                                                   int64_t delta);
const ozayn_metric_record_t *ozayn_monitoring_get_metric(
    const ozayn_monitoring_manager_t *mgr, const char *name);
int ozayn_monitoring_metric_count(const ozayn_monitoring_manager_t *mgr);

/* ---- Incidents ---- */
ozayn_result_t ozayn_monitoring_create_incident(ozayn_monitoring_manager_t *mgr,
                                                  ozayn_component_id_t component,
                                                  ozayn_severity_t severity,
                                                  const char *reason);
ozayn_result_t ozayn_monitoring_resolve_incident(ozayn_monitoring_manager_t *mgr,
                                                   uint32_t incident_id);
const ozayn_incident_t *ozayn_monitoring_get_incident(
    const ozayn_monitoring_manager_t *mgr, uint32_t incident_id);
int ozayn_monitoring_open_incident_count(const ozayn_monitoring_manager_t *mgr);

/* ---- Query ---- */
ozayn_health_state_t ozayn_monitoring_get_health(
    const ozayn_monitoring_manager_t *mgr, ozayn_component_id_t component);
ozayn_health_state_t ozayn_monitoring_overall_health(
    const ozayn_monitoring_manager_t *mgr);
ozayn_monitor_stats_t ozayn_monitoring_stats(const ozayn_monitoring_manager_t *mgr);
int ozayn_monitoring_is_enabled(const ozayn_monitoring_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_health_state_name(ozayn_health_state_t state);
const char *ozayn_metric_type_name(ozayn_metric_type_t type);
const char *ozayn_severity_name(ozayn_severity_t severity);
const char *ozayn_incident_state_name(ozayn_incident_state_t state);
const char *ozayn_component_name(ozayn_component_id_t id);

#endif
