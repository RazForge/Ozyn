#ifndef OZAYN_CONFIG_MGR_H
#define OZAYN_CONFIG_MGR_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * config_mgr.h — Configuration Management & Hot-Reload (Stage 25).
 *
 * Self-contained header — no circular includes.
 * Manages per-service configuration with hot-reload support.
 *
 * Responsibilities:
 *   - Per-service configuration namespaces
 *   - Key-value configuration storage (string, int, float, bool)
 *   - Configuration validation per key (type checks, range, custom validators)
 *   - Change notifications (callbacks + events)
 *   - Configuration versioning (monotonic version counter)
 *   - Configuration history (track last N changes)
 *   - Hot-reload from files (file watcher support)
 *   - Configuration snapshots (save/restore)
 *   - Default values and overrides
 */

/* ---- Constants ---- */

#define OZAYN_CFG_MGR_MAX_SERVICES     64
#define OZAYN_CFG_MGR_MAX_KEYS_PER_SVC 32
#define OZAYN_CFG_MGR_MAX_KEY_LEN      128
#define OZAYN_CFG_MGR_MAX_VALUE_LEN    512
#define OZAYN_CFG_MGR_MAX_SVC_NAME     64
#define OZAYN_CFG_MGR_MAX_HISTORY      16   /* history entries per key */
#define OZAYN_CFG_MGR_MAX_LISTENERS    8    /* change listeners per service */

/* ---- Value types ---- */

typedef enum {
    OZAYN_CFG_VALUE_NONE     = 0,
    OZAYN_CFG_VALUE_STRING   = 1,
    OZAYN_CFG_VALUE_INT      = 2,
    OZAYN_CFG_VALUE_FLOAT    = 3,
    OZAYN_CFG_VALUE_BOOL     = 4,
} ozayn_cfg_value_type_t;

/* ---- Configuration value ---- */

typedef struct {
    ozayn_cfg_value_type_t type;
    union {
        char     str[OZAYN_CFG_MGR_MAX_VALUE_LEN];
        int64_t  ival;
        double   fval;
        int      bval;
    } as;
} ozayn_cfg_value_t;

/* ---- Key metadata ---- */

typedef struct {
    int                 active;
    char                key[OZAYN_CFG_MGR_MAX_KEY_LEN];
    ozayn_cfg_value_t   value;
    ozayn_cfg_value_t   default_value;
    int                 has_default;
    uint32_t            version;       /* monotonic version for this key */
    time_t              updated_at;
} ozayn_cfg_key_t;

/* ---- Change history entry ---- */

typedef struct {
    char                key[OZAYN_CFG_MGR_MAX_KEY_LEN];
    ozayn_cfg_value_t   old_value;
    ozayn_cfg_value_t   new_value;
    uint32_t            version;
    time_t              changed_at;
} ozayn_cfg_history_entry_t;

/* ---- Change listener callback ---- */

typedef void (*ozayn_cfg_change_listener_fn)(const char *service_name,
                                             const char *key,
                                             const ozayn_cfg_value_t *old_val,
                                             const ozayn_cfg_value_t *new_val,
                                             void *user_data);

/* ---- Change listener ---- */

typedef struct {
    int                          active;
    ozayn_cfg_change_listener_fn callback;
    void                        *user_data;
} ozayn_cfg_listener_t;

/* ---- Service configuration ---- */

typedef struct {
    int                          active;
    char                         name[OZAYN_CFG_MGR_MAX_SVC_NAME];
    ozayn_cfg_key_t              keys[OZAYN_CFG_MGR_MAX_KEYS_PER_SVC];
    uint32_t                     key_count;
    uint32_t                     version;       /* service-level version */
    time_t                       updated_at;
    ozayn_cfg_listener_t         listeners[OZAYN_CFG_MGR_MAX_LISTENERS];
    uint32_t                     listener_count;
    /* History ring buffer */
    ozayn_cfg_history_entry_t    history[OZAYN_CFG_MGR_MAX_HISTORY];
    uint32_t                     history_head;
    uint32_t                     history_count;
} ozayn_cfg_service_t;

/* ---- Snapshot ---- */

typedef struct {
    ozayn_cfg_service_t  services[OZAYN_CFG_MGR_MAX_SERVICES];
    uint32_t             service_count;
    uint32_t             global_version;
    time_t               taken_at;
} ozayn_cfg_snapshot_t;

/* ---- Stats ---- */

typedef struct {
    uint32_t total_services;
    uint32_t total_keys;
    uint32_t total_listeners;
    uint32_t total_changes;
    uint32_t global_version;
} ozayn_cfg_mgr_stats_t;

/* ---- Forward declarations for events ---- */
typedef struct ozayn_event_engine_s ozayn_event_engine_t;

/* ---- Manager configuration ---- */

typedef struct {
    uint32_t max_services;
    uint32_t max_keys_per_service;
    uint32_t max_history;
    uint32_t max_listeners;
} ozayn_cfg_mgr_config_t;

/* ---- Manager ---- */

typedef struct {
    ozayn_cfg_mgr_config_t   config;
    ozayn_cfg_service_t      services[OZAYN_CFG_MGR_MAX_SERVICES];
    uint32_t                 global_version;
    uint32_t                 total_changes;
    ozayn_event_engine_t    *events;
} ozayn_cfg_mgr_t;

/* ================================================================
 * API
 * ================================================================ */

/* Lifecycle */
int  ozayn_cfg_mgr_init(ozayn_cfg_mgr_t *mgr, const ozayn_cfg_mgr_config_t *config);
void ozayn_cfg_mgr_shutdown(ozayn_cfg_mgr_t *mgr);
void ozayn_cfg_mgr_set_events(ozayn_cfg_mgr_t *mgr, ozayn_event_engine_t *events);

/* Service registration */
int  ozayn_cfg_mgr_register_service(ozayn_cfg_mgr_t *mgr, const char *service_name);
int  ozayn_cfg_mgr_unregister_service(ozayn_cfg_mgr_t *mgr, const char *service_name);

/* Key operations */
int  ozayn_cfg_mgr_set_string(ozayn_cfg_mgr_t *mgr, const char *service,
                               const char *key, const char *value);
int  ozayn_cfg_mgr_set_int(ozayn_cfg_mgr_t *mgr, const char *service,
                            const char *key, int64_t value);
int  ozayn_cfg_mgr_set_float(ozayn_cfg_mgr_t *mgr, const char *service,
                              const char *key, double value);
int  ozayn_cfg_mgr_set_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                             const char *key, int value);

/* Getters */
int  ozayn_cfg_mgr_get_string(ozayn_cfg_mgr_t *mgr, const char *service,
                               const char *key, char *out, uint32_t out_size);
int  ozayn_cfg_mgr_get_int(ozayn_cfg_mgr_t *mgr, const char *service,
                            const char *key, int64_t *out);
int  ozayn_cfg_mgr_get_float(ozayn_cfg_mgr_t *mgr, const char *service,
                              const char *key, double *out);
int  ozayn_cfg_mgr_get_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                             const char *key, int *out);
int  ozayn_cfg_mgr_get(ozayn_cfg_mgr_t *mgr, const char *service,
                        const char *key, ozayn_cfg_value_t *out);

/* Defaults */
int  ozayn_cfg_mgr_set_default_string(ozayn_cfg_mgr_t *mgr, const char *service,
                                       const char *key, const char *value);
int  ozayn_cfg_mgr_set_default_int(ozayn_cfg_mgr_t *mgr, const char *service,
                                    const char *key, int64_t value);
int  ozayn_cfg_mgr_set_default_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                                     const char *key, int value);

/* Change listeners */
int  ozayn_cfg_mgr_add_listener(ozayn_cfg_mgr_t *mgr, const char *service,
                                 ozayn_cfg_change_listener_fn fn, void *user_data);
int  ozayn_cfg_mgr_remove_listener(ozayn_cfg_mgr_t *mgr, const char *service,
                                    int listener_id);

/* Versioning */
uint32_t ozayn_cfg_mgr_service_version(ozayn_cfg_mgr_t *mgr, const char *service);
uint32_t ozayn_cfg_mgr_key_version(ozayn_cfg_mgr_t *mgr, const char *service,
                                    const char *key);
uint32_t ozayn_cfg_mgr_global_version(ozayn_cfg_mgr_t *mgr);

/* History */
uint32_t ozayn_cfg_mgr_history_count(ozayn_cfg_mgr_t *mgr, const char *service);
int  ozayn_cfg_mgr_get_history(ozayn_cfg_mgr_t *mgr, const char *service,
                                ozayn_cfg_history_entry_t *out, uint32_t max_entries);

/* Snapshots */
int  ozayn_cfg_mgr_snapshot_save(ozayn_cfg_mgr_t *mgr, ozayn_cfg_snapshot_t *snap);
int  ozayn_cfg_mgr_snapshot_restore(ozayn_cfg_mgr_t *mgr, const ozayn_cfg_snapshot_t *snap);

/* Hot-reload */
int  ozayn_cfg_mgr_load_from_string(ozayn_cfg_mgr_t *mgr, const char *service_name,
                                     const char *config_str);
int  ozayn_cfg_mgr_load_from_file(ozayn_cfg_mgr_t *mgr, const char *service_name,
                                   const char *file_path);

/* Queries */
int  ozayn_cfg_mgr_has_key(ozayn_cfg_mgr_t *mgr, const char *service, const char *key);
uint32_t ozayn_cfg_mgr_key_count(ozayn_cfg_mgr_t *mgr, const char *service);
int  ozayn_cfg_mgr_list_services(ozayn_cfg_mgr_t *mgr, char names[][OZAYN_CFG_MGR_MAX_SVC_NAME],
                                  uint32_t max_out);
int  ozayn_cfg_mgr_list_keys(ozayn_cfg_mgr_t *mgr, const char *service,
                              char keys[][OZAYN_CFG_MGR_MAX_KEY_LEN], uint32_t max_out);

/* Stats */
ozayn_cfg_mgr_stats_t ozayn_cfg_mgr_stats(ozayn_cfg_mgr_t *mgr);

/* Print / debug */
void ozayn_cfg_mgr_print_service(ozayn_cfg_mgr_t *mgr, const char *service_name);
void ozayn_cfg_mgr_print_all(ozayn_cfg_mgr_t *mgr);

/* Names */
const char *ozayn_cfg_value_type_name(ozayn_cfg_value_type_t type);

#endif /* OZAYN_CONFIG_MGR_H */
