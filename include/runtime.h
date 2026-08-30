#ifndef OZAYN_RUNTIME_H
#define OZAYN_RUNTIME_H

#include "ozayn.h"
#include <signal.h>

/*
 * ozayn_runtime.h — Runtime lifecycle management.
 *
 * The Runtime is the continuous execution environment of OZAYN.
 * It owns the state machine that keeps OZAYN alive and coordinates shutdown.
 */

/* Forward declarations */
typedef struct ozayn_config_s ozayn_config_t;
typedef struct ozayn_event_engine_s ozayn_event_engine_t;

/* Runtime states */
typedef enum {
    OZAYN_STATE_CREATED       = 0,
    OZAYN_STATE_INITIALIZING  = 1,
    OZAYN_STATE_RUNNING       = 2,
    OZAYN_STATE_STOPPING      = 3,
    OZAYN_STATE_STOPPED       = 4,
    OZAYN_STATE_FAILED        = 5,
} ozayn_state_t;

/* Runtime object */
typedef struct ozayn_runtime_s {
    ozayn_state_t state;
    int           should_stop;
    volatile sig_atomic_t *stop_flag;
    const ozayn_config_t  *config;
    ozayn_event_engine_t  *events;    /* event engine */
    void                  *process_mgr; /* process manager (void* to avoid circular include) */
    void                  *module_mgr;  /* module manager (void* to avoid circular include) */
    void                  *plugin_mgr;  /* plugin manager (void* to avoid circular include) */
    void                  *ipc_mgr;     /* IPC manager (void* to avoid circular include) */
    void                  *registry_mgr; /* service registry (void* to avoid circular include) */
    void                  *security_mgr; /* security manager (void* to avoid circular include) */
    void                  *authorization_mgr; /* authorization manager */
    void                  *resource_mgr;      /* resource manager */
    void                  *scheduler_mgr;     /* scheduler manager */
    void                  *monitoring_mgr;    /* monitoring manager */
    void                  *diagnostics_mgr;   /* diagnostics manager */
    void                  *security_boundary_mgr; /* security boundary manager */
    ozayn_core_t  core;
} ozayn_runtime_t;

/* Lifecycle */
ozayn_runtime_t *ozayn_runtime_create(void);
ozayn_result_t   ozayn_runtime_init(ozayn_runtime_t *rt);
ozayn_result_t   ozayn_runtime_run(ozayn_runtime_t *rt);
void             ozayn_runtime_shutdown(ozayn_runtime_t *rt);
void             ozayn_runtime_destroy(ozayn_runtime_t *rt);

/* Stop flag — set from signal handler */
void ozayn_runtime_set_stop_flag(ozayn_runtime_t *rt, volatile sig_atomic_t *flag);

/* Configuration binding */
void ozayn_runtime_set_config(ozayn_runtime_t *rt, const ozayn_config_t *cfg);

/* Event engine binding */
void ozayn_runtime_set_events(ozayn_runtime_t *rt, ozayn_event_engine_t *events);

/* Process manager binding */
void ozayn_runtime_set_process_mgr(ozayn_runtime_t *rt, void *process_mgr);

/* Module manager binding */
void ozayn_runtime_set_module_mgr(ozayn_runtime_t *rt, void *module_mgr);

/* Plugin manager binding */
void ozayn_runtime_set_plugin_mgr(ozayn_runtime_t *rt, void *plugin_mgr);

/* IPC manager binding */
void ozayn_runtime_set_ipc_mgr(ozayn_runtime_t *rt, void *ipc_mgr);

/* Service registry binding */
void ozayn_runtime_set_registry_mgr(ozayn_runtime_t *rt, void *registry_mgr);

/* Security manager binding */
void ozayn_runtime_set_security_mgr(ozayn_runtime_t *rt, void *security_mgr);

/* Authorization manager binding */
void ozayn_runtime_set_authorization_mgr(ozayn_runtime_t *rt, void *authorization_mgr);

/* Resource manager binding */
void ozayn_runtime_set_resource_mgr(ozayn_runtime_t *rt, void *resource_mgr);

/* Scheduler manager binding */
void ozayn_runtime_set_scheduler_mgr(ozayn_runtime_t *rt, void *scheduler_mgr);

/* Monitoring manager binding */
void ozayn_runtime_set_monitoring_mgr(ozayn_runtime_t *rt, void *monitoring_mgr);

/* Diagnostics manager binding */
void ozayn_runtime_set_diagnostics_mgr(ozayn_runtime_t *rt, void *diagnostics_mgr);

/* Security boundary manager binding */
void ozayn_runtime_set_security_boundary_mgr(ozayn_runtime_t *rt, void *security_boundary_mgr);

/* Stop request — safe to call from command handlers */
void ozayn_runtime_request_stop(ozayn_runtime_t *rt);

/* Query */
const char      *ozayn_state_name(ozayn_state_t state);
int              ozayn_runtime_is_running(const ozayn_runtime_t *rt);

#endif
