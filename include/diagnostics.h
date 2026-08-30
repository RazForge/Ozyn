#ifndef OZAYN_DIAGNOSTICS_H
#define OZAYN_DIAGNOSTICS_H

#include <stdint.h>
#include <time.h>

/*
 * diagnostics.h — Diagnostics & Debugging Engine (Stage 19).
 *
 * Runtime-level diagnostics: evidence collection, correlation, timelines,
 * findings, snapshots, and failure analysis.
 *
 * Monitoring detects. Diagnostics investigates. Recovery acts.
 *
 * This header is self-contained to avoid circular includes with monitoring.h.
 * Types matching monitoring.h are redefined here for independence.
 */

/* ---- Constants ---- */
#define OZAYN_DIAG_MAX_EVIDENCE      128
#define OZAYN_DIAG_MAX_FINDINGS      32
#define OZAYN_DIAG_MAX_SESSIONS       8
#define OZAYN_DIAG_MAX_TIMELINE      256
#define OZAYN_DIAG_MAX_SNAPSHOTS      16
#define OZAYN_DIAG_MAX_FAILURES      64
#define OZAYN_DIAG_MAX_CORR_LEN      16
#define OZAYN_DIAG_MAX_DESC_LEN      64
#define OZAYN_DIAG_MAX_COMPONENTS    16

/* ---- Component IDs (matches monitoring.h values) ---- */
typedef enum {
    OZAYN_DIAG_COMP_CORE           = 0,
    OZAYN_DIAG_COMP_RUNTIME        = 1,
    OZAYN_DIAG_COMP_EVENT_ENGINE   = 2,
    OZAYN_DIAG_COMP_COMMAND_ENGINE = 3,
    OZAYN_DIAG_COMP_TASK_MANAGER   = 4,
    OZAYN_DIAG_COMP_PROCESS_MANAGER = 5,
    OZAYN_DIAG_COMP_MODULE_MANAGER = 6,
    OZAYN_DIAG_COMP_PLUGIN_MANAGER = 7,
    OZAYN_DIAG_COMP_IPC            = 8,
    OZAYN_DIAG_COMP_CONFIGURATION  = 9,
    OZAYN_DIAG_COMP_LOGGER         = 10,
    OZAYN_DIAG_COMP_ERROR_RECOVERY = 11,
    OZAYN_DIAG_COMP_SECURITY       = 12,
    OZAYN_DIAG_COMP_AUTHORIZATION  = 13,
    OZAYN_DIAG_COMP_RESOURCE_MANAGER = 14,
    OZAYN_DIAG_COMP_SCHEDULER      = 15,
} ozayn_diag_component_t;

/* ---- Health states (matches monitoring.h values) ---- */
typedef enum {
    OZAYN_DIAG_HEALTH_UNKNOWN  = 0,
    OZAYN_DIAG_HEALTH_HEALTHY  = 1,
    OZAYN_DIAG_HEALTH_DEGRADED = 2,
    OZAYN_DIAG_HEALTH_UNHEALTHY = 3,
    OZAYN_DIAG_HEALTH_FAILED   = 4,
} ozayn_diag_health_t;

/* ---- Diagnostic levels ---- */
typedef enum {
    OZAYN_DIAG_LEVEL_NORMAL   = 0,
    OZAYN_DIAG_LEVEL_DETAILED = 1,
    OZAYN_DIAG_LEVEL_DEBUG    = 2,
    OZAYN_DIAG_LEVEL_TRACE    = 3,
} ozayn_diag_level_t;

/* ---- Diagnostic session states ---- */
typedef enum {
    OZAYN_DIAG_SESSION_CREATED    = 0,
    OZAYN_DIAG_SESSION_COLLECTING = 1,
    OZAYN_DIAG_SESSION_ANALYZING  = 2,
    OZAYN_DIAG_SESSION_FINDINGS   = 3,
    OZAYN_DIAG_SESSION_COMPLETED  = 4,
    OZAYN_DIAG_SESSION_FAILED     = 5,
} ozayn_diag_session_state_t;

/* ---- Diagnostic targets ---- */
typedef enum {
    OZAYN_DIAG_TARGET_CORE          = 0,
    OZAYN_DIAG_TARGET_RUNTIME       = 1,
    OZAYN_DIAG_TARGET_TASK          = 2,
    OZAYN_DIAG_TARGET_PROCESS       = 3,
    OZAYN_DIAG_TARGET_MODULE        = 4,
    OZAYN_DIAG_TARGET_PLUGIN        = 5,
    OZAYN_DIAG_TARGET_IPC           = 6,
    OZAYN_DIAG_TARGET_SCHEDULER     = 7,
    OZAYN_DIAG_TARGET_RESOURCE      = 8,
    OZAYN_DIAG_TARGET_EVENT         = 9,
    OZAYN_DIAG_TARGET_COMPONENT     = 10,
    OZAYN_DIAG_TARGET_SYSTEM        = 11,
} ozayn_diag_target_t;

/* ---- Confidence levels ---- */
typedef enum {
    OZAYN_DIAG_CONFIDENCE_NONE     = 0,
    OZAYN_DIAG_CONFIDENCE_LOW      = 1,
    OZAYN_DIAG_CONFIDENCE_MEDIUM   = 2,
    OZAYN_DIAG_CONFIDENCE_HIGH     = 3,
    OZAYN_DIAG_CONFIDENCE_CERTAIN  = 4,
} ozayn_diag_confidence_t;

/* ---- Finding severity ---- */
typedef enum {
    OZAYN_DIAG_SEV_INFO     = 0,
    OZAYN_DIAG_SEV_WARNING  = 1,
    OZAYN_DIAG_SEV_ERROR    = 2,
    OZAYN_DIAG_SEV_CRITICAL = 3,
} ozayn_diag_finding_sev_t;

/* ---- Evidence record ---- */
typedef struct {
    int                    active;
    uint32_t               id;
    char                   correlation_id[OZAYN_DIAG_MAX_CORR_LEN];
    ozayn_diag_component_t component;
    ozayn_diag_target_t    target;
    char                   description[OZAYN_DIAG_MAX_DESC_LEN];
    time_t                 timestamp;
    int                    is_redacted;
} ozayn_evidence_t;

/* ---- Diagnostic finding ---- */
typedef struct {
    int                       active;
    uint32_t                  id;
    char                      correlation_id[OZAYN_DIAG_MAX_CORR_LEN];
    ozayn_diag_component_t    component;
    ozayn_diag_finding_sev_t  severity;
    char                      observation[OZAYN_DIAG_MAX_DESC_LEN];
    char                      possible_cause[OZAYN_DIAG_MAX_DESC_LEN];
    ozayn_diag_confidence_t   confidence;
    time_t                    timestamp;
} ozayn_diagnostic_finding_t;

/* ---- Timeline entry ---- */
typedef struct {
    int                    active;
    uint32_t               seq;
    char                   correlation_id[OZAYN_DIAG_MAX_CORR_LEN];
    ozayn_diag_component_t component;
    char                   description[OZAYN_DIAG_MAX_DESC_LEN];
    time_t                 timestamp;
} ozayn_timeline_entry_t;

/* ---- Diagnostic session ---- */
typedef struct {
    int                            active;
    uint32_t                       id;
    char                           correlation_id[OZAYN_DIAG_MAX_CORR_LEN];
    ozayn_diag_target_t            target;
    ozayn_diag_session_state_t     state;
    int                            evidence_count;
    int                            finding_count;
    time_t                         started_at;
    time_t                         completed_at;
} ozayn_diag_session_t;

/* ---- Diagnostic snapshot ---- */
typedef struct {
    int                         active;
    uint32_t                    id;
    time_t                      timestamp;
    ozayn_diag_health_t         overall_health;
    ozayn_diag_health_t         component_health[OZAYN_DIAG_MAX_COMPONENTS];
    int                         open_incidents;
} ozayn_diag_snapshot_t;

/* ---- Failure frequency record ---- */
typedef struct {
    int                    active;
    ozayn_diag_component_t component;
    int                    total_failures;
    int                    recent_failures;
    time_t                 last_failure_at;
    time_t                 window_start;
} ozayn_failure_freq_t;

/* ---- Diagnostic statistics ---- */
typedef struct {
    int evidence_recorded;
    int findings_generated;
    int timeline_entries;
    int sessions_created;
    int sessions_completed;
    int snapshots_captured;
    int redactions_applied;
    int repeated_failures_detected;
} ozayn_diag_stats_t;

/* ---- Diagnostic manager ---- */
typedef struct {
    int                           enabled;
    int                           initialized;
    ozayn_diag_level_t            level;
    ozayn_evidence_t              evidence[OZAYN_DIAG_MAX_EVIDENCE];
    int                           evidence_count;
    uint32_t                      next_evidence_id;
    ozayn_diagnostic_finding_t    findings[OZAYN_DIAG_MAX_FINDINGS];
    int                           finding_count;
    uint32_t                      next_finding_id;
    ozayn_timeline_entry_t        timeline[OZAYN_DIAG_MAX_TIMELINE];
    int                           timeline_count;
    uint32_t                      next_timeline_seq;
    ozayn_diag_session_t          sessions[OZAYN_DIAG_MAX_SESSIONS];
    int                           session_count;
    uint32_t                      next_session_id;
    ozayn_diag_snapshot_t         snapshots[OZAYN_DIAG_MAX_SNAPSHOTS];
    int                           snapshot_count;
    uint32_t                      next_snapshot_id;
    ozayn_failure_freq_t          failures[OZAYN_DIAG_MAX_FAILURES];
    int                           failure_count;
    ozayn_diag_stats_t            stats;
    void                         *events;
    void                         *recovery;
    void                         *monitoring;
} ozayn_diagnostics_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_diagnostics_init(ozayn_diagnostics_manager_t *mgr, int enabled);
void           ozayn_diagnostics_shutdown(ozayn_diagnostics_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_diagnostics_set_events(ozayn_diagnostics_manager_t *mgr, void *events);
void ozayn_diagnostics_set_recovery(ozayn_diagnostics_manager_t *mgr, void *recovery);
void ozayn_diagnostics_set_monitoring(ozayn_diagnostics_manager_t *mgr, void *monitoring);

/* ---- Level control ---- */
void ozayn_diagnostics_set_level(ozayn_diagnostics_manager_t *mgr, ozayn_diag_level_t level);
ozayn_diag_level_t ozayn_diagnostics_get_level(const ozayn_diagnostics_manager_t *mgr);

/* ---- Evidence ---- */
uint32_t ozayn_diagnostics_record_evidence(ozayn_diagnostics_manager_t *mgr,
                                            ozayn_diag_component_t component,
                                            ozayn_diag_target_t target,
                                            const char *correlation_id,
                                            const char *description);
const ozayn_evidence_t *ozayn_diagnostics_get_evidence(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id);
int ozayn_diagnostics_evidence_count(const ozayn_diagnostics_manager_t *mgr);

/* ---- Findings ---- */
uint32_t ozayn_diagnostics_add_finding(ozayn_diagnostics_manager_t *mgr,
                                        ozayn_diag_component_t component,
                                        ozayn_diag_finding_sev_t severity,
                                        const char *correlation_id,
                                        const char *observation,
                                        const char *possible_cause,
                                        ozayn_diag_confidence_t confidence);
const ozayn_diagnostic_finding_t *ozayn_diagnostics_get_finding(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id);
int ozayn_diagnostics_finding_count(const ozayn_diagnostics_manager_t *mgr);

/* ---- Timeline ---- */
uint32_t ozayn_diagnostics_timeline_add(ozayn_diagnostics_manager_t *mgr,
                                         ozayn_diag_component_t component,
                                         const char *correlation_id,
                                         const char *description);
const ozayn_timeline_entry_t *ozayn_diagnostics_timeline_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t seq);
int ozayn_diagnostics_timeline_count(const ozayn_diagnostics_manager_t *mgr);
void ozayn_diagnostics_timeline_print(const ozayn_diagnostics_manager_t *mgr,
                                       const char *correlation_id);

/* ---- Sessions ---- */
uint32_t ozayn_diagnostics_session_start(ozayn_diagnostics_manager_t *mgr,
                                          ozayn_diag_target_t target,
                                          const char *correlation_id);
ozayn_result_t ozayn_diagnostics_session_set_state(
    ozayn_diagnostics_manager_t *mgr,
    uint32_t session_id,
    ozayn_diag_session_state_t state);
const ozayn_diag_session_t *ozayn_diagnostics_session_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t session_id);
int ozayn_diagnostics_session_count(const ozayn_diagnostics_manager_t *mgr);
int ozayn_diagnostics_active_session_count(const ozayn_diagnostics_manager_t *mgr);

/* ---- Snapshots ---- */
uint32_t ozayn_diagnostics_snapshot_capture(ozayn_diagnostics_manager_t *mgr, void *mon);
const ozayn_diag_snapshot_t *ozayn_diagnostics_snapshot_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id);
int ozayn_diagnostics_snapshot_count(const ozayn_diagnostics_manager_t *mgr);

/* ---- Failure tracking ---- */
ozayn_result_t ozayn_diagnostics_record_failure(ozayn_diagnostics_manager_t *mgr,
                                                  ozayn_diag_component_t component);
int ozayn_diagnostics_failure_count(const ozayn_diagnostics_manager_t *mgr,
                                     ozayn_diag_component_t component);
int ozayn_diagnostics_is_repeated_failure(const ozayn_diagnostics_manager_t *mgr,
                                           ozayn_diag_component_t component,
                                           int threshold);
void ozayn_diagnostics_print_failure_summary(const ozayn_diagnostics_manager_t *mgr);

/* ---- Query ---- */
ozayn_diag_stats_t ozayn_diagnostics_stats(const ozayn_diagnostics_manager_t *mgr);
int ozayn_diagnostics_is_enabled(const ozayn_diagnostics_manager_t *mgr);

/* ---- Auto-diagnose (event-driven) ---- */
void ozayn_diagnostics_on_task_failure(ozayn_diagnostics_manager_t *mgr,
                                        uint32_t task_id,
                                        ozayn_diag_component_t component,
                                        const char *reason);
void ozayn_diagnostics_on_resource_failure(ozayn_diagnostics_manager_t *mgr,
                                            const char *resource_name,
                                            ozayn_diag_component_t component,
                                            const char *reason);
void ozayn_diagnostics_on_health_change(ozayn_diagnostics_manager_t *mgr,
                                         ozayn_diag_component_t component,
                                         ozayn_diag_health_t old_state,
                                         ozayn_diag_health_t new_state);

/* ---- Names ---- */
const char *ozayn_diag_level_name(ozayn_diag_level_t level);
const char *ozayn_diag_session_state_name(ozayn_diag_session_state_t state);
const char *ozayn_diag_target_name(ozayn_diag_target_t target);
const char *ozayn_diag_confidence_name(ozayn_diag_confidence_t conf);
const char *ozayn_diag_finding_sev_name(ozayn_diag_finding_sev_t sev);
const char *ozayn_diag_component_name(ozayn_diag_component_t comp);
const char *ozayn_diag_health_name(ozayn_diag_health_t health);

#endif
