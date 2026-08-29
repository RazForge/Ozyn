#ifndef OZAYN_MODULES_H
#define OZAYN_MODULES_H

#include "ozayn.h"

/*
 * ozayn_modules.h — Module manager.
 *
 * Manages in-process OZAYN subsystem lifecycle.
 * Each module implements a standard interface (init/start/stop/shutdown).
 * Registration order = init order; shutdown runs in reverse.
 */

#define OZAYN_MAX_MODULES 16
#define OZAYN_MODULE_NAME_MAX 64

/* Module lifecycle states */
typedef enum {
    OZAYN_MOD_REGISTERED  = 0,
    OZAYN_MOD_INITIALIZED = 1,
    OZAYN_MOD_RUNNING     = 2,
    OZAYN_MOD_STOPPED     = 3,
    OZAYN_MOD_SHUTDOWN    = 4,
    OZAYN_MOD_FAILED      = 5,
} ozayn_module_state_t;

/* Module interface (vtable) */
typedef struct {
    const char *name;
    const char *version;
    const char *description;

    ozayn_result_t (*init)(void *engine);
    ozayn_result_t (*start)(void *engine);
    void           (*stop)(void *engine);
    void           (*shutdown)(void *engine);
} ozayn_module_entry_t;

/* Module record — entry + runtime state */
typedef struct {
    ozayn_module_entry_t  entry;
    ozayn_module_state_t  state;
    void                 *userdata;
} ozayn_module_record_t;

/* Engine pointer bundle — passed to module lifecycle functions */
typedef struct {
    void *logger;      /* ozayn_logger_t* */
    void *events;      /* ozayn_event_engine_t* */
    void *recovery;    /* ozayn_recovery_t* */
    void *config;      /* ozayn_config_object_t* */
    void *runtime;     /* ozayn_runtime_t* */
} ozayn_module_engine_t;

/* Module manager */
typedef struct {
    ozayn_module_record_t modules[OZAYN_MAX_MODULES];
    int                   count;
    ozayn_module_engine_t engine;
    int                   initialized;
} ozayn_module_manager_t;

/* Lifecycle */
ozayn_result_t ozayn_module_manager_init(ozayn_module_manager_t *mgr);
void           ozayn_module_manager_shutdown(ozayn_module_manager_t *mgr);

/* Engine binding */
void ozayn_module_manager_set_logger(ozayn_module_manager_t *mgr, void *logger);
void ozayn_module_manager_set_events(ozayn_module_manager_t *mgr, void *events);
void ozayn_module_manager_set_recovery(ozayn_module_manager_t *mgr, void *recovery);
void ozayn_module_manager_set_config(ozayn_module_manager_t *mgr, void *config);
void ozayn_module_manager_set_runtime(ozayn_module_manager_t *mgr, void *runtime);

/* Registration */
ozayn_result_t ozayn_module_manager_register(ozayn_module_manager_t *mgr,
                                              const ozayn_module_entry_t *entry);
ozayn_result_t ozayn_module_manager_unregister(ozayn_module_manager_t *mgr,
                                                const char *name);

/* Batch lifecycle */
ozayn_result_t ozayn_module_manager_init_all(ozayn_module_manager_t *mgr);
ozayn_result_t ozayn_module_manager_start_all(ozayn_module_manager_t *mgr);
void           ozayn_module_manager_stop_all(ozayn_module_manager_t *mgr);

/* Query */
int                      ozayn_module_manager_count(const ozayn_module_manager_t *mgr);
const ozayn_module_record_t *ozayn_module_manager_get(const ozayn_module_manager_t *mgr,
                                                       int index);
const ozayn_module_record_t *ozayn_module_manager_find(const ozayn_module_manager_t *mgr,
                                                       const char *name);
int                      ozayn_module_manager_active_count(const ozayn_module_manager_t *mgr);
const char              *ozayn_module_state_name(ozayn_module_state_t state);

#endif
