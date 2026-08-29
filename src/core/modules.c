#include "modules.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <string.h>

/* ---------- State name ---------- */

const char *ozayn_module_state_name(ozayn_module_state_t state) {
    switch (state) {
        case OZAYN_MOD_REGISTERED:  return "REGISTERED";
        case OZAYN_MOD_INITIALIZED: return "INITIALIZED";
        case OZAYN_MOD_RUNNING:     return "RUNNING";
        case OZAYN_MOD_STOPPED:     return "STOPPED";
        case OZAYN_MOD_SHUTDOWN:    return "SHUTDOWN";
        case OZAYN_MOD_FAILED:      return "FAILED";
    }
    return "UNKNOWN";
}

/* ---------- Find by name ---------- */

static int find_index(const ozayn_module_manager_t *mgr, const char *name) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->modules[i].entry.name, name) == 0)
            return i;
    }
    return -1;
}

/* ---------- Publish helper ---------- */

static void publish_event(ozayn_module_manager_t *mgr, ozayn_event_type_t type) {
    if (mgr->engine.events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->engine.events,
                             type, OZAYN_SRC_MODULE, NULL);
    }
}

/* ---------- Init ---------- */

ozayn_result_t ozayn_module_manager_init(ozayn_module_manager_t *mgr) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_module_manager_t));
    mgr->initialized = 1;

    LOG_INFO("MODULES", "Module manager initialized");
    return OZAYN_OK;
}

/* ---------- Shutdown ---------- */

void ozayn_module_manager_shutdown(ozayn_module_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Shutdown all modules in reverse order */
    ozayn_module_manager_stop_all(mgr);

    for (int i = mgr->count - 1; i >= 0; i--) {
        ozayn_module_record_t *rec = &mgr->modules[i];

        if (rec->state == OZAYN_MOD_SHUTDOWN || rec->state == OZAYN_MOD_FAILED)
            continue;

        if (rec->entry.shutdown) {
            rec->entry.shutdown(&mgr->engine);
        }

        rec->state = OZAYN_MOD_SHUTDOWN;
        LOG_INFO("MODULES", "Module '%s' shut down", rec->entry.name);
    }

    mgr->initialized = 0;
    mgr->count = 0;

    LOG_INFO("MODULES", "Module manager shut down");
}

/* ---------- Engine bindings ---------- */

void ozayn_module_manager_set_logger(ozayn_module_manager_t *mgr, void *logger) {
    if (mgr) mgr->engine.logger = logger;
}

void ozayn_module_manager_set_events(ozayn_module_manager_t *mgr, void *events) {
    if (mgr) mgr->engine.events = events;
}

void ozayn_module_manager_set_recovery(ozayn_module_manager_t *mgr, void *recovery) {
    if (mgr) mgr->engine.recovery = recovery;
}

void ozayn_module_manager_set_config(ozayn_module_manager_t *mgr, void *config) {
    if (mgr) mgr->engine.config = config;
}

void ozayn_module_manager_set_runtime(ozayn_module_manager_t *mgr, void *runtime) {
    if (mgr) mgr->engine.runtime = runtime;
}

/* ---------- Registration ---------- */

ozayn_result_t ozayn_module_manager_register(ozayn_module_manager_t *mgr,
                                              const ozayn_module_entry_t *entry) {
    if (!mgr || !entry) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;
    if (mgr->count >= OZAYN_MAX_MODULES) {
        LOG_ERROR("MODULES", "Module limit reached (%d)", OZAYN_MAX_MODULES);
        return OZAYN_ERR;
    }
    if (!entry->name || !entry->init) {
        LOG_ERROR("MODULES", "Module entry missing name or init");
        return OZAYN_ERR;
    }

    /* Reject duplicates */
    if (find_index(mgr, entry->name) >= 0) {
        LOG_WARN("MODULES", "Duplicate module '%s' rejected", entry->name);
        return OZAYN_ERR_STATE;
    }

    ozayn_module_record_t *rec = &mgr->modules[mgr->count];
    memset(rec, 0, sizeof(ozayn_module_record_t));
    rec->entry = *entry;  /* shallow copy of function pointers + strings */
    rec->state = OZAYN_MOD_REGISTERED;
    rec->userdata = NULL;

    mgr->count++;

    LOG_INFO("MODULES", "Module '%s' v%s registered (slot %d)",
             entry->name, entry->version ? entry->version : "0.0",
             mgr->count - 1);

    publish_event(mgr, OZAYN_EVENT_MODULE_REGISTERED);
    return OZAYN_OK;
}

/* ---------- Unregister ---------- */

ozayn_result_t ozayn_module_manager_unregister(ozayn_module_manager_t *mgr,
                                                const char *name) {
    if (!mgr || !name) return OZAYN_ERR_NULL;

    int idx = find_index(mgr, name);
    if (idx < 0) {
        LOG_WARN("MODULES", "Module '%s' not found for unregister", name);
        return OZAYN_ERR;
    }

    ozayn_module_record_t *rec = &mgr->modules[idx];

    /* Shutdown if still alive */
    if (rec->state != OZAYN_MOD_SHUTDOWN && rec->state != OZAYN_MOD_FAILED) {
        if (rec->state == OZAYN_MOD_RUNNING && rec->entry.stop)
            rec->entry.stop(&mgr->engine);
        if (rec->entry.shutdown)
            rec->entry.shutdown(&mgr->engine);
        rec->state = OZAYN_MOD_SHUTDOWN;
    }

    /* Shift remaining modules down */
    for (int i = idx; i < mgr->count - 1; i++) {
        mgr->modules[i] = mgr->modules[i + 1];
    }
    mgr->count--;

    LOG_INFO("MODULES", "Module '%s' unregistered", name);
    return OZAYN_OK;
}

/* ---------- Init all ---------- */

ozayn_result_t ozayn_module_manager_init_all(ozayn_module_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    int failed = 0;

    for (int i = 0; i < mgr->count; i++) {
        ozayn_module_record_t *rec = &mgr->modules[i];

        if (rec->state != OZAYN_MOD_REGISTERED) continue;

        if (!rec->entry.init) {
            LOG_WARN("MODULES", "Module '%s' has no init, marking FAILED", rec->entry.name);
            rec->state = OZAYN_MOD_FAILED;
            failed++;
            continue;
        }

        ozayn_result_t r = rec->entry.init(&mgr->engine);
        if (r == OZAYN_OK) {
            rec->state = OZAYN_MOD_INITIALIZED;
            LOG_INFO("MODULES", "Module '%s' initialized", rec->entry.name);
            publish_event(mgr, OZAYN_EVENT_MODULE_INITIALIZED);
        } else {
            rec->state = OZAYN_MOD_FAILED;
            failed++;
            LOG_ERROR("MODULES", "Module '%s' init failed (result=%d)", rec->entry.name, r);

            /* Raise recovery error */
            if (mgr->engine.recovery) {
                ozayn_recovery_raise((ozayn_recovery_t *)mgr->engine.recovery,
                                     OZAYN_ERRCAT_MODULE, OZAYN_LOG_ERROR,
                                     OZAYN_SCOPE_COMPONENT,
                                     rec->entry.name, "Module init failed");
            }

            publish_event(mgr, OZAYN_EVENT_MODULE_FAILED);
        }
    }

    if (failed > 0) {
        LOG_WARN("MODULES", "%d module(s) failed initialization", failed);
    }

    return OZAYN_OK;
}

/* ---------- Start all ---------- */

ozayn_result_t ozayn_module_manager_start_all(ozayn_module_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    for (int i = 0; i < mgr->count; i++) {
        ozayn_module_record_t *rec = &mgr->modules[i];

        if (rec->state != OZAYN_MOD_INITIALIZED) continue;

        if (!rec->entry.start) {
            /* No start function — treat as running anyway */
            rec->state = OZAYN_MOD_RUNNING;
            continue;
        }

        ozayn_result_t r = rec->entry.start(&mgr->engine);
        if (r == OZAYN_OK) {
            rec->state = OZAYN_MOD_RUNNING;
            LOG_INFO("MODULES", "Module '%s' started", rec->entry.name);
            publish_event(mgr, OZAYN_EVENT_MODULE_STARTED);
        } else {
            rec->state = OZAYN_MOD_FAILED;
            LOG_ERROR("MODULES", "Module '%s' start failed", rec->entry.name);

            if (mgr->engine.recovery) {
                ozayn_recovery_raise((ozayn_recovery_t *)mgr->engine.recovery,
                                     OZAYN_ERRCAT_MODULE, OZAYN_LOG_ERROR,
                                     OZAYN_SCOPE_COMPONENT,
                                     rec->entry.name, "Module start failed");
            }

            publish_event(mgr, OZAYN_EVENT_MODULE_FAILED);
        }
    }

    return OZAYN_OK;
}

/* ---------- Stop all ---------- */

void ozayn_module_manager_stop_all(ozayn_module_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Reverse order */
    for (int i = mgr->count - 1; i >= 0; i--) {
        ozayn_module_record_t *rec = &mgr->modules[i];

        if (rec->state != OZAYN_MOD_RUNNING) continue;

        if (rec->entry.stop) {
            rec->entry.stop(&mgr->engine);
        }

        rec->state = OZAYN_MOD_STOPPED;
        LOG_INFO("MODULES", "Module '%s' stopped", rec->entry.name);
        publish_event(mgr, OZAYN_EVENT_MODULE_STOPPED);
    }
}

/* ---------- Query ---------- */

int ozayn_module_manager_count(const ozayn_module_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->count;
}

const ozayn_module_record_t *ozayn_module_manager_get(const ozayn_module_manager_t *mgr,
                                                       int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->modules[index];
}

const ozayn_module_record_t *ozayn_module_manager_find(const ozayn_module_manager_t *mgr,
                                                       const char *name) {
    if (!mgr || !name) return NULL;
    int idx = find_index(mgr, name);
    if (idx < 0) return NULL;
    return &mgr->modules[idx];
}

int ozayn_module_manager_active_count(const ozayn_module_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->modules[i].state == OZAYN_MOD_RUNNING ||
            mgr->modules[i].state == OZAYN_MOD_INITIALIZED)
            count++;
    }
    return count;
}
