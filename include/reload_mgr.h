#ifndef OZAYN_RELOAD_MGR_H
#define OZAYN_RELOAD_MGR_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * reload_mgr.h — Runtime Hot Reloading (Stage 26).
 *
 * Self-contained header — no circular includes.
 * Coordinates safe hot-reload of eligible components while Core stays alive.
 *
 * Responsibilities:
 *   - Declare components as reloadable / non-reloadable / restart-required
 *   - Reload state machine (15 states + rollback)
 *   - Quiescing: stop new work, drain active work
 *   - State export / import / validation
 *   - Version compatibility checking
 *   - Dependency-aware reload (quiesce dependents first)
 *   - Rollback on failure
 *   - Timeout enforcement
 *   - Reload audit trail
 *   - Integration with Event Engine, Dependency Manager, Lifecycle,
 *     Service Lifecycle, Config Manager, Security, Monitoring, Diagnostics
 */

/* ---- Constants ---- */

#define OZAYN_RELOAD_MAX_COMPONENTS     64
#define OZAYN_RELOAD_MAX_NAME           64
#define OZAYN_RELOAD_MAX_VERSION        32
#define OZAYN_RELOAD_MAX_DEPENDENTS     16
#define OZAYN_RELOAD_MAX_STATE_SIZE     4096   /* max state blob per component */
#define OZAYN_RELOAD_MAX_AUDIT          128
#define OZAYN_RELOAD_MAX_LISTENERS      8

/* ---- Reload capability ---- */

typedef enum {
    OZAYN_RELOAD_SUPPORTED       = 0,  /* can be hot-reloaded */
    OZAYN_RELOAD_UNSUPPORTED     = 1,  /* cannot be reloaded */
    OZAYN_RELOAD_RESTART_REQUIRED = 2, /* changes require full restart */
} ozayn_reload_capability_t;

/* ---- Reload state machine ---- */

typedef enum {
    OZAYN_RELOAD_STATE_IDLE                  = 0,
    OZAYN_RELOAD_STATE_REQUESTED             = 1,
    OZAYN_RELOAD_STATE_VALIDATING            = 2,
    OZAYN_RELOAD_STATE_QUIESCING             = 3,
    OZAYN_RELOAD_STATE_STOPPING              = 4,
    OZAYN_RELOAD_STATE_STATE_SAVED           = 5,
    OZAYN_RELOAD_STATE_UNLOADING             = 6,
    OZAYN_RELOAD_STATE_LOADING               = 7,
    OZAYN_RELOAD_STATE_INITIALIZING          = 8,
    OZAYN_RELOAD_STATE_STATE_RESTORED        = 9,
    OZAYN_RELOAD_STATE_STARTING              = 10,
    OZAYN_RELOAD_STATE_HEALTH_CHECK          = 11,
    OZAYN_RELOAD_STATE_READY                 = 12,
    OZAYN_RELOAD_STATE_COMPLETED             = 13,
    OZAYN_RELOAD_STATE_ROLLBACK              = 14,
    OZAYN_RELOAD_STATE_ROLLBACK_COMPLETE     = 15,
    OZAYN_RELOAD_STATE_FAILED                = 16,
} ozayn_reload_state_t;

/* ---- Reload result ---- */

typedef enum {
    OZAYN_RELOAD_RESULT_OK               = 0,
    OZAYN_RELOAD_RESULT_NOT_RELOADABLE   = 1,
    OZAYN_RELOAD_RESULT_AUTH_DENIED      = 2,
    OZAYN_RELOAD_RESULT_INCOMPATIBLE     = 3,
    OZAYN_RELOAD_RESULT_DEP_FAILURE      = 4,
    OZAYN_RELOAD_RESULT_QUIESCE_TIMEOUT  = 5,
    OZAYN_RELOAD_RESULT_STATE_SAVE_FAIL  = 6,
    OZAYN_RELOAD_RESULT_LOAD_FAIL        = 7,
    OZAYN_RELOAD_RESULT_INIT_FAIL        = 8,
    OZAYN_RELOAD_RESULT_STATE_RESTORE_FAIL = 9,
    OZAYN_RELOAD_RESULT_HEALTH_FAIL      = 10,
    OZAYN_RELOAD_RESULT_ROLLBACK_FAIL    = 11,
    OZAYN_RELOAD_RESULT_ABORTED          = 12,
    OZAYN_RELOAD_RESULT_TIMEOUT          = 13,
} ozayn_reload_result_t;

/* ---- Reloadable component descriptor ---- */

typedef struct {
    int                              active;
    char                             name[OZAYN_RELOAD_MAX_NAME];
    char                             current_version[OZAYN_RELOAD_MAX_VERSION];
    ozayn_reload_capability_t        capability;
    int                              required;       /* 1=critical, 0=optional */
    uint32_t                         active_requests; /* in-flight work count */
    uint32_t                         max_concurrent;  /* max concurrent reloads */
    /* State blob */
    void                            *state_data;
    uint32_t                         state_size;
    uint32_t                         state_version;
    /* Version info for incoming */
    char                             pending_version[OZAYN_RELOAD_MAX_VERSION];
    /* Dependent tracking */
    char                             dependents[OZAYN_RELOAD_MAX_DEPENDENTS][OZAYN_RELOAD_MAX_NAME];
    uint32_t                         dependent_count;
} ozayn_reload_component_t;

/* ---- Reload audit entry ---- */

typedef struct {
    char                             component[OZAYN_RELOAD_MAX_NAME];
    char                             old_version[OZAYN_RELOAD_MAX_VERSION];
    char                             new_version[OZAYN_RELOAD_MAX_VERSION];
    ozayn_reload_result_t            result;
    ozayn_reload_state_t             final_state;
    time_t                           requested_at;
    time_t                           completed_at;
    uint32_t                         duration_ms;
    char                             requester[OZAYN_RELOAD_MAX_NAME];
    char                             reason[128];
} ozayn_reload_audit_t;

/* ---- Reload statistics ---- */

typedef struct {
    uint32_t total_requests;
    uint32_t total_succeeded;
    uint32_t total_failed;
    uint32_t total_rollback;
    uint32_t total_timeout;
    uint32_t current_reloading;
    uint32_t reloadable_count;
    uint32_t non_reloadable_count;
    uint32_t restart_required_count;
} ozayn_reload_stats_t;

/* ---- Reload configuration ---- */

typedef struct {
    uint32_t quiesce_timeout_ms;    /* max time to quiesce */
    uint32_t load_timeout_ms;       /* max time to load/init */
    uint32_t health_check_timeout_ms; /* max time for health check */
    uint32_t rollback_on_fail;      /* 1=auto-rollback, 0=leave failed */
    uint32_t max_concurrent;        /* max simultaneous reloads */
} ozayn_reload_config_t;

/* ---- Forward declarations for events ---- */
typedef struct ozayn_event_engine_s ozayn_event_engine_t;

/* ---- Manager ---- */

typedef struct {
    ozayn_reload_config_t           config;
    ozayn_reload_component_t        components[OZAYN_RELOAD_MAX_COMPONENTS];
    uint32_t                        component_count;
    /* Active reload */
    int                             reload_active;
    char                            active_component[OZAYN_RELOAD_MAX_NAME];
    ozayn_reload_state_t            active_state;
    time_t                          reload_started_at;
    uint32_t                        active_reload_count;
    /* Audit trail */
    ozayn_reload_audit_t            audit[OZAYN_RELOAD_MAX_AUDIT];
    uint32_t                        audit_head;
    uint32_t                        audit_count;
    /* Stats */
    uint32_t                        total_requests;
    uint32_t                        total_succeeded;
    uint32_t                        total_failed;
    uint32_t                        total_rollback;
    uint32_t                        total_timeout;
    /* Events */
    ozayn_event_engine_t           *events;
} ozayn_reload_mgr_t;

/* ================================================================
 * API
 * ================================================================ */

/* Lifecycle */
int  ozayn_reload_mgr_init(ozayn_reload_mgr_t *mgr, const ozayn_reload_config_t *config);
void ozayn_reload_mgr_shutdown(ozayn_reload_mgr_t *mgr);
void ozayn_reload_mgr_set_events(ozayn_reload_mgr_t *mgr, ozayn_event_engine_t *events);

/* Component registration */
int  ozayn_reload_register(ozayn_reload_mgr_t *mgr, const char *name,
                            const char *version,
                            ozayn_reload_capability_t capability,
                            int required);
int  ozayn_reload_unregister(ozayn_reload_mgr_t *mgr, const char *name);

/* Reload request and execution */
int  ozayn_reload_request(ozayn_reload_mgr_t *mgr, const char *component,
                           const char *new_version,
                           const char *requester,
                           const char *reason);
int  ozayn_reload_cancel(ozayn_reload_mgr_t *mgr);
int  ozayn_reload_rollback(ozayn_reload_mgr_t *mgr);

/* State machine tick (call periodically or on events) */
int  ozayn_reload_tick(ozayn_reload_mgr_t *mgr);

/* Active work tracking */
int  ozayn_reload_request_begin(ozayn_reload_mgr_t *mgr, const char *component);
int  ozayn_reload_request_end(ozayn_reload_mgr_t *mgr, const char *component);
uint32_t ozayn_reload_active_requests(ozayn_reload_mgr_t *mgr, const char *component);

/* State save/load (implementations provide callbacks) */
int  ozayn_reload_save_state(ozayn_reload_mgr_t *mgr, const char *component,
                              const void *data, uint32_t size, uint32_t version);
int  ozayn_reload_load_state(ozayn_reload_mgr_t *mgr, const char *component,
                              void *out, uint32_t max_size, uint32_t *out_version);
int  ozayn_reload_clear_state(ozayn_reload_mgr_t *mgr, const char *component);

/* Version checking */
int  ozayn_reload_check_version(ozayn_reload_mgr_t *mgr, const char *component,
                                 const char *new_version);

/* Queries */
ozayn_reload_state_t ozayn_reload_get_state(ozayn_reload_mgr_t *mgr, const char *name);
int  ozayn_reload_is_reloadable(ozayn_reload_mgr_t *mgr, const char *name);
int  ozayn_reload_is_busy(ozayn_reload_mgr_t *mgr);
int  ozayn_reload_can_quiesce(ozayn_reload_mgr_t *mgr, const char *name);
const ozayn_reload_component_t *ozayn_reload_find(ozayn_reload_mgr_t *mgr, const char *name);
const char *ozayn_reload_active_component(ozayn_reload_mgr_t *mgr);

/* Audit trail */
uint32_t ozayn_reload_audit_count(ozayn_reload_mgr_t *mgr);
const ozayn_reload_audit_t *ozayn_reload_get_audit(ozayn_reload_mgr_t *mgr, uint32_t index);

/* Stats */
ozayn_reload_stats_t ozayn_reload_stats(ozayn_reload_mgr_t *mgr);

/* Print / debug */
void ozayn_reload_print_components(ozayn_reload_mgr_t *mgr);
void ozayn_reload_print_status(ozayn_reload_mgr_t *mgr);
void ozayn_reload_print_audit(ozayn_reload_mgr_t *mgr);
void ozayn_reload_print_audit_entry(ozayn_reload_mgr_t *mgr, uint32_t index);

/* Names */
const char *ozayn_reload_state_name(ozayn_reload_state_t state);
const char *ozayn_reload_result_name(ozayn_reload_result_t result);
const char *ozayn_reload_capability_name(ozayn_reload_capability_t cap);

#endif /* OZAYN_RELOAD_MGR_H */
