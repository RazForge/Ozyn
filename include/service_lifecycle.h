#ifndef OZAYN_SERVICE_LIFECYCLE_H
#define OZAYN_SERVICE_LIFECYCLE_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * service_lifecycle.h — Service Lifecycle Management (Stage 24).
 *
 * Self-contained header — no circular includes.
 * Manages the full lifecycle of individual services.
 *
 * Responsibilities:
 *   - Service state machine (REGISTERED → INITIALIZING → RUNNING → DRAINING → STOPPED)
 *   - Restart policies (never, on-failure, always)
 *   - Health check integration (heartbeat, timeout)
 *   - Graceful shutdown with drain timeout
 *   - Service versioning
 *   - Service groups (start/stop groups of services together)
 *   - Restart budget (max restarts within a time window)
 *   - Failure tracking and circuit-breaker
 */

/* ---- Constants ---- */

#define OZAYN_SVC_MAX_SERVICES    64
#define OZAYN_SVC_MAX_NAME        64
#define OZAYN_SVC_MAX_VERSION     32
#define OZAYN_SVC_MAX_GROUPS      16
#define OZAYN_SVC_MAX_GROUP_NAME  64
#define OZAYN_SVC_MAX_PER_GROUP   16
#define OZAYN_SVC_MAX_RESTART_WINDOW_MS  60000  /* 60-second window for restart budget */

/* ---- Service lifecycle states ---- */

typedef enum {
    OZAYN_SVC_STATE_UNREGISTERED = 0,
    OZAYN_SVC_STATE_REGISTERED   = 1,
    OZAYN_SVC_STATE_INITIALIZING = 2,
    OZAYN_SVC_STATE_RUNNING      = 3,
    OZAYN_SVC_STATE_DRAINING     = 4,   /* graceful shutdown in progress */
    OZAYN_SVC_STATE_STOPPING     = 5,
    OZAYN_SVC_STATE_STOPPED      = 6,
    OZAYN_SVC_STATE_FAILED       = 7,
    OZAYN_SVC_STATE_RESTARTING   = 8,
    OZAYN_SVC_STATE_SUSPENDED    = 9,   /* temporarily paused */
} ozayn_svc_state_t;

/* ---- Restart policy ---- */

typedef enum {
    OZAYN_SVC_RESTART_NEVER       = 0,  /* do not restart */
    OZAYN_SVC_RESTART_ON_FAILURE  = 1,  /* restart only on unexpected failure */
    OZAYN_SVC_RESTART_ALWAYS      = 2,  /* always restart after stop */
} ozayn_svc_restart_policy_t;

/* ---- Service health status ---- */

typedef enum {
    OZAYN_SVC_HEALTH_UNKNOWN   = 0,
    OZAYN_SVC_HEALTH_HEALTHY   = 1,
    OZAYN_SVC_HEALTH_DEGRADED  = 2,
    OZAYN_SVC_HEALTH_UNHEALTHY = 3,
} ozayn_svc_health_t;

/* ---- Service configuration (at registration) ---- */

typedef struct {
    const char                  *name;
    const char                  *version;
    ozayn_svc_restart_policy_t   restart_policy;
    uint32_t                     max_restarts;        /* max restarts within window (0=unlimited) */
    uint32_t                     restart_window_ms;   /* time window for restart budget */
    uint32_t                     drain_timeout_ms;    /* max time to wait for graceful drain */
    uint32_t                     health_check_ms;     /* health check interval (0=disabled) */
    int                          required;            /* 1=critical, 0=optional */
} ozayn_svc_config_t;

/* ---- Service record ---- */

typedef struct {
    int                          active;
    char                         name[OZAYN_SVC_MAX_NAME];
    char                         version[OZAYN_SVC_MAX_VERSION];
    ozayn_svc_state_t            state;
    ozayn_svc_restart_policy_t   restart_policy;
    ozayn_svc_health_t           health;
    int                          required;
    uint32_t                     max_restarts;
    uint32_t                     restart_window_ms;
    uint32_t                     drain_timeout_ms;
    uint32_t                     health_check_ms;
    uint32_t                     restart_count;
    uint32_t                     failure_count;
    time_t                       started_at;
    time_t                       stopped_at;
    time_t                       last_health_check;
    time_t                       first_failure_in_window;
    uint32_t                     failures_in_window;
} ozayn_svc_record_t;

/* ---- Service group ---- */

typedef struct {
    int      active;
    char     name[OZAYN_SVC_MAX_GROUP_NAME];
    uint32_t service_count;
    char     services[OZAYN_SVC_MAX_PER_GROUP][OZAYN_SVC_MAX_NAME];
} ozayn_svc_group_t;

/* ---- Stats ---- */

typedef struct {
    uint32_t total_services;
    uint32_t running;
    uint32_t stopped;
    uint32_t failed;
    uint32_t total_restarts;
    uint32_t total_failures;
    uint32_t total_groups;
} ozayn_svc_lc_stats_t;

/* ---- Manager configuration ---- */

typedef struct {
    uint32_t max_services;
    uint32_t max_groups;
} ozayn_svc_lc_config_t;

/* ---- Manager ---- */

typedef struct {
    ozayn_svc_lc_config_t   config;
    ozayn_svc_record_t      services[OZAYN_SVC_MAX_SERVICES];
    ozayn_svc_group_t       groups[OZAYN_SVC_MAX_GROUPS];
    uint32_t                total_restarts;
    uint32_t                total_failures;
    void                   *events;
} ozayn_svc_lc_manager_t;

/* ================================================================
 * API
 * ================================================================ */

/* Lifecycle */
int  ozayn_svc_lc_init(ozayn_svc_lc_manager_t *mgr, const ozayn_svc_lc_config_t *config);
void ozayn_svc_lc_shutdown(ozayn_svc_lc_manager_t *mgr);
void ozayn_svc_lc_set_events(ozayn_svc_lc_manager_t *mgr, void *events);

/* Registration */
int  ozayn_svc_lc_register(ozayn_svc_lc_manager_t *mgr, const ozayn_svc_config_t *config);
int  ozayn_svc_lc_unregister(ozayn_svc_lc_manager_t *mgr, const char *name);

/* State transitions */
int  ozayn_svc_lc_start(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_stop(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_restart(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_suspend(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_resume(ozayn_svc_lc_manager_t *mgr, const char *name);

/* Health */
int  ozayn_svc_lc_set_health(ozayn_svc_lc_manager_t *mgr, const char *name,
                              ozayn_svc_health_t health);
int  ozayn_svc_lc_check_health(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_check_all(ozayn_svc_lc_manager_t *mgr);

/* Groups */
int  ozayn_svc_lc_group_create(ozayn_svc_lc_manager_t *mgr, const char *group_name);
int  ozayn_svc_lc_group_add(ozayn_svc_lc_manager_t *mgr, const char *group_name,
                             const char *service_name);
int  ozayn_svc_lc_group_start(ozayn_svc_lc_manager_t *mgr, const char *group_name);
int  ozayn_svc_lc_group_stop(ozayn_svc_lc_manager_t *mgr, const char *group_name);

/* Queries */
const ozayn_svc_record_t *ozayn_svc_lc_find(ozayn_svc_lc_manager_t *mgr, const char *name);
ozayn_svc_state_t  ozayn_svc_lc_get_state(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_is_running(ozayn_svc_lc_manager_t *mgr, const char *name);
int  ozayn_svc_lc_can_restart(ozayn_svc_lc_manager_t *mgr, const char *name);
uint32_t ozayn_svc_lc_running_count(ozayn_svc_lc_manager_t *mgr);
uint32_t ozayn_svc_lc_failed_count(ozayn_svc_lc_manager_t *mgr);

/* Stats */
ozayn_svc_lc_stats_t ozayn_svc_lc_stats(ozayn_svc_lc_manager_t *mgr);

/* Print / debug */
void ozayn_svc_lc_print_status(ozayn_svc_lc_manager_t *mgr);
void ozayn_svc_lc_print_service(ozayn_svc_lc_manager_t *mgr, const char *name);
void ozayn_svc_lc_print_groups(ozayn_svc_lc_manager_t *mgr);

/* Names */
const char *ozayn_svc_state_name(ozayn_svc_state_t state);
const char *ozayn_svc_restart_policy_name(ozayn_svc_restart_policy_t policy);
const char *ozayn_svc_health_name(ozayn_svc_health_t health);

#endif /* OZAYN_SERVICE_LIFECYCLE_H */
