#ifndef OZAYN_PLUGINS_H
#define OZAYN_PLUGINS_H

#include "ozayn.h"
#include <stdint.h>

/*
 * ozayn_plugins.h — Plugin manager.
 *
 * Manages externally supplied dynamically loaded extensions.
 * Plugins are .so shared libraries discovered from a configured directory.
 * Each plugin exports a known entry point symbol returning an API table.
 *
 * Trust model: discovered → validated → loaded → initialized → started.
 * Plugins are NOT trusted by default. Every candidate is validated before load.
 */

#define OZAYN_PLUGIN_API_VERSION    1
#define OZAYN_PLUGIN_ENTRY_SYMBOL   "ozayn_plugin_entry"
#define OZAYN_MAX_PLUGINS           16
#define OZAYN_PLUGIN_ID_MAX         64
#define OZAYN_PLUGIN_NAME_MAX       128
#define OZAYN_PLUGIN_VERSION_MAX    32

/* Plugin lifecycle states */
typedef enum {
    OZAYN_PLUGIN_DISCOVERED   = 0,
    OZAYN_PLUGIN_VALIDATED    = 1,
    OZAYN_PLUGIN_LOADED       = 2,
    OZAYN_PLUGIN_INITIALIZED  = 3,
    OZAYN_PLUGIN_RUNNING      = 4,
    OZAYN_PLUGIN_STOPPING     = 5,
    OZAYN_PLUGIN_STOPPED      = 6,
    OZAYN_PLUGIN_UNLOADED     = 7,
    OZAYN_PLUGIN_INVALID      = 8,
    OZAYN_PLUGIN_INCOMPATIBLE = 9,
    OZAYN_PLUGIN_FAILED       = 10,
} ozayn_plugin_state_t;

/* Plugin metadata — returned by get_info() */
typedef struct {
    const char *id;           /* unique identifier, e.g. "camera" */
    const char *name;         /* human-readable name */
    const char *version;      /* plugin version string */
    uint32_t    api_version;  /* must equal OZAYN_PLUGIN_API_VERSION */
    const char *author;       /* author or vendor */
    const char *description;  /* short description */
} ozayn_plugin_info_t;

/* Plugin context — controlled access to Core services */
typedef struct {
    void *logger;      /* ozayn_logger_t* */
    void *events;      /* ozayn_event_engine_t* */
    void *recovery;    /* ozayn_recovery_t* */
    void *config;      /* ozayn_config_object_t* */
    void *runtime;     /* ozayn_runtime_t* */
} ozayn_plugin_context_t;

/* Plugin API table — the interface a plugin exports */
typedef struct {
    /* Metadata — called before init to identify the plugin */
    const ozayn_plugin_info_t *(*get_info)(void);

    /* Lifecycle — receive context from Plugin Manager */
    ozayn_result_t (*init)(const ozayn_plugin_context_t *ctx);
    ozayn_result_t (*start)(const ozayn_plugin_context_t *ctx);
    void           (*stop)(const ozayn_plugin_context_t *ctx);
    void           (*shutdown)(const ozayn_plugin_context_t *ctx);
} ozayn_plugin_api_t;

/* Plugin record — internal registry entry */
typedef struct {
    char                    id[OZAYN_PLUGIN_ID_MAX];
    char                    name[OZAYN_PLUGIN_NAME_MAX];
    char                    version[OZAYN_PLUGIN_VERSION_MAX];
    char                    path[512];
    ozayn_plugin_state_t    state;
    void                   *handle;      /* dlopen handle */
    ozayn_plugin_api_t     *api;         /* resolved API table */
    ozayn_plugin_info_t     info;        /* cached metadata copy */
    int                     event_sub_id; /* event subscription for cleanup */
} ozayn_plugin_record_t;

/* Plugin manager */
typedef struct {
    ozayn_plugin_record_t   plugins[OZAYN_MAX_PLUGINS];
    int                     count;
    ozayn_plugin_context_t  context;     /* shared context passed to plugins */
    int                     initialized;
} ozayn_plugin_manager_t;

/* Lifecycle */
ozayn_result_t ozayn_plugin_manager_init(ozayn_plugin_manager_t *mgr);
void           ozayn_plugin_manager_shutdown(ozayn_plugin_manager_t *mgr);

/* Engine binding */
void ozayn_plugin_manager_set_logger(ozayn_plugin_manager_t *mgr, void *logger);
void ozayn_plugin_manager_set_events(ozayn_plugin_manager_t *mgr, void *events);
void ozayn_plugin_manager_set_recovery(ozayn_plugin_manager_t *mgr, void *recovery);
void ozayn_plugin_manager_set_config(ozayn_plugin_manager_t *mgr, void *config);
void ozayn_plugin_manager_set_runtime(ozayn_plugin_manager_t *mgr, void *runtime);

/* Discovery — scan plugin directory, populate registry with candidates */
int ozayn_plugin_manager_discover(ozayn_plugin_manager_t *mgr, const char *plugin_dir);

/* Load — validate and load a single plugin by filename */
ozayn_result_t ozayn_plugin_manager_load(ozayn_plugin_manager_t *mgr, const char *filename);

/* Initialize — call init() on all LOADED plugins */
ozayn_result_t ozayn_plugin_manager_init_all(ozayn_plugin_manager_t *mgr);

/* Start — call start() on all INITIALIZED plugins */
ozayn_result_t ozayn_plugin_manager_start_all(ozayn_plugin_manager_t *mgr);

/* Stop — call stop() on all RUNNING plugins (reverse order) */
void ozayn_plugin_manager_stop_all(ozayn_plugin_manager_t *mgr);

/* Unload — shutdown + dlclose a single plugin */
ozayn_result_t ozayn_plugin_manager_unload(ozayn_plugin_manager_t *mgr, const char *id);

/* Query */
int                           ozayn_plugin_manager_count(const ozayn_plugin_manager_t *mgr);
const ozayn_plugin_record_t  *ozayn_plugin_manager_get(const ozayn_plugin_manager_t *mgr, int index);
const ozayn_plugin_record_t  *ozayn_plugin_manager_find(const ozayn_plugin_manager_t *mgr, const char *id);
int                           ozayn_plugin_manager_active_count(const ozayn_plugin_manager_t *mgr);
const char                   *ozayn_plugin_state_name(ozayn_plugin_state_t state);

#endif
