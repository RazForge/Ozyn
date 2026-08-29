#include "plugins.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

/* ---------- State name ---------- */

const char *ozayn_plugin_state_name(ozayn_plugin_state_t state) {
    switch (state) {
        case OZAYN_PLUGIN_DISCOVERED:   return "DISCOVERED";
        case OZAYN_PLUGIN_VALIDATED:    return "VALIDATED";
        case OZAYN_PLUGIN_LOADED:       return "LOADED";
        case OZAYN_PLUGIN_INITIALIZED:  return "INITIALIZED";
        case OZAYN_PLUGIN_RUNNING:      return "RUNNING";
        case OZAYN_PLUGIN_STOPPING:     return "STOPPING";
        case OZAYN_PLUGIN_STOPPED:      return "STOPPED";
        case OZAYN_PLUGIN_UNLOADED:     return "UNLOADED";
        case OZAYN_PLUGIN_INVALID:      return "INVALID";
        case OZAYN_PLUGIN_INCOMPATIBLE: return "INCOMPATIBLE";
        case OZAYN_PLUGIN_FAILED:       return "FAILED";
    }
    return "UNKNOWN";
}

/* ---------- Find by ID ---------- */

static int find_index(const ozayn_plugin_manager_t *mgr, const char *id) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->plugins[i].id, id) == 0)
            return i;
    }
    return -1;
}

/* ---------- Find by filename ---------- */

static int find_by_path(const ozayn_plugin_manager_t *mgr, const char *path) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->plugins[i].path, path) == 0)
            return i;
    }
    return -1;
}

/* ---------- Publish helper ---------- */

static void publish_event(ozayn_plugin_manager_t *mgr, ozayn_event_type_t type) {
    if (mgr->context.events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->context.events,
                             type, OZAYN_SRC_PLUGIN, NULL);
    }
}

/* ---------- Raise recovery error ---------- */

static void raise_error(ozayn_plugin_manager_t *mgr, const char *plugin_id,
                        const char *message) {
    if (mgr->context.recovery) {
        ozayn_recovery_raise((ozayn_recovery_t *)mgr->context.recovery,
                             OZAYN_ERRCAT_PLUGIN, OZAYN_LOG_ERROR,
                             OZAYN_SCOPE_COMPONENT,
                             plugin_id, message);
    }
}

/* ---------- Validate file ---------- */

static ozayn_result_t validate_file(const char *filepath, char *id_out, size_t id_len) {
    struct stat st;

    /* Check file exists and is a regular file */
    if (stat(filepath, &st) != 0) {
        return OZAYN_ERR;
    }
    if (!S_ISREG(st.st_mode)) {
        return OZAYN_ERR;
    }

    /* Check .so extension */
    const char *ext = strrchr(filepath, '.');
    if (!ext || strcmp(ext, ".so") != 0) {
        return OZAYN_ERR;
    }

    /* Check readable */
    if (access(filepath, R_OK) != 0) {
        return OZAYN_ERR;
    }

    /* Extract base filename as candidate ID */
    const char *base = strrchr(filepath, '/');
    base = base ? base + 1 : filepath;

    /* Remove .so extension for ID */
    size_t base_len = (size_t)(ext - base);
    if (base_len >= id_len) base_len = id_len - 1;
    memcpy(id_out, base, base_len);
    id_out[base_len] = '\0';

    return OZAYN_OK;
}

/* ---------- Init ---------- */

ozayn_result_t ozayn_plugin_manager_init(ozayn_plugin_manager_t *mgr) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_plugin_manager_t));
    mgr->initialized = 1;

    LOG_INFO("PLUGINS", "Plugin manager initialized");
    return OZAYN_OK;
}

/* ---------- Shutdown ---------- */

void ozayn_plugin_manager_shutdown(ozayn_plugin_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Stop and unload all plugins in reverse order */
    ozayn_plugin_manager_stop_all(mgr);

    for (int i = mgr->count - 1; i >= 0; i--) {
        ozayn_plugin_record_t *rec = &mgr->plugins[i];

        if (rec->state == OZAYN_PLUGIN_UNLOADED || rec->state == OZAYN_PLUGIN_FAILED)
            continue;

        /* Shutdown if initialized */
        if (rec->state == OZAYN_PLUGIN_INITIALIZED ||
            rec->state == OZAYN_PLUGIN_STOPPED) {
            if (rec->api && rec->api->shutdown) {
                rec->api->shutdown(&mgr->context);
            }
        }

        /* Close library */
        if (rec->handle) {
            dlclose(rec->handle);
            rec->handle = NULL;
        }

        rec->api = NULL;
        rec->state = OZAYN_PLUGIN_UNLOADED;
    }

    mgr->initialized = 0;
    mgr->count = 0;

    LOG_INFO("PLUGINS", "Plugin manager shut down");
}

/* ---------- Engine bindings ---------- */

void ozayn_plugin_manager_set_logger(ozayn_plugin_manager_t *mgr, void *logger) {
    if (mgr) mgr->context.logger = logger;
}

void ozayn_plugin_manager_set_events(ozayn_plugin_manager_t *mgr, void *events) {
    if (mgr) mgr->context.events = events;
}

void ozayn_plugin_manager_set_recovery(ozayn_plugin_manager_t *mgr, void *recovery) {
    if (mgr) mgr->context.recovery = recovery;
}

void ozayn_plugin_manager_set_config(ozayn_plugin_manager_t *mgr, void *config) {
    if (mgr) mgr->context.config = config;
}

void ozayn_plugin_manager_set_runtime(ozayn_plugin_manager_t *mgr, void *runtime) {
    if (mgr) mgr->context.runtime = runtime;
}

/* ---------- Discovery ---------- */

int ozayn_plugin_manager_discover(ozayn_plugin_manager_t *mgr, const char *plugin_dir) {
    if (!mgr || !plugin_dir) return 0;
    if (!mgr->initialized) return 0;

    DIR *dir = opendir(plugin_dir);
    if (!dir) {
        LOG_WARN("PLUGINS", "Plugin directory not found: %s", plugin_dir);
        return 0;
    }

    int discovered = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        /* Skip directories and hidden files */
        if (entry->d_name[0] == '.') continue;

        /* Build full path */
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", plugin_dir, entry->d_name);

        /* Quick extension check before full validation */
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".so") != 0) continue;

        /* Check if already registered */
        if (find_by_path(mgr, filepath) >= 0) continue;

        /* Validate */
        char candidate_id[OZAYN_PLUGIN_ID_MAX];
        if (validate_file(filepath, candidate_id, sizeof(candidate_id)) != OZAYN_OK) {
            LOG_WARN("PLUGINS", "Invalid plugin candidate: %s", entry->d_name);
            continue;
        }

        /* Check registry capacity */
        if (mgr->count >= OZAYN_MAX_PLUGINS) {
            LOG_WARN("PLUGINS", "Plugin limit reached (%d)", OZAYN_MAX_PLUGINS);
            break;
        }

        /* Register as discovered */
        ozayn_plugin_record_t *rec = &mgr->plugins[mgr->count];
        memset(rec, 0, sizeof(ozayn_plugin_record_t));
        snprintf(rec->id, sizeof(rec->id), "%s", candidate_id);
        snprintf(rec->path, sizeof(rec->path), "%s", filepath);
        rec->state = OZAYN_PLUGIN_DISCOVERED;
        rec->event_sub_id = -1;

        mgr->count++;
        discovered++;

        LOG_INFO("PLUGINS", "Discovered: %s (id=%s)", entry->d_name, candidate_id);
        publish_event(mgr, OZAYN_EVENT_PLUGIN_DISCOVERED);
    }

    closedir(dir);

    LOG_INFO("PLUGINS", "Discovery complete: %d plugin(s) found in %s",
             discovered, plugin_dir);
    return discovered;
}

/* ---------- Load single plugin ---------- */

ozayn_result_t ozayn_plugin_manager_load(ozayn_plugin_manager_t *mgr, const char *filename) {
    if (!mgr || !filename) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Find the discovered record by path or ID */
    int idx = -1;
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->plugins[i].path, filename) == 0 ||
            strcmp(mgr->plugins[i].id, filename) == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        /* Not discovered yet — try to validate and add */
        if (mgr->count >= OZAYN_MAX_PLUGINS) {
            LOG_ERROR("PLUGINS", "Plugin limit reached");
            return OZAYN_ERR;
        }

        char candidate_id[OZAYN_PLUGIN_ID_MAX];
        if (validate_file(filename, candidate_id, sizeof(candidate_id)) != OZAYN_OK) {
            LOG_ERROR("PLUGINS", "Validation failed: %s", filename);
            return OZAYN_ERR;
        }

        ozayn_plugin_record_t *rec = &mgr->plugins[mgr->count];
        memset(rec, 0, sizeof(ozayn_plugin_record_t));
        snprintf(rec->id, sizeof(rec->id), "%s", candidate_id);
        snprintf(rec->path, sizeof(rec->path), "%s", filename);
        rec->state = OZAYN_PLUGIN_DISCOVERED;
        rec->event_sub_id = -1;
        idx = mgr->count;
        mgr->count++;
    }

    ozayn_plugin_record_t *rec = &mgr->plugins[idx];

    /* Skip if not in DISCOVERED or VALIDATED state */
    if (rec->state != OZAYN_PLUGIN_DISCOVERED &&
        rec->state != OZAYN_PLUGIN_VALIDATED) {
        LOG_WARN("PLUGINS", "Plugin '%s' in state %s — skipping load",
                 rec->id, ozayn_plugin_state_name(rec->state));
        return OZAYN_ERR_STATE;
    }

    /* Mark validated */
    rec->state = OZAYN_PLUGIN_VALIDATED;
    LOG_INFO("PLUGINS", "Validated: %s (%s)", rec->id, rec->path);

    /* dlopen */
    rec->handle = dlopen(rec->path, RTLD_NOW);
    if (!rec->handle) {
        LOG_ERROR("PLUGINS", "Load failed: %s — %s", rec->id, dlerror());
        rec->state = OZAYN_PLUGIN_FAILED;
        raise_error(mgr, rec->id, "dlopen failed");
        publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
        return OZAYN_ERR;
    }

    /* Resolve entry point */
    ozayn_plugin_api_t *(*entry_fn)(void);
    entry_fn = dlsym(rec->handle, OZAYN_PLUGIN_ENTRY_SYMBOL);
    if (!entry_fn) {
        LOG_ERROR("PLUGINS", "Missing entry symbol: %s — %s", rec->id, dlerror());
        dlclose(rec->handle);
        rec->handle = NULL;
        rec->state = OZAYN_PLUGIN_FAILED;
        raise_error(mgr, rec->id, "Missing entry symbol");
        publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
        return OZAYN_ERR;
    }

    /* Get API table */
    rec->api = entry_fn();
    if (!rec->api) {
        LOG_ERROR("PLUGINS", "Entry returned NULL: %s", rec->id);
        dlclose(rec->handle);
        rec->handle = NULL;
        rec->state = OZAYN_PLUGIN_FAILED;
        raise_error(mgr, rec->id, "Entry returned NULL");
        publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
        return OZAYN_ERR;
    }

    /* Get metadata */
    if (rec->api->get_info) {
        const ozayn_plugin_info_t *info = rec->api->get_info();
        if (info) {
            rec->info = *info; /* shallow copy */

            /* Check API version */
            if (info->api_version != OZAYN_PLUGIN_API_VERSION) {
                LOG_ERROR("PLUGINS", "API mismatch: %s declares API %u, need %u",
                          rec->id, info->api_version, OZAYN_PLUGIN_API_VERSION);
                dlclose(rec->handle);
                rec->handle = NULL;
                rec->api = NULL;
                rec->state = OZAYN_PLUGIN_INCOMPATIBLE;
                raise_error(mgr, rec->id, "API version incompatible");
                publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
                return OZAYN_ERR;
            }

            /* Check for duplicate ID before updating the record */
            if (info->id) {
                int existing = find_index(mgr, info->id);
                if (existing >= 0 && existing != idx) {
                    LOG_WARN("PLUGINS", "Duplicate plugin ID '%s' — rejecting (already loaded from %s)",
                             info->id, mgr->plugins[existing].path);
                    dlclose(rec->handle);
                    rec->handle = NULL;
                    rec->api = NULL;
                    for (int j = idx; j < mgr->count - 1; j++)
                        mgr->plugins[j] = mgr->plugins[j + 1];
                    memset(&mgr->plugins[mgr->count - 1], 0, sizeof(ozayn_plugin_record_t));
                    mgr->count--;
                    return OZAYN_ERR_STATE;
                }
            }

            /* Cache strings — after duplicate check */
            if (info->id) snprintf(rec->id, sizeof(rec->id), "%s", info->id);
            if (info->name) snprintf(rec->name, sizeof(rec->name), "%s", info->name);
            if (info->version) snprintf(rec->version, sizeof(rec->version), "%s", info->version);

            LOG_INFO("PLUGINS", "Loaded: %s v%s (API %u, author=%s)",
                     rec->id, rec->version, info->api_version,
                     info->author ? info->author : "unknown");
        }
    }

    rec->state = OZAYN_PLUGIN_LOADED;
    publish_event(mgr, OZAYN_EVENT_PLUGIN_LOADED);
    return OZAYN_OK;
}

/* ---------- Init all ---------- */

ozayn_result_t ozayn_plugin_manager_init_all(ozayn_plugin_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    int failed = 0;

    for (int i = 0; i < mgr->count; i++) {
        ozayn_plugin_record_t *rec = &mgr->plugins[i];

        if (rec->state != OZAYN_PLUGIN_LOADED) continue;

        if (!rec->api || !rec->api->init) {
            LOG_WARN("PLUGINS", "Plugin '%s' has no init — marking FAILED", rec->id);
            rec->state = OZAYN_PLUGIN_FAILED;
            failed++;
            continue;
        }

        ozayn_result_t r = rec->api->init(&mgr->context);
        if (r == OZAYN_OK) {
            rec->state = OZAYN_PLUGIN_INITIALIZED;
            LOG_INFO("PLUGINS", "Initialized: %s", rec->id);
            publish_event(mgr, OZAYN_EVENT_PLUGIN_INITIALIZED);
        } else {
            rec->state = OZAYN_PLUGIN_FAILED;
            failed++;
            LOG_ERROR("PLUGINS", "Init failed: %s (result=%d)", rec->id, r);

            /* Cleanup: close library */
            if (rec->handle) {
                dlclose(rec->handle);
                rec->handle = NULL;
            }
            rec->api = NULL;

            raise_error(mgr, rec->id, "Plugin initialization failed");
            publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
        }
    }

    if (failed > 0) {
        LOG_WARN("PLUGINS", "%d plugin(s) failed initialization", failed);
    }

    return OZAYN_OK;
}

/* ---------- Start all ---------- */

ozayn_result_t ozayn_plugin_manager_start_all(ozayn_plugin_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    for (int i = 0; i < mgr->count; i++) {
        ozayn_plugin_record_t *rec = &mgr->plugins[i];

        if (rec->state != OZAYN_PLUGIN_INITIALIZED) continue;

        if (!rec->api || !rec->api->start) {
            /* No start — treat as running */
            rec->state = OZAYN_PLUGIN_RUNNING;
            continue;
        }

        ozayn_result_t r = rec->api->start(&mgr->context);
        if (r == OZAYN_OK) {
            rec->state = OZAYN_PLUGIN_RUNNING;
            LOG_INFO("PLUGINS", "Started: %s", rec->id);
            publish_event(mgr, OZAYN_EVENT_PLUGIN_STARTED);
        } else {
            rec->state = OZAYN_PLUGIN_FAILED;
            LOG_ERROR("PLUGINS", "Start failed: %s", rec->id);
            raise_error(mgr, rec->id, "Plugin start failed");
            publish_event(mgr, OZAYN_EVENT_PLUGIN_FAILED);
        }
    }

    return OZAYN_OK;
}

/* ---------- Stop all ---------- */

void ozayn_plugin_manager_stop_all(ozayn_plugin_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Reverse order */
    for (int i = mgr->count - 1; i >= 0; i--) {
        ozayn_plugin_record_t *rec = &mgr->plugins[i];

        if (rec->state != OZAYN_PLUGIN_RUNNING) continue;

        rec->state = OZAYN_PLUGIN_STOPPING;

        if (rec->api && rec->api->stop) {
            rec->api->stop(&mgr->context);
        }

        rec->state = OZAYN_PLUGIN_STOPPED;
        LOG_INFO("PLUGINS", "Stopped: %s", rec->id);
        publish_event(mgr, OZAYN_EVENT_PLUGIN_STOPPED);
    }
}

/* ---------- Unload single plugin ---------- */

ozayn_result_t ozayn_plugin_manager_unload(ozayn_plugin_manager_t *mgr, const char *id) {
    if (!mgr || !id) return OZAYN_ERR_NULL;

    int idx = find_index(mgr, id);
    if (idx < 0) {
        LOG_WARN("PLUGINS", "Plugin '%s' not found for unload", id);
        return OZAYN_ERR;
    }

    ozayn_plugin_record_t *rec = &mgr->plugins[idx];

    /* Already unloaded or failed */
    if (rec->state == OZAYN_PLUGIN_UNLOADED || rec->state == OZAYN_PLUGIN_FAILED) {
        return OZAYN_OK;
    }

    /* Stop if running */
    if (rec->state == OZAYN_PLUGIN_RUNNING) {
        if (rec->api && rec->api->stop) {
            rec->api->stop(&mgr->context);
        }
    }

    /* Shutdown if initialized or stopped */
    if (rec->state == OZAYN_PLUGIN_INITIALIZED ||
        rec->state == OZAYN_PLUGIN_STOPPED ||
        rec->state == OZAYN_PLUGIN_RUNNING) {
        if (rec->api && rec->api->shutdown) {
            rec->api->shutdown(&mgr->context);
        }
    }

    /* Unsubscribe events */
    if (rec->event_sub_id >= 0 && mgr->context.events) {
        ozayn_events_unsubscribe((ozayn_event_engine_t *)mgr->context.events,
                                 rec->event_sub_id);
        rec->event_sub_id = -1;
    }

    /* Close library */
    if (rec->handle) {
        dlclose(rec->handle);
        rec->handle = NULL;
    }

    rec->api = NULL;
    rec->state = OZAYN_PLUGIN_UNLOADED;

    LOG_INFO("PLUGINS", "Unloaded: %s", id);
    publish_event(mgr, OZAYN_EVENT_PLUGIN_UNLOADED);

    return OZAYN_OK;
}

/* ---------- Query ---------- */

int ozayn_plugin_manager_count(const ozayn_plugin_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->count;
}

const ozayn_plugin_record_t *ozayn_plugin_manager_get(const ozayn_plugin_manager_t *mgr,
                                                       int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->plugins[index];
}

const ozayn_plugin_record_t *ozayn_plugin_manager_find(const ozayn_plugin_manager_t *mgr,
                                                       const char *id) {
    if (!mgr || !id) return NULL;
    int idx = find_index(mgr, id);
    if (idx < 0) return NULL;
    return &mgr->plugins[idx];
}

int ozayn_plugin_manager_active_count(const ozayn_plugin_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->count; i++) {
        ozayn_plugin_state_t s = mgr->plugins[i].state;
        if (s == OZAYN_PLUGIN_RUNNING || s == OZAYN_PLUGIN_INITIALIZED)
            count++;
    }
    return count;
}
