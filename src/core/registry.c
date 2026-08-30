#include "registry.h"
#include "logger.h"
#include "recovery.h"
#include "events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * NAMES
 * ================================================================ */

const char *ozayn_service_state_name(ozayn_service_state_t state) {
    switch (state) {
        case OZAYN_SVC_REGISTERING: return "REGISTERING";
        case OZAYN_SVC_READY:       return "READY";
        case OZAYN_SVC_DEGRADED:    return "DEGRADED";
        case OZAYN_SVC_FAILED:      return "FAILED";
        case OZAYN_SVC_OFFLINE:     return "OFFLINE";
        case OZAYN_SVC_STOPPING:    return "STOPPING";
    }
    return "UNKNOWN";
}

/* ================================================================
 * FIND
 * ================================================================ */

static ozayn_service_record_t *find_service(ozayn_registry_manager_t *mgr, const char *id) {
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        if (mgr->services[i].active && strcmp(mgr->services[i].id, id) == 0)
            return &mgr->services[i];
    }
    return NULL;
}

static const ozayn_service_record_t *find_service_const(const ozayn_registry_manager_t *mgr, const char *id) {
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        if (mgr->services[i].active && strcmp(mgr->services[i].id, id) == 0)
            return &mgr->services[i];
    }
    return NULL;
}

static ozayn_service_record_t *alloc_record(ozayn_registry_manager_t *mgr) {
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        if (!mgr->services[i].active) {
            memset(&mgr->services[i], 0, sizeof(ozayn_service_record_t));
            mgr->services[i].active = 1;
            mgr->service_count++;
            return &mgr->services[i];
        }
    }
    return NULL;
}

/* ================================================================
 * INIT
 * ================================================================ */

ozayn_result_t ozayn_registry_init(ozayn_registry_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_registry_manager_t));
    mgr->enabled = enabled;

    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        mgr->services[i].active = 0;
        mgr->services[i].conn_fd = -1;
    }

    mgr->initialized = 1;

    if (enabled) {
        LOG_INFO("REGISTRY", "Service registry initialized (capacity=%d)",
                 OZAYN_REGISTRY_MAX_SERVICES);
    } else {
        LOG_INFO("REGISTRY", "Service registry disabled by configuration");
    }

    return OZAYN_OK;
}

/* ================================================================
 * SHUTDOWN
 * ================================================================ */

void ozayn_registry_shutdown(ozayn_registry_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Mark all services offline */
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        if (mgr->services[i].active) {
            LOG_INFO("REGISTRY", "Service '%s' removed on shutdown", mgr->services[i].id);
            mgr->services[i].active = 0;
        }
    }

    mgr->service_count = 0;
    mgr->initialized = 0;

    LOG_INFO("REGISTRY", "Service registry shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_registry_set_events(ozayn_registry_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_registry_set_recovery(ozayn_registry_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ================================================================
 * VALIDATE REGISTRATION
 * ================================================================ */

static ozayn_result_t validate_registration(const ozayn_service_registration_t *reg) {
    if (!reg) return OZAYN_ERR_NULL;

    /* Service ID required */
    if (reg->id[0] == '\0') {
        LOG_WARN("REGISTRY", "Registration rejected: missing service ID");
        return OZAYN_ERR;
    }

    /* Endpoint required */
    if (reg->endpoint[0] == '\0') {
        LOG_WARN("REGISTRY", "Registration rejected: missing endpoint for '%s'", reg->id);
        return OZAYN_ERR;
    }

    /* Protocol version must match */
    if (reg->protocol_version != OZAYN_IPC_VERSION) {
        LOG_WARN("REGISTRY", "Registration rejected: protocol mismatch for '%s' (got %d, need %d)",
                 reg->id, reg->protocol_version, OZAYN_IPC_VERSION);
        return OZAYN_ERR;
    }

    return OZAYN_OK;
}

/* ================================================================
 * REGISTER
 * ================================================================ */

ozayn_result_t ozayn_registry_register(ozayn_registry_manager_t *mgr,
                                        const ozayn_service_registration_t *reg,
                                        int conn_fd) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;
    if (!mgr->enabled) return OZAYN_ERR_STATE;
    if (!reg) return OZAYN_ERR_NULL;

    /* Validate */
    ozayn_result_t r = validate_registration(reg);
    if (r != OZAYN_OK) {
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_IPC, OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION, "REGISTRY",
                                 "Invalid service registration");
        }
        return r;
    }

    /* Check duplicate ID */
    if (find_service(mgr, reg->id)) {
        LOG_WARN("REGISTRY", "Duplicate service ID '%s' — rejected", reg->id);
        if (mgr->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                                 OZAYN_ERRCAT_IPC, OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION, "REGISTRY",
                                 "Duplicate service registration");
        }
        return OZAYN_ERR_STATE;
    }

    /* Allocate record */
    ozayn_service_record_t *rec = alloc_record(mgr);
    if (!rec) {
        LOG_ERROR("REGISTRY", "Service registry full — rejecting '%s'", reg->id);
        return OZAYN_ERR;
    }

    /* Copy data */
    snprintf(rec->id, sizeof(rec->id), "%s", reg->id);
    snprintf(rec->name, sizeof(rec->name), "%s", reg->name[0] ? reg->name : reg->id);
    snprintf(rec->version, sizeof(rec->version), "%s", reg->version[0] ? reg->version : "0.0.0");
    rec->protocol_version = reg->protocol_version;
    snprintf(rec->endpoint, sizeof(rec->endpoint), "%s", reg->endpoint);
    snprintf(rec->provider, sizeof(rec->provider), "%s", reg->provider[0] ? reg->provider : "unknown");
    rec->capability_count = 0;
    for (int i = 0; i < reg->capability_count && i < OZAYN_REGISTRY_MAX_CAPABILITIES; i++) {
        snprintf(rec->capabilities[i], sizeof(rec->capabilities[i]), "%s", reg->capabilities[i]);
        rec->capability_count++;
    }
    rec->conn_fd = conn_fd;
    rec->registered_at = time(NULL);
    rec->updated_at = rec->registered_at;
    rec->state = OZAYN_SVC_READY;

    LOG_INFO("REGISTRY", "Service registered: '%s' v%s (endpoint=%s, provider=%s, caps=%d)",
             rec->id, rec->version, rec->endpoint, rec->provider, rec->capability_count);

    /* Publish event */
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             OZAYN_EVENT_SERVICE_REGISTERED,
                             OZAYN_SRC_REGISTRY,
                             NULL);
    }

    return OZAYN_OK;
}

/* ================================================================
 * UNREGISTER
 * ================================================================ */

ozayn_result_t ozayn_registry_unregister(ozayn_registry_manager_t *mgr,
                                          const char *service_id) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;
    if (!service_id || service_id[0] == '\0') return OZAYN_ERR_NULL;

    ozayn_service_record_t *rec = find_service(mgr, service_id);
    if (!rec) {
        LOG_WARN("REGISTRY", "Unregister failed: service '%s' not found", service_id);
        return OZAYN_ERR;
    }

    LOG_INFO("REGISTRY", "Service unregistered: '%s'", rec->id);

    rec->active = 0;
    mgr->service_count--;

    /* Publish event */
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             OZAYN_EVENT_SERVICE_OFFLINE,
                             OZAYN_SRC_REGISTRY,
                             NULL);
    }

    return OZAYN_OK;
}

/* ================================================================
 * LOOKUP
 * ================================================================ */

const ozayn_service_record_t *ozayn_registry_lookup(const ozayn_registry_manager_t *mgr,
                                                     const char *service_id) {
    if (!mgr || !service_id || service_id[0] == '\0') return NULL;

    return find_service_const(mgr, service_id);
}

/* ================================================================
 * FIND BY CAPABILITY
 * ================================================================ */

const ozayn_service_record_t *ozayn_registry_find_by_capability(const ozayn_registry_manager_t *mgr,
                                                                 const char *capability) {
    if (!mgr || !capability || capability[0] == '\0') return NULL;

    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        const ozayn_service_record_t *rec = &mgr->services[i];
        if (!rec->active) continue;
        if (rec->state != OZAYN_SVC_READY) continue;

        for (int c = 0; c < rec->capability_count; c++) {
            if (strcmp(rec->capabilities[c], capability) == 0)
                return rec;
        }
    }
    return NULL;
}

/* ================================================================
 * LIST
 * ================================================================ */

int ozayn_registry_list(const ozayn_registry_manager_t *mgr,
                         const ozayn_service_record_t **out,
                         int max_out) {
    if (!mgr || !out) return 0;

    int count = 0;
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES && count < max_out; i++) {
        if (mgr->services[i].active) {
            out[count++] = &mgr->services[i];
        }
    }
    return count;
}

/* ================================================================
 * UPDATE STATE
 * ================================================================ */

ozayn_result_t ozayn_registry_update_state(ozayn_registry_manager_t *mgr,
                                            const char *service_id,
                                            ozayn_service_state_t state) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    ozayn_service_record_t *rec = find_service(mgr, service_id);
    if (!rec) return OZAYN_ERR;

    ozayn_service_state_t old_state = rec->state;
    rec->state = state;
    rec->updated_at = time(NULL);

    LOG_INFO("REGISTRY", "Service '%s' state: %s -> %s",
             rec->id,
             ozayn_service_state_name(old_state),
             ozayn_service_state_name(state));

    /* Publish appropriate event */
    if (mgr->events) {
        ozayn_event_type_t event = OZAYN_EVENT_NONE;
        switch (state) {
            case OZAYN_SVC_READY:    event = OZAYN_EVENT_SERVICE_READY; break;
            case OZAYN_SVC_DEGRADED: event = OZAYN_EVENT_SERVICE_DEGRADED; break;
            case OZAYN_SVC_FAILED:   event = OZAYN_EVENT_SERVICE_FAILED; break;
            case OZAYN_SVC_OFFLINE:  event = OZAYN_EVENT_SERVICE_OFFLINE; break;
            default: break;
        }
        if (event != OZAYN_EVENT_NONE) {
            ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                 event, OZAYN_SRC_REGISTRY, NULL);
        }
    }

    return OZAYN_OK;
}

/* ================================================================
 * ON DISCONNECT
 * ================================================================ */

ozayn_result_t ozayn_registry_on_disconnect(ozayn_registry_manager_t *mgr, int conn_fd) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_STATE;

    int found = 0;
    for (int i = 0; i < OZAYN_REGISTRY_MAX_SERVICES; i++) {
        if (mgr->services[i].active && mgr->services[i].conn_fd == conn_fd) {
            LOG_INFO("REGISTRY", "Service '%s' owner disconnected (fd=%d) — marking OFFLINE",
                     mgr->services[i].id, conn_fd);

            mgr->services[i].state = OZAYN_SVC_OFFLINE;
            mgr->services[i].conn_fd = -1;
            mgr->services[i].updated_at = time(NULL);
            found = 1;

            /* Publish event */
            if (mgr->events) {
                ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                     OZAYN_EVENT_SERVICE_OFFLINE,
                                     OZAYN_SRC_REGISTRY,
                                     NULL);
            }
        }
    }

    return found ? OZAYN_OK : OZAYN_ERR;
}

/* ================================================================
 * QUERY
 * ================================================================ */

int ozayn_registry_count(const ozayn_registry_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->service_count;
}

int ozayn_registry_is_enabled(const ozayn_registry_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->enabled;
}
