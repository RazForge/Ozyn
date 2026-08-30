#include "resource.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * NAMES
 * ================================================================ */

const char *ozayn_resource_type_name(ozayn_resource_type_t type) {
    switch (type) {
        case OZAYN_RESOURCE_TYPE_UNKNOWN: return "UNKNOWN";
        case OZAYN_RESOURCE_TYPE_PROCESS: return "PROCESS";
        case OZAYN_RESOURCE_TYPE_TASK:    return "TASK";
        case OZAYN_RESOURCE_TYPE_IPC:     return "IPC";
        case OZAYN_RESOURCE_TYPE_MODULE:  return "MODULE";
        case OZAYN_RESOURCE_TYPE_PLUGIN:  return "PLUGIN";
        case OZAYN_RESOURCE_TYPE_SERVICE: return "SERVICE";
        case OZAYN_RESOURCE_TYPE_BUFFER:  return "BUFFER";
        case OZAYN_RESOURCE_TYPE_DEVICE:  return "DEVICE";
        case OZAYN_RESOURCE_TYPE_HANDLE:  return "HANDLE";
    }
    return "UNKNOWN";
}

const char *ozayn_resource_state_name(ozayn_resource_state_t state) {
    switch (state) {
        case OZAYN_RESOURCE_STATE_CREATED:    return "CREATED";
        case OZAYN_RESOURCE_STATE_AVAILABLE:  return "AVAILABLE";
        case OZAYN_RESOURCE_STATE_ALLOCATED:  return "ALLOCATED";
        case OZAYN_RESOURCE_STATE_ACTIVE:     return "ACTIVE";
        case OZAYN_RESOURCE_STATE_RELEASING:  return "RELEASING";
        case OZAYN_RESOURCE_STATE_DESTROYING: return "DESTROYING";
        case OZAYN_RESOURCE_STATE_DESTROYED:  return "DESTROYED";
        case OZAYN_RESOURCE_STATE_FAILED:     return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_resource_result_name(ozayn_resource_result_t result) {
    switch (result) {
        case OZAYN_RESOURCE_OK:               return "OK";
        case OZAYN_RESOURCE_ERR:              return "ERROR";
        case OZAYN_RESOURCE_NOT_FOUND:        return "NOT_FOUND";
        case OZAYN_RESOURCE_UNAVAILABLE:      return "UNAVAILABLE";
        case OZAYN_RESOURCE_UNAUTHORIZED:     return "UNAUTHORIZED";
        case OZAYN_RESOURCE_INVALID_STATE:    return "INVALID_STATE";
        case OZAYN_RESOURCE_INVALID_TYPE:     return "INVALID_TYPE";
        case OZAYN_RESOURCE_ALREADY_OWNED:    return "ALREADY_OWNED";
        case OZAYN_RESOURCE_NOT_OWNER:        return "NOT_OWNER";
        case OZAYN_RESOURCE_ALREADY_RELEASED: return "ALREADY_RELEASED";
        case OZAYN_RESOURCE_EXHAUSTED:        return "EXHAUSTED";
    }
    return "UNKNOWN";
}

/* ================================================================
 * EVENT HELPERS
 * ================================================================ */

static void publish_resource_event(ozayn_resource_manager_t *mgr,
                                   ozayn_event_type_t type,
                                   const ozayn_resource_record_t *rec) {
    if (mgr && mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             type, OZAYN_SRC_CORE, (void *)rec);
    }
}

/* ================================================================
 * STATE VALIDATION
 * ================================================================ */

static int is_valid_transition(ozayn_resource_state_t from, ozayn_resource_state_t to) {
    switch (from) {
        case OZAYN_RESOURCE_STATE_CREATED:
            return to == OZAYN_RESOURCE_STATE_AVAILABLE ||
                   to == OZAYN_RESOURCE_STATE_FAILED;
        case OZAYN_RESOURCE_STATE_AVAILABLE:
            return to == OZAYN_RESOURCE_STATE_ALLOCATED ||
                   to == OZAYN_RESOURCE_STATE_DESTROYING;
        case OZAYN_RESOURCE_STATE_ALLOCATED:
            return to == OZAYN_RESOURCE_STATE_ACTIVE ||
                   to == OZAYN_RESOURCE_STATE_RELEASING ||
                   to == OZAYN_RESOURCE_STATE_FAILED;
        case OZAYN_RESOURCE_STATE_ACTIVE:
            return to == OZAYN_RESOURCE_STATE_RELEASING ||
                   to == OZAYN_RESOURCE_STATE_FAILED;
        case OZAYN_RESOURCE_STATE_RELEASING:
            return to == OZAYN_RESOURCE_STATE_AVAILABLE ||
                   to == OZAYN_RESOURCE_STATE_DESTROYED;
        case OZAYN_RESOURCE_STATE_DESTROYING:
            return to == OZAYN_RESOURCE_STATE_DESTROYED;
        case OZAYN_RESOURCE_STATE_DESTROYED:
            return 0; /* terminal */
        case OZAYN_RESOURCE_STATE_FAILED:
            return to == OZAYN_RESOURCE_STATE_RELEASING ||
                   to == OZAYN_RESOURCE_STATE_DESTROYING;
    }
    return 0;
}

/* ================================================================
 * LOOKUP HELPERS
 * ================================================================ */

static ozayn_resource_record_t *find_slot(ozayn_resource_manager_t *mgr, const char *resource_id) {
    if (!mgr || !resource_id) return NULL;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active &&
            strcmp(mgr->resources[i].resource_id, resource_id) == 0) {
            return &mgr->resources[i];
        }
    }
    return NULL;
}

static const ozayn_resource_record_t *find_slot_const(const ozayn_resource_manager_t *mgr,
                                                       const char *resource_id) {
    if (!mgr || !resource_id) return NULL;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active &&
            strcmp(mgr->resources[i].resource_id, resource_id) == 0) {
            return &mgr->resources[i];
        }
    }
    return NULL;
}

static void update_stats(ozayn_resource_manager_t *mgr) {
    memset(&mgr->stats, 0, sizeof(ozayn_resource_stats_t));
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (!mgr->resources[i].active) continue;
        mgr->stats.total++;
        switch (mgr->resources[i].state) {
            case OZAYN_RESOURCE_STATE_AVAILABLE:
                mgr->stats.available++;
                break;
            case OZAYN_RESOURCE_STATE_ALLOCATED:
                mgr->stats.allocated++;
                break;
            case OZAYN_RESOURCE_STATE_ACTIVE:
                mgr->stats.active++;
                break;
            case OZAYN_RESOURCE_STATE_FAILED:
                mgr->stats.failed++;
                break;
            case OZAYN_RESOURCE_STATE_DESTROYED:
            case OZAYN_RESOURCE_STATE_RELEASING:
            case OZAYN_RESOURCE_STATE_DESTROYING:
                mgr->stats.released++;
                break;
            default:
                break;
        }
    }
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

ozayn_result_t ozayn_resource_manager_init(ozayn_resource_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_resource_manager_t));
    mgr->enabled = enabled;
    mgr->next_id = 1;
    mgr->initialized = 1;

    LOG_INFO("RESOURCE", "Resource manager initialized (enabled=%s, capacity=%d)",
             enabled ? "yes" : "no", OZAYN_RESOURCE_MAX);

    return OZAYN_OK;
}

void ozayn_resource_manager_shutdown(ozayn_resource_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Warn about active resources */
    int active_count = 0;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active &&
            mgr->resources[i].state != OZAYN_RESOURCE_STATE_DESTROYED &&
            mgr->resources[i].state != OZAYN_RESOURCE_STATE_AVAILABLE) {
            active_count++;
        }
    }
    if (active_count > 0) {
        LOG_WARN("RESOURCE", "%d resource(s) still active at shutdown", active_count);
    }

    /* Execute cleanup callbacks and clear */
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active) {
            if (mgr->resources[i].cleanup) {
                mgr->resources[i].cleanup(&mgr->resources[i]);
            }
            mgr->resources[i].active = 0;
        }
    }

    mgr->resource_count = 0;
    mgr->initialized = 0;

    LOG_INFO("RESOURCE", "Resource manager shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_resource_manager_set_events(ozayn_resource_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_resource_manager_set_recovery(ozayn_resource_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

void ozayn_resource_manager_set_authorization(ozayn_resource_manager_t *mgr, void *authorization) {
    if (mgr) mgr->authorization = authorization;
}

/* ================================================================
 * RESOURCE LIFECYCLE
 * ================================================================ */

ozayn_resource_result_t ozayn_resource_create(ozayn_resource_manager_t *mgr,
                                               const char *resource_id,
                                               const char *name,
                                               ozayn_resource_type_t type,
                                               int exclusive) {
    if (!mgr || !resource_id) return OZAYN_RESOURCE_ERR;
    if (!mgr->initialized) return OZAYN_RESOURCE_ERR;
    if (type == OZAYN_RESOURCE_TYPE_UNKNOWN) return OZAYN_RESOURCE_INVALID_TYPE;

    /* Check duplicate */
    if (find_slot(mgr, resource_id)) {
        LOG_WARN("RESOURCE", "Resource '%s' already exists", resource_id);
        return OZAYN_RESOURCE_ERR;
    }

    /* Find free slot */
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (!mgr->resources[i].active) {
            ozayn_resource_record_t *rec = &mgr->resources[i];
            memset(rec, 0, sizeof(ozayn_resource_record_t));

            rec->active = 1;
            rec->id = mgr->next_id++;
            rec->generation = 1;
            strncpy(rec->resource_id, resource_id, OZAYN_RESOURCE_MAX_ID_LEN - 1);
            if (name) strncpy(rec->name, name, OZAYN_RESOURCE_MAX_NAME_LEN - 1);
            rec->type = type;
            rec->state = OZAYN_RESOURCE_STATE_AVAILABLE;
            rec->exclusive = exclusive;
            rec->ref_count = 0;
            rec->created_at = time(NULL);

            mgr->resource_count++;
            update_stats(mgr);

            LOG_INFO("RESOURCE", "Created '%s' (type=%s, exclusive=%s)",
                     resource_id, ozayn_resource_type_name(type),
                     exclusive ? "yes" : "no");

            publish_resource_event(mgr, OZAYN_EVENT_RESOURCE_CREATED, rec);

            return OZAYN_RESOURCE_OK;
        }
    }

    LOG_ERROR("RESOURCE", "Resource registry full — cannot create '%s'", resource_id);
    if (mgr->recovery) {
        ozayn_recovery_raise((ozayn_recovery_t *)mgr->recovery,
                             OZAYN_ERRCAT_RESOURCE, OZAYN_LOG_ERROR,
                             OZAYN_SCOPE_OPERATION, "RESOURCE",
                             "Resource registry full");
    }
    return OZAYN_RESOURCE_EXHAUSTED;
}

ozayn_resource_result_t ozayn_resource_destroy(ozayn_resource_manager_t *mgr,
                                                const char *resource_id,
                                                const char *identity_id) {
    if (!mgr || !resource_id) return OZAYN_RESOURCE_ERR;

    ozayn_resource_record_t *rec = find_slot(mgr, resource_id);
    if (!rec) return OZAYN_RESOURCE_NOT_FOUND;

    /* Only owner or CORE_ADMIN can destroy */
    if (identity_id && rec->owner[0] &&
        strcmp(rec->owner, identity_id) != 0) {
        /* Check authorization if available */
        if (mgr->authorization) {
            ozayn_authorization_manager_t *authz = (ozayn_authorization_manager_t *)mgr->authorization;
            ozayn_authz_result_t az = ozayn_authorize(authz, identity_id, "resource", "destroy");
            if (az.decision != OZAYN_AUTHZ_ALLOW) {
                LOG_WARN("RESOURCE", "DENY: '%s' cannot destroy '%s' (not owner, not authorized)",
                         identity_id, resource_id);
                return OZAYN_RESOURCE_UNAUTHORIZED;
            }
        } else {
            return OZAYN_RESOURCE_NOT_OWNER;
        }
    }

    /* Validate state */
    if (rec->state == OZAYN_RESOURCE_STATE_DESTROYED) {
        return OZAYN_RESOURCE_ALREADY_RELEASED;
    }

    /* Execute cleanup callback */
    if (rec->cleanup) {
        rec->cleanup(rec);
    }

    LOG_INFO("RESOURCE", "Destroyed '%s' (was %s, owner=%s)",
             resource_id, ozayn_resource_state_name(rec->state),
             rec->owner[0] ? rec->owner : "none");

    rec->state = OZAYN_RESOURCE_STATE_DESTROYED;
    publish_resource_event(mgr, OZAYN_EVENT_RESOURCE_RELEASED, rec);

    /* Remove from registry */
    rec->active = 0;
    mgr->resource_count--;
    update_stats(mgr);

    return OZAYN_RESOURCE_OK;
}

ozayn_resource_result_t ozayn_resource_allocate(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *identity_id) {
    if (!mgr || !resource_id || !identity_id) return OZAYN_RESOURCE_ERR;

    ozayn_resource_record_t *rec = find_slot(mgr, resource_id);
    if (!rec) return OZAYN_RESOURCE_NOT_FOUND;

    /* Check authorization if available */
    if (mgr->authorization) {
        ozayn_authorization_manager_t *authz = (ozayn_authorization_manager_t *)mgr->authorization;
        ozayn_authz_result_t az = ozayn_authorize(authz, identity_id, "resource", "allocate");
        if (az.decision != OZAYN_AUTHZ_ALLOW) {
            LOG_WARN("RESOURCE", "DENY: '%s' cannot allocate '%s'",
                     identity_id, resource_id);
            return OZAYN_RESOURCE_UNAUTHORIZED;
        }
    }

    /* Validate state */
    if (rec->state != OZAYN_RESOURCE_STATE_AVAILABLE) {
        LOG_WARN("RESOURCE", "Cannot allocate '%s': state is %s (need AVAILABLE)",
                 resource_id, ozayn_resource_state_name(rec->state));
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    /* Check exclusive */
    if (rec->exclusive && rec->ref_count > 0) {
        LOG_WARN("RESOURCE", "Cannot allocate '%s': exclusive and already owned by '%s'",
                 resource_id, rec->owner);
        return OZAYN_RESOURCE_UNAVAILABLE;
    }

    /* Allocate */
    if (!is_valid_transition(rec->state, OZAYN_RESOURCE_STATE_ALLOCATED)) {
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    rec->state = OZAYN_RESOURCE_STATE_ALLOCATED;
    strncpy(rec->owner, identity_id, OZAYN_RESOURCE_MAX_OWNER_LEN - 1);
    rec->ref_count++;
    rec->allocated_at = time(NULL);
    update_stats(mgr);

    LOG_INFO("RESOURCE", "Allocated '%s' to '%s' (ref_count=%d)",
             resource_id, identity_id, rec->ref_count);

    publish_resource_event(mgr, OZAYN_EVENT_RESOURCE_ALLOCATED, rec);

    return OZAYN_RESOURCE_OK;
}

ozayn_resource_result_t ozayn_resource_release(ozayn_resource_manager_t *mgr,
                                                const char *resource_id,
                                                const char *identity_id) {
    if (!mgr || !resource_id) return OZAYN_RESOURCE_ERR;

    ozayn_resource_record_t *rec = find_slot(mgr, resource_id);
    if (!rec) return OZAYN_RESOURCE_NOT_FOUND;

    /* Validate state */
    if (rec->state == OZAYN_RESOURCE_STATE_AVAILABLE ||
        rec->state == OZAYN_RESOURCE_STATE_DESTROYED) {
        return OZAYN_RESOURCE_ALREADY_RELEASED;
    }

    if (rec->state != OZAYN_RESOURCE_STATE_ALLOCATED &&
        rec->state != OZAYN_RESOURCE_STATE_ACTIVE) {
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    /* Check owner (if identity provided) */
    if (identity_id && rec->owner[0] &&
        strcmp(rec->owner, identity_id) != 0) {
        /* Allow release if not exclusive, or if authorized */
        if (rec->exclusive) {
            return OZAYN_RESOURCE_NOT_OWNER;
        }
    }

    /* Decrement ref count */
    rec->ref_count--;
    rec->released_at = time(NULL);

    if (rec->ref_count <= 0) {
        /* Transition to AVAILABLE */
        rec->ref_count = 0;
        if (is_valid_transition(rec->state, OZAYN_RESOURCE_STATE_RELEASING)) {
            rec->state = OZAYN_RESOURCE_STATE_RELEASING;
        }
        if (is_valid_transition(rec->state, OZAYN_RESOURCE_STATE_AVAILABLE)) {
            rec->state = OZAYN_RESOURCE_STATE_AVAILABLE;
        }
        rec->owner[0] = '\0';
    }

    update_stats(mgr);

    LOG_INFO("RESOURCE", "Released '%s' (ref_count=%d, owner=%s)",
             resource_id, rec->ref_count,
             rec->owner[0] ? rec->owner : "none");

    publish_resource_event(mgr, OZAYN_EVENT_RESOURCE_RELEASED, rec);

    return OZAYN_RESOURCE_OK;
}

ozayn_resource_result_t ozayn_resource_activate(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *identity_id) {
    if (!mgr || !resource_id) return OZAYN_RESOURCE_ERR;

    ozayn_resource_record_t *rec = find_slot(mgr, resource_id);
    if (!rec) return OZAYN_RESOURCE_NOT_FOUND;

    /* Must be allocated */
    if (rec->state != OZAYN_RESOURCE_STATE_ALLOCATED) {
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    /* Must be owner */
    if (identity_id && rec->owner[0] &&
        strcmp(rec->owner, identity_id) != 0) {
        return OZAYN_RESOURCE_NOT_OWNER;
    }

    if (!is_valid_transition(rec->state, OZAYN_RESOURCE_STATE_ACTIVE)) {
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    rec->state = OZAYN_RESOURCE_STATE_ACTIVE;
    update_stats(mgr);

    LOG_INFO("RESOURCE", "Activated '%s' (owner=%s)", resource_id, rec->owner);

    return OZAYN_RESOURCE_OK;
}

ozayn_resource_result_t ozayn_resource_transfer(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *from_identity,
                                                 const char *to_identity) {
    if (!mgr || !resource_id || !from_identity || !to_identity) return OZAYN_RESOURCE_ERR;

    ozayn_resource_record_t *rec = find_slot(mgr, resource_id);
    if (!rec) return OZAYN_RESOURCE_NOT_FOUND;

    /* Must be current owner */
    if (rec->owner[0] && strcmp(rec->owner, from_identity) != 0) {
        return OZAYN_RESOURCE_NOT_OWNER;
    }

    /* Must be in a transferable state */
    if (rec->state != OZAYN_RESOURCE_STATE_ALLOCATED &&
        rec->state != OZAYN_RESOURCE_STATE_ACTIVE) {
        return OZAYN_RESOURCE_INVALID_STATE;
    }

    /* Check authorization for both parties */
    if (mgr->authorization) {
        ozayn_authorization_manager_t *authz = (ozayn_authorization_manager_t *)mgr->authorization;
        ozayn_authz_result_t az_from = ozayn_authorize(authz, from_identity, "resource", "transfer");
        if (az_from.decision != OZAYN_AUTHZ_ALLOW) {
            return OZAYN_RESOURCE_UNAUTHORIZED;
        }
        ozayn_authz_result_t az_to = ozayn_authorize(authz, to_identity, "resource", "allocate");
        if (az_to.decision != OZAYN_AUTHZ_ALLOW) {
            return OZAYN_RESOURCE_UNAUTHORIZED;
        }
    }

    /* Transfer */
    LOG_INFO("RESOURCE", "Transferred '%s' from '%s' to '%s'",
             resource_id, from_identity, to_identity);

    strncpy(rec->owner, to_identity, OZAYN_RESOURCE_MAX_OWNER_LEN - 1);

    publish_resource_event(mgr, OZAYN_EVENT_RESOURCE_ALLOCATED, rec);

    return OZAYN_RESOURCE_OK;
}

/* ================================================================
 * QUERY
 * ================================================================ */

const ozayn_resource_record_t *ozayn_resource_find(const ozayn_resource_manager_t *mgr,
                                                    const char *resource_id) {
    return find_slot_const(mgr, resource_id);
}

int ozayn_resource_is_available(const ozayn_resource_manager_t *mgr, const char *resource_id) {
    const ozayn_resource_record_t *rec = find_slot_const(mgr, resource_id);
    if (!rec) return 0;
    return rec->state == OZAYN_RESOURCE_STATE_AVAILABLE;
}

int ozayn_resource_exists(const ozayn_resource_manager_t *mgr, const char *resource_id) {
    return find_slot_const(mgr, resource_id) != NULL;
}

const char *ozayn_resource_owner(const ozayn_resource_manager_t *mgr, const char *resource_id) {
    const ozayn_resource_record_t *rec = find_slot_const(mgr, resource_id);
    if (!rec) return NULL;
    return rec->owner[0] ? rec->owner : NULL;
}

/* ================================================================
 * HANDLE-BASED API
 * ================================================================ */

ozayn_resource_handle_t ozayn_resource_get_handle(const ozayn_resource_record_t *rec) {
    ozayn_resource_handle_t h = { 0, 0 };
    if (rec) {
        h.slot = rec->id;
        h.generation = rec->generation;
    }
    return h;
}

const ozayn_resource_record_t *ozayn_resource_from_handle(const ozayn_resource_manager_t *mgr,
                                                          ozayn_resource_handle_t handle) {
    if (!mgr) return NULL;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active &&
            mgr->resources[i].id == handle.slot &&
            mgr->resources[i].generation == handle.generation) {
            return &mgr->resources[i];
        }
    }
    return NULL; /* stale or invalid handle */
}

/* ================================================================
 * STATISTICS
 * ================================================================ */

ozayn_resource_stats_t ozayn_resource_manager_stats(const ozayn_resource_manager_t *mgr) {
    ozayn_resource_stats_t zero = { 0 };
    if (!mgr) return zero;
    return mgr->stats;
}

int ozayn_resource_manager_count(const ozayn_resource_manager_t *mgr) {
    return mgr ? mgr->resource_count : 0;
}

/* ================================================================
 * QUERY BY TYPE / OWNER
 * ================================================================ */

int ozayn_resource_count_by_type(const ozayn_resource_manager_t *mgr, ozayn_resource_type_t type) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active && mgr->resources[i].type == type) {
            count++;
        }
    }
    return count;
}

int ozayn_resource_count_by_owner(const ozayn_resource_manager_t *mgr, const char *identity_id) {
    if (!mgr || !identity_id) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_RESOURCE_MAX; i++) {
        if (mgr->resources[i].active &&
            strcmp(mgr->resources[i].owner, identity_id) == 0) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * ORPHAN DETECTION
 * ================================================================ */

int ozayn_resource_detect_orphans(ozayn_resource_manager_t *mgr,
                                   const char *dead_identity,
                                   ozayn_resource_record_t *orphan_list,
                                   int max_orphans) {
    if (!mgr || !dead_identity || !orphan_list) return 0;

    int found = 0;
    for (int i = 0; i < OZAYN_RESOURCE_MAX && found < max_orphans; i++) {
        if (mgr->resources[i].active &&
            mgr->resources[i].owner[0] &&
            strcmp(mgr->resources[i].owner, dead_identity) == 0) {
            orphan_list[found] = mgr->resources[i];
            found++;
        }
    }

    if (found > 0) {
        LOG_WARN("RESOURCE", "Detected %d orphaned resource(s) from '%s'",
                 found, dead_identity);
    }

    return found;
}
