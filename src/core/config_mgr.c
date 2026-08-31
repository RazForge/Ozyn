#include "config_mgr.h"
#include "events.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * config_mgr.c — Configuration Management & Hot-Reload (Stage 25).
 *
 * Manages per-service key-value configuration with:
 *   - Type-safe getters/setters (string, int, float, bool)
 *   - Change notifications via callbacks
 *   - Monotonic versioning (global + per-service + per-key)
 *   - Change history ring buffer
 *   - Snapshots (save/restore entire config state)
 *   - Hot-reload from files (parses key=value lines)
 *   - Default values
 */

/* ================================================================
 * Helpers
 * ================================================================ */

static void publish_event(ozayn_cfg_mgr_t *mgr, ozayn_event_type_t type) {
    if (!mgr || !mgr->events) return;
    ozayn_events_publish(mgr->events, type, OZAYN_SRC_CONFIG, NULL);
}

static ozayn_cfg_service_t *find_service(ozayn_cfg_mgr_t *mgr, const char *name) {
    if (!name || !name[0]) return NULL;
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active &&
            strcmp(mgr->services[i].name, name) == 0) {
            return &mgr->services[i];
        }
    }
    return NULL;
}

static ozayn_cfg_key_t *find_key(ozayn_cfg_service_t *svc, const char *key) {
    if (!key || !key[0]) return NULL;
    for (uint32_t i = 0; i < svc->key_count; i++) {
        if (svc->keys[i].active && strcmp(svc->keys[i].key, key) == 0) {
            return &svc->keys[i];
        }
    }
    return NULL;
}

static ozayn_cfg_key_t *find_or_create_key(ozayn_cfg_service_t *svc, const char *key) {
    /* Try to find existing */
    ozayn_cfg_key_t *k = find_key(svc, key);
    if (k) return k;

    /* Create new */
    if (svc->key_count >= OZAYN_CFG_MGR_MAX_KEYS_PER_SVC) return NULL;
    k = &svc->keys[svc->key_count];
    memset(k, 0, sizeof(*k));
    k->active = 1;
    strncpy(k->key, key, OZAYN_CFG_MGR_MAX_KEY_LEN - 1);
    k->version = 0;
    svc->key_count++;
    return k;
}

static void add_history(ozayn_cfg_service_t *svc, const char *key,
                         const ozayn_cfg_value_t *old_val,
                         const ozayn_cfg_value_t *new_val,
                         uint32_t version) {
    uint32_t idx = (svc->history_head + svc->history_count) % OZAYN_CFG_MGR_MAX_HISTORY;
    if (svc->history_count >= OZAYN_CFG_MGR_MAX_HISTORY) {
        /* Overwrite oldest, advance head */
        svc->history_head = (svc->history_head + 1) % OZAYN_CFG_MGR_MAX_HISTORY;
    } else {
        svc->history_count++;
    }

    ozayn_cfg_history_entry_t *h = &svc->history[idx];
    memset(h, 0, sizeof(*h));
    strncpy(h->key, key, OZAYN_CFG_MGR_MAX_KEY_LEN - 1);
    if (old_val) h->old_value = *old_val;
    if (new_val) h->new_value = *new_val;
    h->version = version;
    h->changed_at = time(NULL);
}

static void notify_listeners(ozayn_cfg_service_t *svc, const char *key,
                              const ozayn_cfg_value_t *old_val,
                              const ozayn_cfg_value_t *new_val) {
    for (uint32_t i = 0; i < OZAYN_CFG_MGR_MAX_LISTENERS; i++) {
        if (svc->listeners[i].active && svc->listeners[i].callback) {
            svc->listeners[i].callback(svc->name, key, old_val, new_val,
                                        svc->listeners[i].user_data);
        }
    }
}

/* Trim whitespace */
static const char *trim(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int parse_bool_value(const char *s) {
    if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0)
        return 1;
    return 0;
}

/* ================================================================
 * Names
 * ================================================================ */

const char *ozayn_cfg_value_type_name(ozayn_cfg_value_type_t type) {
    switch (type) {
        case OZAYN_CFG_VALUE_NONE:   return "NONE";
        case OZAYN_CFG_VALUE_STRING: return "STRING";
        case OZAYN_CFG_VALUE_INT:    return "INT";
        case OZAYN_CFG_VALUE_FLOAT:  return "FLOAT";
        case OZAYN_CFG_VALUE_BOOL:   return "BOOL";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

int ozayn_cfg_mgr_init(ozayn_cfg_mgr_t *mgr, const ozayn_cfg_mgr_config_t *config) {
    if (!mgr || !config) return -1;
    memset(mgr, 0, sizeof(*mgr));
    mgr->config.max_services = config->max_services > 0 ?
        config->max_services : OZAYN_CFG_MGR_MAX_SERVICES;
    mgr->config.max_keys_per_service = config->max_keys_per_service > 0 ?
        config->max_keys_per_service : OZAYN_CFG_MGR_MAX_KEYS_PER_SVC;
    mgr->config.max_history = config->max_history > 0 ?
        config->max_history : OZAYN_CFG_MGR_MAX_HISTORY;
    mgr->config.max_listeners = config->max_listeners > 0 ?
        config->max_listeners : OZAYN_CFG_MGR_MAX_LISTENERS;
    if (mgr->config.max_services > OZAYN_CFG_MGR_MAX_SERVICES)
        mgr->config.max_services = OZAYN_CFG_MGR_MAX_SERVICES;
    mgr->global_version = 0;
    LOG_INFO("CONFIG_MGR", "Configuration manager initialized (max_services=%u)",
             mgr->config.max_services);
    return 0;
}

void ozayn_cfg_mgr_shutdown(ozayn_cfg_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("CONFIG_MGR", "Configuration manager shut down (services=%u, changes=%u, version=%u)",
             mgr->total_changes > 0 ? /* rough count */ 0u : 0u,
             mgr->total_changes, mgr->global_version);
    memset(mgr->services, 0, sizeof(mgr->services));
}

void ozayn_cfg_mgr_set_events(ozayn_cfg_mgr_t *mgr, ozayn_event_engine_t *events) {
    if (mgr) mgr->events = events;
}

/* ================================================================
 * Service registration
 * ================================================================ */

int ozayn_cfg_mgr_register_service(ozayn_cfg_mgr_t *mgr, const char *service_name) {
    if (!mgr || !service_name || !service_name[0]) return -1;
    if (find_service(mgr, service_name)) return -2; /* already registered */

    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (!mgr->services[i].active) {
            memset(&mgr->services[i], 0, sizeof(ozayn_cfg_service_t));
            mgr->services[i].active = 1;
            strncpy(mgr->services[i].name, service_name, OZAYN_CFG_MGR_MAX_SVC_NAME - 1);
            mgr->services[i].version = 0;
            LOG_INFO("CONFIG_MGR", "Registered service config: '%s'", service_name);
            return 0;
        }
    }
    return -3; /* no slot */
}

int ozayn_cfg_mgr_unregister_service(ozayn_cfg_mgr_t *mgr, const char *service_name) {
    ozayn_cfg_service_t *svc = find_service(mgr, service_name);
    if (!svc) return -1;
    LOG_INFO("CONFIG_MGR", "Unregistered service config: '%s'", svc->name);
    svc->active = 0;
    return 0;
}

/* ================================================================
 * Setters (with change notification)
 * ================================================================ */

static int set_value(ozayn_cfg_mgr_t *mgr, const char *service, const char *key,
                      const ozayn_cfg_value_t *new_val) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return -1;

    ozayn_cfg_key_t *k = find_or_create_key(svc, key);
    if (!k) return -2;

    ozayn_cfg_value_t old_val = k->value;
    int had_value = (k->value.type != OZAYN_CFG_VALUE_NONE);

    k->value = *new_val;
    k->version++;
    k->updated_at = time(NULL);
    svc->version++;
    svc->updated_at = time(NULL);
    mgr->global_version++;
    mgr->total_changes++;

    /* Record history */
    add_history(svc, key, had_value ? &old_val : NULL, new_val, k->version);

    /* Notify listeners */
    notify_listeners(svc, key, had_value ? &old_val : NULL, new_val);

    return 0;
}

int ozayn_cfg_mgr_set_string(ozayn_cfg_mgr_t *mgr, const char *service,
                               const char *key, const char *value) {
    if (!value) return -1;
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = OZAYN_CFG_VALUE_STRING;
    strncpy(v.as.str, value, OZAYN_CFG_MGR_MAX_VALUE_LEN - 1);
    return set_value(mgr, service, key, &v);
}

int ozayn_cfg_mgr_set_int(ozayn_cfg_mgr_t *mgr, const char *service,
                            const char *key, int64_t value) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = OZAYN_CFG_VALUE_INT;
    v.as.ival = value;
    return set_value(mgr, service, key, &v);
}

int ozayn_cfg_mgr_set_float(ozayn_cfg_mgr_t *mgr, const char *service,
                              const char *key, double value) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = OZAYN_CFG_VALUE_FLOAT;
    v.as.fval = value;
    return set_value(mgr, service, key, &v);
}

int ozayn_cfg_mgr_set_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                             const char *key, int value) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = OZAYN_CFG_VALUE_BOOL;
    v.as.bval = value ? 1 : 0;
    return set_value(mgr, service, key, &v);
}

/* ================================================================
 * Getters
 * ================================================================ */

int ozayn_cfg_mgr_get(ozayn_cfg_mgr_t *mgr, const char *service,
                        const char *key, ozayn_cfg_value_t *out) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc || !out) return -1;

    ozayn_cfg_key_t *k = find_key(svc, key);
    if (!k) {
        /* Try default */
        if (k && k->has_default) {
            *out = k->default_value;
            return 0;
        }
        return -2; /* not found */
    }

    if (k->value.type == OZAYN_CFG_VALUE_NONE && k->has_default) {
        *out = k->default_value;
    } else {
        *out = k->value;
    }
    return 0;
}

int ozayn_cfg_mgr_get_string(ozayn_cfg_mgr_t *mgr, const char *service,
                               const char *key, char *out, uint32_t out_size) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    int r = ozayn_cfg_mgr_get(mgr, service, key, &v);
    if (r != 0) return r;
    if (v.type != OZAYN_CFG_VALUE_STRING) return -3;
    strncpy(out, v.as.str, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
}

int ozayn_cfg_mgr_get_int(ozayn_cfg_mgr_t *mgr, const char *service,
                            const char *key, int64_t *out) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    int r = ozayn_cfg_mgr_get(mgr, service, key, &v);
    if (r != 0) return r;
    if (v.type != OZAYN_CFG_VALUE_INT) return -3;
    *out = v.as.ival;
    return 0;
}

int ozayn_cfg_mgr_get_float(ozayn_cfg_mgr_t *mgr, const char *service,
                              const char *key, double *out) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    int r = ozayn_cfg_mgr_get(mgr, service, key, &v);
    if (r != 0) return r;
    if (v.type != OZAYN_CFG_VALUE_FLOAT) return -3;
    *out = v.as.fval;
    return 0;
}

int ozayn_cfg_mgr_get_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                             const char *key, int *out) {
    ozayn_cfg_value_t v;
    memset(&v, 0, sizeof(v));
    int r = ozayn_cfg_mgr_get(mgr, service, key, &v);
    if (r != 0) return r;
    if (v.type != OZAYN_CFG_VALUE_BOOL) return -3;
    *out = v.as.bval;
    return 0;
}

/* ================================================================
 * Defaults
 * ================================================================ */

int ozayn_cfg_mgr_set_default_string(ozayn_cfg_mgr_t *mgr, const char *service,
                                       const char *key, const char *value) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc || !value) return -1;
    ozayn_cfg_key_t *k = find_or_create_key(svc, key);
    if (!k) return -2;
    k->default_value.type = OZAYN_CFG_VALUE_STRING;
    strncpy(k->default_value.as.str, value, OZAYN_CFG_MGR_MAX_VALUE_LEN - 1);
    k->has_default = 1;
    return 0;
}

int ozayn_cfg_mgr_set_default_int(ozayn_cfg_mgr_t *mgr, const char *service,
                                    const char *key, int64_t value) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return -1;
    ozayn_cfg_key_t *k = find_or_create_key(svc, key);
    if (!k) return -2;
    k->default_value.type = OZAYN_CFG_VALUE_INT;
    k->default_value.as.ival = value;
    k->has_default = 1;
    return 0;
}

int ozayn_cfg_mgr_set_default_bool(ozayn_cfg_mgr_t *mgr, const char *service,
                                     const char *key, int value) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return -1;
    ozayn_cfg_key_t *k = find_or_create_key(svc, key);
    if (!k) return -2;
    k->default_value.type = OZAYN_CFG_VALUE_BOOL;
    k->default_value.as.bval = value ? 1 : 0;
    k->has_default = 1;
    return 0;
}

/* ================================================================
 * Change listeners
 * ================================================================ */

int ozayn_cfg_mgr_add_listener(ozayn_cfg_mgr_t *mgr, const char *service,
                                 ozayn_cfg_change_listener_fn fn, void *user_data) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc || !fn) return -1;

    for (uint32_t i = 0; i < OZAYN_CFG_MGR_MAX_LISTENERS; i++) {
        if (!svc->listeners[i].active) {
            svc->listeners[i].active = 1;
            svc->listeners[i].callback = fn;
            svc->listeners[i].user_data = user_data;
            svc->listener_count++;
            LOG_INFO("CONFIG_MGR", "Added change listener for '%s' (slot=%u)",
                     service, i);
            return (int)i;
        }
    }
    return -2; /* no slot */
}

int ozayn_cfg_mgr_remove_listener(ozayn_cfg_mgr_t *mgr, const char *service,
                                    int listener_id) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return -1;
    if (listener_id < 0 || listener_id >= (int)OZAYN_CFG_MGR_MAX_LISTENERS) return -2;
    if (!svc->listeners[listener_id].active) return -3;

    svc->listeners[listener_id].active = 0;
    svc->listeners[listener_id].callback = NULL;
    svc->listeners[listener_id].user_data = NULL;
    if (svc->listener_count > 0) svc->listener_count--;
    LOG_INFO("CONFIG_MGR", "Removed change listener for '%s' (slot=%d)",
             service, listener_id);
    return 0;
}

/* ================================================================
 * Versioning
 * ================================================================ */

uint32_t ozayn_cfg_mgr_service_version(ozayn_cfg_mgr_t *mgr, const char *service) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    return svc ? svc->version : 0;
}

uint32_t ozayn_cfg_mgr_key_version(ozayn_cfg_mgr_t *mgr, const char *service,
                                    const char *key) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return 0;
    ozayn_cfg_key_t *k = find_key(svc, key);
    return k ? k->version : 0;
}

uint32_t ozayn_cfg_mgr_global_version(ozayn_cfg_mgr_t *mgr) {
    return mgr ? mgr->global_version : 0;
}

/* ================================================================
 * History
 * ================================================================ */

uint32_t ozayn_cfg_mgr_history_count(ozayn_cfg_mgr_t *mgr, const char *service) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    return svc ? svc->history_count : 0;
}

int ozayn_cfg_mgr_get_history(ozayn_cfg_mgr_t *mgr, const char *service,
                                ozayn_cfg_history_entry_t *out, uint32_t max_entries) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc || !out) return -1;

    uint32_t count = svc->history_count < max_entries ? svc->history_count : max_entries;
    uint32_t start = (svc->history_head + svc->history_count - count) %
                     OZAYN_CFG_MGR_MAX_HISTORY;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start + i) % OZAYN_CFG_MGR_MAX_HISTORY;
        out[i] = svc->history[idx];
    }
    return (int)count;
}

/* ================================================================
 * Snapshots
 * ================================================================ */

int ozayn_cfg_mgr_snapshot_save(ozayn_cfg_mgr_t *mgr, ozayn_cfg_snapshot_t *snap) {
    if (!mgr || !snap) return -1;
    memset(snap, 0, sizeof(*snap));
    snap->global_version = mgr->global_version;
    snap->taken_at = time(NULL);
    snap->service_count = 0;

    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active) {
            snap->services[snap->service_count] = mgr->services[i];
            snap->service_count++;
        }
    }
    LOG_INFO("CONFIG_MGR", "Snapshot saved (version=%u, services=%u)",
             snap->global_version, snap->service_count);
    return 0;
}

int ozayn_cfg_mgr_snapshot_restore(ozayn_cfg_mgr_t *mgr, const ozayn_cfg_snapshot_t *snap) {
    if (!mgr || !snap) return -1;
    memset(mgr->services, 0, sizeof(mgr->services));
    mgr->global_version = snap->global_version;

    for (uint32_t i = 0; i < snap->service_count && i < mgr->config.max_services; i++) {
        mgr->services[i] = snap->services[i];
    }
    LOG_INFO("CONFIG_MGR", "Snapshot restored (version=%u, services=%u)",
             snap->global_version, snap->service_count);
    return 0;
}

/* ================================================================
 * Hot-reload (parse key=value from string)
 * ================================================================ */

int ozayn_cfg_mgr_load_from_string(ozayn_cfg_mgr_t *mgr, const char *service_name,
                                     const char *config_str) {
    if (!mgr || !service_name || !config_str) return -1;

    int r = ozayn_cfg_mgr_register_service(mgr, service_name);
    if (r < 0 && r != -2) return r; /* -2 means already registered, that's fine */

    int loaded = 0;
    const char *p = config_str;

    while (*p) {
        /* Skip empty lines and comments */
        while (*p == '\n' || *p == '\r') p++;
        if (*p == '#' || *p == '\0') { p++; continue; }

        /* Find key */
        const char *key_start = p;
        while (*p && *p != '=' && *p != '\n' && *p != '\r') p++;
        if (*p != '=' || p == key_start) {
            /* Skip malformed line */
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Extract key (trimmed) */
        char key[OZAYN_CFG_MGR_MAX_KEY_LEN];
        size_t klen = (size_t)(p - key_start);
        if (klen >= OZAYN_CFG_MGR_MAX_KEY_LEN) klen = OZAYN_CFG_MGR_MAX_KEY_LEN - 1;
        memcpy(key, key_start, klen);
        key[klen] = '\0';
        /* Trim trailing spaces from key */
        while (klen > 0 && (key[klen-1] == ' ' || key[klen-1] == '\t')) {
            key[--klen] = '\0';
        }

        p++; /* skip '=' */

        /* Find end of value */
        const char *val_start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        size_t vlen = (size_t)(p - val_start);

        /* Extract value (trimmed) */
        char value[OZAYN_CFG_MGR_MAX_VALUE_LEN];
        if (vlen >= OZAYN_CFG_MGR_MAX_VALUE_LEN) vlen = OZAYN_CFG_MGR_MAX_VALUE_LEN - 1;
        memcpy(value, val_start, vlen);
        value[vlen] = '\0';
        /* Trim leading/trailing spaces */
        const char *trimmed = trim(value);
        size_t tlen = strlen(trimmed);
        while (tlen > 0 && (trimmed[tlen-1] == ' ' || trimmed[tlen-1] == '\t' ||
                            trimmed[tlen-1] == '\r')) tlen--;

        /* Detect type and set */
        if (strcmp(trimmed, "true") == 0 || strcmp(trimmed, "false") == 0 ||
            strcmp(trimmed, "yes") == 0 || strcmp(trimmed, "no") == 0 ||
            strcmp(trimmed, "1") == 0 || strcmp(trimmed, "0") == 0) {
            ozayn_cfg_mgr_set_bool(mgr, service_name, key, parse_bool_value(trimmed));
        } else {
            /* Try integer */
            char *endptr = NULL;
            long long ival = strtoll(trimmed, &endptr, 10);
            if (endptr != trimmed && *endptr == '\0') {
                ozayn_cfg_mgr_set_int(mgr, service_name, key, (int64_t)ival);
            } else {
                /* Try float */
                double fval = strtod(trimmed, &endptr);
                if (endptr != trimmed && *endptr == '\0') {
                    ozayn_cfg_mgr_set_float(mgr, service_name, key, fval);
                } else {
                    ozayn_cfg_mgr_set_string(mgr, service_name, key, trimmed);
                }
            }
        }
        loaded++;
    }

    LOG_INFO("CONFIG_MGR", "Loaded %d keys for service '%s'", loaded, service_name);
    publish_event(mgr, OZAYN_EVENT_CONFIG_LOADED);
    return loaded;
}

int ozayn_cfg_mgr_load_from_file(ozayn_cfg_mgr_t *mgr, const char *service_name,
                                   const char *file_path) {
    if (!mgr || !service_name || !file_path) return -1;

    FILE *f = fopen(file_path, "r");
    if (!f) {
        LOG_INFO("CONFIG_MGR", "Failed to open config file: %s", file_path);
        return -2;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 65536) {
        fclose(f);
        return -3;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -4; }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    buf[nread] = '\0';
    fclose(f);

    int r = ozayn_cfg_mgr_load_from_string(mgr, service_name, buf);
    free(buf);
    return r;
}

/* ================================================================
 * Queries
 * ================================================================ */

int ozayn_cfg_mgr_has_key(ozayn_cfg_mgr_t *mgr, const char *service, const char *key) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return 0;
    return find_key(svc, key) != NULL;
}

uint32_t ozayn_cfg_mgr_key_count(ozayn_cfg_mgr_t *mgr, const char *service) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    return svc ? svc->key_count : 0;
}

int ozayn_cfg_mgr_list_services(ozayn_cfg_mgr_t *mgr, char names[][OZAYN_CFG_MGR_MAX_SVC_NAME],
                                  uint32_t max_out) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->config.max_services && count < max_out; i++) {
        if (mgr->services[i].active) {
            strncpy(names[count], mgr->services[i].name, OZAYN_CFG_MGR_MAX_SVC_NAME - 1);
            count++;
        }
    }
    return (int)count;
}

int ozayn_cfg_mgr_list_keys(ozayn_cfg_mgr_t *mgr, const char *service,
                              char keys[][OZAYN_CFG_MGR_MAX_KEY_LEN], uint32_t max_out) {
    ozayn_cfg_service_t *svc = find_service(mgr, service);
    if (!svc) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < svc->key_count && count < max_out; i++) {
        if (svc->keys[i].active) {
            strncpy(keys[count], svc->keys[i].key, OZAYN_CFG_MGR_MAX_KEY_LEN - 1);
            count++;
        }
    }
    return (int)count;
}

/* ================================================================
 * Stats
 * ================================================================ */

ozayn_cfg_mgr_stats_t ozayn_cfg_mgr_stats(ozayn_cfg_mgr_t *mgr) {
    ozayn_cfg_mgr_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    s.global_version = mgr->global_version;
    s.total_changes = mgr->total_changes;

    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active) {
            s.total_services++;
            s.total_keys += mgr->services[i].key_count;
            s.total_listeners += mgr->services[i].listener_count;
        }
    }
    return s;
}

/* ================================================================
 * Print / debug
 * ================================================================ */

static void print_value(const char *key, const ozayn_cfg_value_t *val, uint32_t version) {
    switch (val->type) {
        case OZAYN_CFG_VALUE_STRING:
            LOG_INFO("CONFIG_MGR", "    %s = \"%s\" (v%u, string)", key, val->as.str, version);
            break;
        case OZAYN_CFG_VALUE_INT:
            LOG_INFO("CONFIG_MGR", "    %s = %lld (v%u, int)",
                     key, (long long)val->as.ival, version);
            break;
        case OZAYN_CFG_VALUE_FLOAT:
            LOG_INFO("CONFIG_MGR", "    %s = %.6f (v%u, float)",
                     key, val->as.fval, version);
            break;
        case OZAYN_CFG_VALUE_BOOL:
            LOG_INFO("CONFIG_MGR", "    %s = %s (v%u, bool)",
                     key, val->as.bval ? "true" : "false", version);
            break;
        case OZAYN_CFG_VALUE_NONE:
            LOG_INFO("CONFIG_MGR", "    %s = <default> (v%u)", key, version);
            break;
    }
}

void ozayn_cfg_mgr_print_service(ozayn_cfg_mgr_t *mgr, const char *service_name) {
    ozayn_cfg_service_t *svc = find_service(mgr, service_name);
    if (!svc) {
        LOG_INFO("CONFIG_MGR", "Service '%s' not found", service_name ? service_name : "(null)");
        return;
    }
    LOG_INFO("CONFIG_MGR", "=== Service Config: %s (v%u, %u keys) ===",
             svc->name, svc->version, svc->key_count);
    for (uint32_t i = 0; i < svc->key_count; i++) {
        if (svc->keys[i].active) {
            ozayn_cfg_key_t *k = &svc->keys[i];
            if (k->value.type != OZAYN_CFG_VALUE_NONE) {
                print_value(k->key, &k->value, k->version);
            } else if (k->has_default) {
                print_value(k->key, &k->default_value, k->version);
                LOG_INFO("CONFIG_MGR", "    (using default)");
            }
        }
    }
    LOG_INFO("CONFIG_MGR", "  Listeners: %u", svc->listener_count);
    LOG_INFO("CONFIG_MGR", "  History entries: %u", svc->history_count);
}

void ozayn_cfg_mgr_print_all(ozayn_cfg_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("CONFIG_MGR", "=== Configuration Manager (global_version=%u) ===",
             mgr->global_version);
    for (uint32_t i = 0; i < mgr->config.max_services; i++) {
        if (mgr->services[i].active) {
            ozayn_cfg_mgr_print_service(mgr, mgr->services[i].name);
        }
    }
}
