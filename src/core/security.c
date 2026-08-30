#include "security.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * NAME FUNCTIONS
 * ================================================================ */

const char *ozayn_identity_type_name(ozayn_identity_type_t type) {
    switch (type) {
        case OZAYN_IDENTITY_CORE:     return "CORE";
        case OZAYN_IDENTITY_MODULE:   return "MODULE";
        case OZAYN_IDENTITY_PLUGIN:   return "PLUGIN";
        case OZAYN_IDENTITY_SERVICE:  return "SERVICE";
        case OZAYN_IDENTITY_EXTERNAL: return "EXTERNAL";
        case OZAYN_IDENTITY_USER:     return "USER";
    }
    return "UNKNOWN";
}

const char *ozayn_trust_state_name(ozayn_trust_state_t state) {
    switch (state) {
        case OZAYN_TRUST_UNREGISTERED: return "UNREGISTERED";
        case OZAYN_TRUST_PENDING:      return "PENDING";
        case OZAYN_TRUST_TRUSTED:      return "TRUSTED";
        case OZAYN_TRUST_REVOKED:      return "REVOKED";
        case OZAYN_TRUST_DENIED:       return "DENIED";
    }
    return "UNKNOWN";
}

const char *ozayn_auth_method_name(ozayn_auth_method_t method) {
    switch (method) {
        case OZAYN_AUTH_NONE:       return "none";
        case OZAYN_AUTH_TRUST:      return "trust-all";
        case OZAYN_AUTH_UID:        return "uid-check";
        case OZAYN_AUTH_CREDENTIAL: return "credential";
    }
    return "unknown";
}

const char *ozayn_auth_result_name(ozayn_auth_result_t result) {
    switch (result) {
        case OZAYN_AUTH_OK:              return "SUCCESS";
        case OZAYN_AUTH_ERR_NOT_FOUND:   return "NOT_FOUND";
        case OZAYN_AUTH_ERR_REVOKED:     return "REVOKED";
        case OZAYN_AUTH_ERR_CREDENTIAL:  return "CREDENTIAL_MISMATCH";
        case OZAYN_AUTH_ERR_DENIED:      return "DENIED";
        case OZAYN_AUTH_ERR_DISABLED:    return "DISABLED";
        case OZAYN_AUTH_ERR_POLICY:      return "POLICY_VIOLATION";
    }
    return "UNKNOWN";
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

ozayn_result_t ozayn_security_init(ozayn_security_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_security_manager_t));
    mgr->enabled = enabled;
    mgr->auth_mode = OZAYN_AUTH_UID;
    mgr->audit_logging = 1;
    mgr->initialized = 1;

    /* Register Core identity by default */
    ozayn_security_register_identity(mgr, "ozayn.core", "OZAYN Core",
                                     OZAYN_IDENTITY_CORE, OZAYN_AUTH_TRUST, 0, 0);

    LOG_INFO("SECURITY", "Security manager initialized (enabled=%s, mode=%s)",
             enabled ? "yes" : "no",
             ozayn_auth_method_name(mgr->auth_mode));

    return OZAYN_OK;
}

void ozayn_security_shutdown(ozayn_security_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Revoke all identities on shutdown */
    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active) {
            mgr->identities[i].trust_state = OZAYN_TRUST_REVOKED;
            mgr->identities[i].revoked_at = time(NULL);
        }
    }

    mgr->initialized = 0;
    LOG_INFO("SECURITY", "Security manager shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_security_set_events(ozayn_security_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_security_set_recovery(ozayn_security_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ================================================================
 * CONFIGURATION
 * ================================================================ */

void ozayn_security_set_auth_mode(ozayn_security_manager_t *mgr, ozayn_auth_method_t mode) {
    if (mgr) {
        mgr->auth_mode = mode;
        LOG_INFO("SECURITY", "Auth mode set to %s", ozayn_auth_method_name(mode));
    }
}

void ozayn_security_set_allowed_uid(ozayn_security_manager_t *mgr, uint32_t uid) {
    if (!mgr) return;
    if (mgr->allowed_uid_count < 16) {
        mgr->allowed_uids[mgr->allowed_uid_count++] = uid;
        LOG_INFO("SECURITY", "Allowed UID added: %u", uid);
    }
}

void ozayn_security_set_audit_logging(ozayn_security_manager_t *mgr, int enabled) {
    if (mgr) mgr->audit_logging = enabled;
}

/* ================================================================
 * IDENTITY MANAGEMENT
 * ================================================================ */

ozayn_result_t ozayn_security_register_identity(ozayn_security_manager_t *mgr,
                                                 const char *id,
                                                 const char *name,
                                                 ozayn_identity_type_t type,
                                                 ozayn_auth_method_t auth_method,
                                                 uint32_t auth_uid,
                                                 uint32_t auth_gid) {
    if (!mgr || !id) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Check for duplicate */
    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {
            LOG_WARN("SECURITY", "Duplicate identity '%s' — rejected", id);
            return OZAYN_ERR_STATE;
        }
    }

    /* Find free slot */
    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (!mgr->identities[i].active) {
            ozayn_identity_record_t *rec = &mgr->identities[i];
            memset(rec, 0, sizeof(ozayn_identity_record_t));

            rec->active = 1;
            strncpy(rec->id, id, OZAYN_SECURITY_MAX_ID_LEN - 1);
            if (name) strncpy(rec->name, name, OZAYN_SECURITY_MAX_NAME_LEN - 1);
            else snprintf(rec->name, OZAYN_SECURITY_MAX_NAME_LEN, "%s", id);
            rec->type = type;
            rec->trust_state = OZAYN_TRUST_TRUSTED;
            rec->auth_method = auth_method;
            rec->auth_uid = auth_uid;
            rec->auth_gid = auth_gid;
            rec->created_at = time(NULL);

            mgr->identity_count++;

            if (mgr->audit_logging) {
                LOG_INFO("SECURITY", "Identity registered: '%s' (type=%s, auth=%s, uid=%u)",
                         id, ozayn_identity_type_name(type),
                         ozayn_auth_method_name(auth_method), auth_uid);
            }

            /* Publish event */
            if (mgr->events) {
                ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                     OZAYN_EVENT_IDENTITY_REGISTERED,
                                     OZAYN_SRC_SECURITY, (void *)id);
            }

            return OZAYN_OK;
        }
    }

    LOG_ERROR("SECURITY", "Identity registry full — cannot register '%s'", id);
    return OZAYN_ERR;
}

ozayn_result_t ozayn_security_revoke_identity(ozayn_security_manager_t *mgr, const char *id) {
    if (!mgr || !id) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {

            if (mgr->identities[i].type == OZAYN_IDENTITY_CORE) {
                LOG_WARN("SECURITY", "Cannot revoke Core identity");
                return OZAYN_ERR_STATE;
            }

            mgr->identities[i].trust_state = OZAYN_TRUST_REVOKED;
            mgr->identities[i].revoked_at = time(NULL);

            if (mgr->audit_logging) {
                LOG_WARN("SECURITY", "Identity revoked: '%s'", id);
            }

            /* Publish event */
            if (mgr->events) {
                ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                     OZAYN_EVENT_IDENTITY_REVOKED,
                                     OZAYN_SRC_SECURITY, (void *)id);
            }

            return OZAYN_OK;
        }
    }

    return OZAYN_ERR;
}

ozayn_result_t ozayn_security_remove_identity(ozayn_security_manager_t *mgr, const char *id) {
    if (!mgr || !id) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {

            if (mgr->identities[i].type == OZAYN_IDENTITY_CORE) {
                LOG_WARN("SECURITY", "Cannot remove Core identity");
                return OZAYN_ERR_STATE;
            }

            mgr->identities[i].active = 0;
            mgr->identity_count--;

            if (mgr->audit_logging) {
                LOG_INFO("SECURITY", "Identity removed: '%s'", id);
            }

            return OZAYN_OK;
        }
    }

    return OZAYN_ERR;
}

/* ================================================================
 * AUTHENTICATION
 * ================================================================ */

ozayn_auth_result_t ozayn_security_authenticate(ozayn_security_manager_t *mgr,
                                                 const char *claimed_id,
                                                 const ozayn_peer_creds_t *creds) {
    if (!mgr || !claimed_id) return OZAYN_AUTH_ERR_DISABLED;
    if (!mgr->initialized) return OZAYN_AUTH_ERR_DISABLED;

    /* If security is disabled, allow everything */
    if (!mgr->enabled) return OZAYN_AUTH_OK;

    /* Trust-all mode (development only) */
    if (mgr->auth_mode == OZAYN_AUTH_TRUST) {
        if (mgr->audit_logging) {
            LOG_INFO("SECURITY", "Auth (trust-all): '%s' -> ALLOWED", claimed_id);
        }
        return OZAYN_AUTH_OK;
    }

    /* Find the identity */
    const ozayn_identity_record_t *rec = NULL;
    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, claimed_id) == 0) {
            rec = &mgr->identities[i];
            break;
        }
    }

    if (!rec) {
        if (mgr->audit_logging) {
            LOG_WARN("SECURITY", "Auth FAILED: identity '%s' not found", claimed_id);
        }
        if (mgr->events) {
            ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                 OZAYN_EVENT_AUTH_FAILURE,
                                 OZAYN_SRC_SECURITY, (void *)claimed_id);
        }
        return OZAYN_AUTH_ERR_NOT_FOUND;
    }

    /* Check revoked state */
    if (rec->trust_state == OZAYN_TRUST_REVOKED) {
        if (mgr->audit_logging) {
            LOG_WARN("SECURITY", "Auth FAILED: identity '%s' is REVOKED", claimed_id);
        }
        if (mgr->events) {
            ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                 OZAYN_EVENT_AUTH_FAILURE,
                                 OZAYN_SRC_SECURITY, (void *)claimed_id);
        }
        return OZAYN_AUTH_ERR_REVOKED;
    }

    /* Check denied state */
    if (rec->trust_state == OZAYN_TRUST_DENIED) {
        if (mgr->audit_logging) {
            LOG_WARN("SECURITY", "Auth FAILED: identity '%s' is DENIED", claimed_id);
        }
        return OZAYN_AUTH_ERR_DENIED;
    }

    /* UID-based authentication */
    if (mgr->auth_mode == OZAYN_AUTH_UID && rec->auth_method == OZAYN_AUTH_UID) {
        if (!creds || !creds->valid) {
            if (mgr->audit_logging) {
                LOG_WARN("SECURITY", "Auth FAILED: no peer credentials for '%s'", claimed_id);
            }
            return OZAYN_AUTH_ERR_CREDENTIAL;
        }

        /* Core identity bypasses UID check (it's the server itself) */
        if (rec->type != OZAYN_IDENTITY_CORE) {
            if (rec->auth_uid != 0 && creds->uid != rec->auth_uid) {
                if (mgr->audit_logging) {
                    LOG_WARN("SECURITY", "Auth FAILED: UID mismatch for '%s' (expected %u, got %u)",
                             claimed_id, rec->auth_uid, creds->uid);
                }
                return OZAYN_AUTH_ERR_CREDENTIAL;
            }
        }

        /* Check allowed UIDs list */
        if (mgr->allowed_uid_count > 0 && rec->type != OZAYN_IDENTITY_CORE) {
            int allowed = 0;
            for (int i = 0; i < mgr->allowed_uid_count; i++) {
                if (mgr->allowed_uids[i] == creds->uid) {
                    allowed = 1;
                    break;
                }
            }
            if (!allowed) {
                if (mgr->audit_logging) {
                    LOG_WARN("SECURITY", "Auth FAILED: UID %u not in allowed list for '%s'",
                             creds->uid, claimed_id);
                }
                return OZAYN_AUTH_ERR_POLICY;
            }
        }
    }

    /* Authentication passed */
    if (mgr->audit_logging) {
        LOG_INFO("SECURITY", "Auth SUCCESS: '%s' (uid=%u, gid=%u)",
                 claimed_id,
                 creds ? creds->uid : 0,
                 creds ? creds->gid : 0);
    }

    /* Update last auth time */
    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, claimed_id) == 0) {
            mgr->identities[i].last_auth_at = time(NULL);
            mgr->identities[i].auth_verified = 1;
            break;
        }
    }

    /* Publish event */
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             OZAYN_EVENT_AUTH_SUCCESS,
                             OZAYN_SRC_SECURITY, (void *)claimed_id);
    }

    return OZAYN_AUTH_OK;
}

/* ================================================================
 * TRUST QUERIES
 * ================================================================ */

int ozayn_security_is_trusted(const ozayn_security_manager_t *mgr, const char *id) {
    if (!mgr || !id || !mgr->enabled) return mgr ? mgr->enabled : 0;

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {
            return mgr->identities[i].trust_state == OZAYN_TRUST_TRUSTED;
        }
    }

    return 0; /* unknown = not trusted (deny by default) */
}

ozayn_trust_state_t ozayn_security_get_trust_state(const ozayn_security_manager_t *mgr,
                                                    const char *id) {
    if (!mgr || !id) return OZAYN_TRUST_UNREGISTERED;

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {
            return mgr->identities[i].trust_state;
        }
    }

    return OZAYN_TRUST_UNREGISTERED;
}

const ozayn_identity_record_t *ozayn_security_find_identity(
    const ozayn_security_manager_t *mgr, const char *id) {
    if (!mgr || !id) return NULL;

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        if (mgr->identities[i].active &&
            strcmp(mgr->identities[i].id, id) == 0) {
            return &mgr->identities[i];
        }
    }

    return NULL;
}

/* ================================================================
 * QUERY
 * ================================================================ */

int ozayn_security_identity_count(const ozayn_security_manager_t *mgr) {
    return mgr ? mgr->identity_count : 0;
}

int ozayn_security_is_enabled(const ozayn_security_manager_t *mgr) {
    return mgr ? mgr->enabled : 0;
}
