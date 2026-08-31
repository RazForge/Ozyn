#ifndef OZAYN_LIFECYCLE_H
#define OZAYN_LIFECYCLE_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * lifecycle.h — Startup & Shutdown Orchestration (Stage 22).
 *
 * Self-contained header — no circular includes.
 * Defines the Core lifecycle state machine, startup phases, shutdown reasons,
 * component registration, and the startup/shutdown coordinator.
 *
 * Startup follows dependency order through 7 phases:
 *   BOOTSTRAP → FOUNDATION → SECURITY → RUNTIME_SERVICES →
 *   MODULES_PLUGINS → MONITORING → READY
 *
 * Shutdown is reverse-order: dependents stop before dependencies.
 */

/* ---- Constants ---- */

#define OZAYN_LC_MAX_COMPONENTS  32
#define OZAYN_LC_MAX_COMPONENTS_PER_PHASE 16
#define OZAYN_LC_MAX_NAME        64

/* ---- Startup phases ---- */

typedef enum {
    OZAYN_LC_PHASE_BOOTSTRAP       = 0,  /* process startup, env detection */
    OZAYN_LC_PHASE_FOUNDATION      = 1,  /* config, logging, events */
    OZAYN_LC_PHASE_SECURITY        = 2,  /* security, authorization, boundary */
    OZAYN_LC_PHASE_RUNTIME_SERVICES = 3, /* IPC, resource, scheduler, task, process */
    OZAYN_LC_PHASE_MODULES_PLUGINS = 4,  /* module manager, plugin manager */
    OZAYN_LC_PHASE_MONITORING      = 5,  /* monitoring, diagnostics, state persistence */
    OZAYN_LC_PHASE_READY           = 6,  /* all systems operational */
    OZAYN_LC_PHASE_COUNT           = 7,
} ozayn_lc_phase_t;

/* ---- Lifecycle states ---- */

typedef enum {
    OZAYN_LC_STATE_CREATED    = 0,  /* coordinator created, nothing started */
    OZAYN_LC_STATE_STARTING   = 1,  /* startup in progress */
    OZAYN_LC_STATE_RUNNING    = 2,  /* all phases complete, ONLINE */
    OZAYN_LC_STATE_STOPPING   = 3,  /* shutdown in progress */
    OZAYN_LC_STATE_STOPPED    = 4,  /* fully stopped */
    OZAYN_LC_STATE_FAILED     = 5,  /* startup or shutdown failed */
} ozayn_lc_state_t;

/* ---- Shutdown reasons ---- */

typedef enum {
    OZAYN_LC_SHUTDOWN_NORMAL          = 0,  /* clean shutdown */
    OZAYN_LC_SHUTDOWN_USER_REQUEST    = 1,  /* SIGINT / user command */
    OZAYN_LC_SHUTDOWN_SYSTEM          = 2,  /* SIGTERM / system event */
    OZAYN_LC_SHUTDOWN_CONFIG_CHANGE   = 3,  /* configuration reload needed */
    OZAYN_LC_SHUTDOWN_FATAL_ERROR     = 4,  /* unrecoverable error */
    OZAYN_LC_SHUTDOWN_SECURITY        = 5,  /* security violation */
    OZAYN_LC_SHUTDOWN_STARTUP_FAIL    = 6,  /* startup failed, rolling back */
    OZAYN_LC_SHUTDOWN_RESTART         = 7,  /* controlled restart */
} ozayn_lc_shutdown_reason_t;

/* ---- Component criticality ---- */

typedef enum {
    OZAYN_LC_CRITICALITY_REQUIRED  = 0,  /* failure = startup abort */
    OZAYN_LC_CRITICALITY_OPTIONAL  = 1,  /* failure = disable and continue */
    OZAYN_LC_CRITICALITY_BEST_EFFORT = 2, /* failure = ignore */
} ozayn_lc_criticality_t;

/* ---- Component state ---- */

typedef enum {
    OZAYN_LC_COMP_CREATED      = 0,
    OZAYN_LC_COMP_INITIALIZING = 1,
    OZAYN_LC_COMP_INITIALIZED  = 2,
    OZAYN_LC_COMP_STARTING     = 3,
    OZAYN_LC_COMP_STARTED      = 4,
    OZAYN_LC_COMP_STOPPING     = 5,
    OZAYN_LC_COMP_STOPPED      = 6,
    OZAYN_LC_COMP_FAILED       = 7,
    OZAYN_LC_COMP_DISABLED     = 8,
} ozayn_lc_component_state_t;

/* ---- Callback types ---- */

typedef int  (*ozayn_lc_init_fn)(void *context);
typedef int  (*ozayn_lc_start_fn)(void *context);
typedef void (*ozayn_lc_stop_fn)(void *context);
typedef void (*ozayn_lc_shutdown_fn)(void *context);

typedef void (*ozayn_lc_phase_hook_fn)(ozayn_lc_phase_t phase, void *context);
typedef void (*ozayn_lc_state_hook_fn)(ozayn_lc_state_t state, void *context);
typedef void (*ozayn_lc_readiness_hook_fn)(int passed, void *context);

/* ---- Component descriptor ---- */

typedef struct {
    int                          active;
    char                         name[OZAYN_LC_MAX_NAME];
    ozayn_lc_phase_t             phase;
    ozayn_lc_criticality_t       criticality;
    ozayn_lc_component_state_t   state;
    ozayn_lc_init_fn             init_fn;
    ozayn_lc_start_fn            start_fn;
    ozayn_lc_stop_fn             stop_fn;
    ozayn_lc_shutdown_fn         shutdown_fn;
    void                        *context;
    time_t                       init_time;
    time_t                       start_time;
    time_t                       stop_time;
    int                          init_result;
} ozayn_lc_component_t;

/* ---- Lifecycle configuration ---- */

typedef struct {
    int startup_timeout_ms;     /* max time for entire startup */
    int component_timeout_ms;   /* max time per component init/start */
    int shutdown_timeout_ms;    /* max time for entire shutdown */
} ozayn_lc_config_t;

/* ---- Lifecycle coordinator ---- */

typedef struct {
    /* State machine */
    ozayn_lc_state_t             state;
    ozayn_lc_shutdown_reason_t   shutdown_reason;
    ozayn_lc_phase_t             current_phase;

    /* Timestamps */
    time_t                       startup_time;
    time_t                       ready_time;
    time_t                       shutdown_time;
    time_t                       stopped_time;

    /* Components */
    ozayn_lc_component_t         components[OZAYN_LC_MAX_COMPONENTS];
    int                          component_count;

    /* Init order tracking */
    int                          init_order[OZAYN_LC_MAX_COMPONENTS];
    int                          init_order_count;

    /* Shutdown order tracking */
    int                          shutdown_order[OZAYN_LC_MAX_COMPONENTS];
    int                          shutdown_order_count;

    /* Configuration */
    ozayn_lc_config_t            config;

    /* Statistics */
    int                          components_initialized;
    int                          components_started;
    int                          components_failed;
    int                          components_disabled;
    int                          startup_retries;

    /* External dependencies (void* to avoid circular includes) */
    void                        *runtime;
    void                        *events;
    void                        *recovery;

    /* State */
    int                          initialized;
} ozayn_lc_t;

/* ---- Component descriptor macro ---- */

#define OZAYN_LC_COMPONENT(n, p, crit, init, start, stop, shut, ctx) \
    { .active = 1, .name = (n), .phase = (p), .criticality = (crit), \
      .state = OZAYN_LC_COMP_CREATED, .init_fn = (init), \
      .start_fn = (start), .stop_fn = (stop), .shutdown_fn = (shut), \
      .context = (ctx) }

/* ---- Lifecycle ---- */

int  ozayn_lc_init(ozayn_lc_t *lc, const ozayn_lc_config_t *cfg);
void ozayn_lc_shutdown(ozayn_lc_t *lc);

/* ---- Binding ---- */

void ozayn_lc_set_runtime(ozayn_lc_t *lc, void *runtime);
void ozayn_lc_set_events(ozayn_lc_t *lc, void *events);
void ozayn_lc_set_recovery(ozayn_lc_t *lc, void *recovery);

/* ---- Component registration ---- */

int  ozayn_lc_register(ozayn_lc_t *lc, const ozayn_lc_component_t *comp);
int  ozayn_lc_register_simple(ozayn_lc_t *lc,
                              const char *name,
                              ozayn_lc_phase_t phase,
                              ozayn_lc_criticality_t criticality,
                              ozayn_lc_init_fn init_fn,
                              void *context);

/* ---- Startup ---- */

int  ozayn_lc_startup(ozayn_lc_t *lc);

/* ---- Shutdown ---- */

int  ozayn_lc_request_shutdown(ozayn_lc_t *lc, ozayn_lc_shutdown_reason_t reason);
int  ozayn_lc_perform_shutdown(ozayn_lc_t *lc);

/* ---- Restart ---- */

int  ozayn_lc_restart(ozayn_lc_t *lc);

/* ---- Query ---- */

ozayn_lc_phase_t       ozayn_lc_get_phase(const ozayn_lc_t *lc);
ozayn_lc_state_t       ozayn_lc_get_state(const ozayn_lc_t *lc);
int                    ozayn_lc_is_running(const ozayn_lc_t *lc);
ozayn_lc_shutdown_reason_t ozayn_lc_get_shutdown_reason(const ozayn_lc_t *lc);
const ozayn_lc_component_t *ozayn_lc_get_component(const ozayn_lc_t *lc, const char *name);
const ozayn_lc_component_t *ozayn_lc_get_component_by_index(const ozayn_lc_t *lc, int index);
int                    ozayn_lc_component_count(const ozayn_lc_t *lc);
int                    ozayn_lc_component_count_by_phase(const ozayn_lc_t *lc, ozayn_lc_phase_t phase);
int                    ozayn_lc_component_count_by_state(const ozayn_lc_t *lc, ozayn_lc_component_state_t state);
int                    ozayn_lc_component_count_by_criticality(const ozayn_lc_t *lc, ozayn_lc_criticality_t crit);

/* ---- Readiness check ---- */

int  ozayn_lc_readiness_check(ozayn_lc_t *lc);

/* ---- Event type constants (defined in events.h as OZAYN_LC_EVENT_*) ---- */
/* OZAYN_LC_EVENT_INIT_BEGAN            = 111 */
/* OZAYN_LC_EVENT_INIT_PHASE_COMPLETE   = 112 */
/* OZAYN_LC_EVENT_INIT_COMPLETE         = 113 */
/* OZAYN_LC_EVENT_ONLINE                = 114 */
/* OZAYN_LC_EVENT_SHUTDOWN_REQUESTED    = 115 */
/* OZAYN_LC_EVENT_SHUTDOWN_BEGAN        = 116 */
/* OZAYN_LC_EVENT_SHUTDOWN_COMPLETED    = 117 */
/* OZAYN_LC_EVENT_RESTART_REQUESTED     = 118 */
/* OZAYN_LC_EVENT_COMPONENT_FAILED      = 119 */
/* OZAYN_LC_EVENT_STARTUP_ROLLBACK      = 120 */
/* OZAYN_LC_EVENT_READINESS_PASSED      = 121 */
/* OZAYN_LC_EVENT_READINESS_FAILED      = 122 */
/* OZAYN_LC_EVENT_STATE_CHANGED         = 123 */

/* ---- Name helpers ---- */

const char *ozayn_lc_phase_name(ozayn_lc_phase_t phase);
const char *ozayn_lc_state_name(ozayn_lc_state_t state);
const char *ozayn_lc_shutdown_reason_name(ozayn_lc_shutdown_reason_t reason);
const char *ozayn_lc_criticality_name(ozayn_lc_criticality_t crit);
const char *ozayn_lc_component_state_name(ozayn_lc_component_state_t state);

/* ---- Print ---- */

void ozayn_lc_print_status(const ozayn_lc_t *lc);
void ozayn_lc_print_components(const ozayn_lc_t *lc);
void ozayn_lc_print_startup_log(const ozayn_lc_t *lc);

#endif
