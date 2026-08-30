#include "authorization.h"
#include "security.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * NAME FUNCTIONS
 * ================================================================ */

const char *ozayn_authz_decision_name(ozayn_authz_decision_t decision) {
    switch (decision) {
        case OZAYN_AUTHZ_ALLOW: return "ALLOW";
        case OZAYN_AUTHZ_DENY:  return "DENY";
        case OZAYN_AUTHZ_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

const char *ozayn_deny_reason_name(ozayn_deny_reason_t reason) {
    switch (reason) {
        case OZAYN_DENY_REASON_NONE:               return "none";
        case OZAYN_DENY_REASON_UNAUTHENTICATED:    return "UNAUTHENTICATED";
        case OZAYN_DENY_REASON_NOT_TRUSTED:        return "NOT_TRUSTED";
        case OZAYN_DENY_REASON_NO_ROLE:            return "NO_ROLE";
        case OZAYN_DENY_REASON_MISSING_PERMISSION: return "MISSING_PERMISSION";
        case OZAYN_DENY_REASON_UNKNOWN_PERMISSION: return "UNKNOWN_PERMISSION";
        case OZAYN_DENY_REASON_REVOKED:            return "REVOKED";
        case OZAYN_DENY_REASON_POLICY:             return "POLICY";
    }
    return "UNKNOWN";
}

/* ================================================================
 * BUILT-IN PERMISSIONS
 * ================================================================ */

static const struct { const char *name; const char *desc; } builtin_permissions[] = {
    { "camera.read",                   "Read camera data" },
    { "camera.control",                "Control camera hardware" },
    { "microphone.read",               "Read microphone data" },
    { "speech.process",                "Process speech input" },
    { "vision.detect",                 "Detect objects in vision" },
    { "gesture.detect",                "Detect gestures" },
    { "process.start",                 "Start processes" },
    { "process.stop",                  "Stop/terminate processes" },
    { "module.load",                   "Load modules" },
    { "module.unload",                 "Unload modules" },
    { "plugin.load",                   "Load plugins" },
    { "plugin.unload",                 "Unload plugins" },
    { "service.register",              "Register services" },
    { "service.lookup",                "Look up services" },
    { "service.unregister",            "Unregister services" },
    { "config.read",                   "Read configuration" },
    { "config.write",                  "Write configuration" },
    { "security.read",                 "Read security state" },
    { "security.policy.modify",        "Modify security policy" },
    { "core.shutdown",                 "Shut down OZAYN Core" },
    { "event.publish",                 "Publish events" },
    { "event.subscribe",               "Subscribe to events" },
    { "task.create",                   "Create tasks" },
    { "ipc.request",                   "Send IPC requests" },
};
static const int builtin_permission_count =
    (int)(sizeof(builtin_permissions) / sizeof(builtin_permissions[0]));

/* ================================================================
 * BUILT-IN ROLES
 * ================================================================ */

static void load_builtin_roles(ozayn_authorization_manager_t *mgr) {
    /* CORE_ADMIN — full access */
    ozayn_authorization_create_role(mgr, "CORE_ADMIN");
    for (int i = 0; i < builtin_permission_count; i++) {
        ozayn_authorization_role_add_permission(mgr, "CORE_ADMIN", builtin_permissions[i].name);
    }

    /* VISION_SERVICE */
    ozayn_authorization_create_role(mgr, "VISION_SERVICE");
    ozayn_authorization_role_add_permission(mgr, "VISION_SERVICE", "camera.read");
    ozayn_authorization_role_add_permission(mgr, "VISION_SERVICE", "camera.control");
    ozayn_authorization_role_add_permission(mgr, "VISION_SERVICE", "vision.detect");
    ozayn_authorization_role_add_permission(mgr, "VISION_SERVICE", "gesture.detect");

    /* VOICE_SERVICE */
    ozayn_authorization_create_role(mgr, "VOICE_SERVICE");
    ozayn_authorization_role_add_permission(mgr, "VOICE_SERVICE", "microphone.read");
    ozayn_authorization_role_add_permission(mgr, "VOICE_SERVICE", "speech.process");

    /* SERVICE_BASE — minimal for any registered service */
    ozayn_authorization_create_role(mgr, "SERVICE_BASE");
    ozayn_authorization_role_add_permission(mgr, "SERVICE_BASE", "service.lookup");
    ozayn_authorization_role_add_permission(mgr, "SERVICE_BASE", "config.read");
    ozayn_authorization_role_add_permission(mgr, "SERVICE_BASE", "event.subscribe");
    ozayn_authorization_role_add_permission(mgr, "SERVICE_BASE", "event.publish");
    ozayn_authorization_role_add_permission(mgr, "SERVICE_BASE", "ipc.request");
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

ozayn_result_t ozayn_authorization_init(ozayn_authorization_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_authorization_manager_t));
    mgr->enabled = enabled;
    mgr->audit_logging = 1;
    mgr->initialized = 1;

    /* Register built-in permissions */
    for (int i = 0; i < builtin_permission_count; i++) {
        ozayn_authorization_register_permission(mgr,
                                                builtin_permissions[i].name,
                                                builtin_permissions[i].desc);
    }

    /* Load built-in roles */
    load_builtin_roles(mgr);

    /* Assign CORE_ADMIN to ozayn.core by default */
    ozayn_authorization_assign_role(mgr, "ozayn.core", "CORE_ADMIN");

    LOG_INFO("AUTHORIZATION", "Authorization manager initialized (enabled=%s, permissions=%d, roles=%d)",
             enabled ? "yes" : "no",
             mgr->permission_count,
             mgr->role_count);

    return OZAYN_OK;
}

void ozayn_authorization_shutdown(ozayn_authorization_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Clear all bindings */
    mgr->identity_role_count = 0;
    mgr->role_count = 0;
    mgr->permission_count = 0;
    mgr->initialized = 0;

    LOG_INFO("AUTHORIZATION", "Authorization manager shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_authorization_set_security(ozayn_authorization_manager_t *mgr, void *security) {
    if (mgr) mgr->security = security;
}

void ozayn_authorization_set_events(ozayn_authorization_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_authorization_set_recovery(ozayn_authorization_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ================================================================
 * CONFIGURATION
 * ================================================================ */

void ozayn_authorization_set_enabled(ozayn_authorization_manager_t *mgr, int enabled) {
    if (mgr) mgr->enabled = enabled;
}

void ozayn_authorization_set_audit_logging(ozayn_authorization_manager_t *mgr, int enabled) {
    if (mgr) mgr->audit_logging = enabled;
}

/* ================================================================
 * PERMISSION CATALOG
 * ================================================================ */

ozayn_result_t ozayn_authorization_register_permission(ozayn_authorization_manager_t *mgr,
                                                       const char *name,
                                                       const char *description) {
    if (!mgr || !name) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Check duplicate */
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->permissions[i].active &&
            strcmp(mgr->permissions[i].name, name) == 0) {
            return OZAYN_ERR_STATE; /* already registered */
        }
    }

    /* Find free slot */
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (!mgr->permissions[i].active) {
            mgr->permissions[i].active = 1;
            strncpy(mgr->permissions[i].name, name, OZAYN_AUTHZ_MAX_PERM_LEN - 1);
            if (description) {
                strncpy(mgr->permissions[i].description, description,
                        OZAYN_AUTHZ_MAX_DESCRIPTION - 1);
            }
            mgr->permission_count++;
            return OZAYN_OK;
        }
    }

    LOG_ERROR("AUTHORIZATION", "Permission catalog full — cannot register '%s'", name);
    return OZAYN_ERR;
}

int ozayn_authorization_permission_exists(const ozayn_authorization_manager_t *mgr,
                                          const char *name) {
    if (!mgr || !name) return 0;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->permissions[i].active &&
            strcmp(mgr->permissions[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

const ozayn_permission_t *ozayn_authorization_find_permission(
    const ozayn_authorization_manager_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->permissions[i].active &&
            strcmp(mgr->permissions[i].name, name) == 0) {
            return &mgr->permissions[i];
        }
    }
    return NULL;
}

int ozayn_authorization_permission_count(const ozayn_authorization_manager_t *mgr) {
    return mgr ? mgr->permission_count : 0;
}

/* ================================================================
 * ROLE MANAGEMENT
 * ================================================================ */

ozayn_result_t ozayn_authorization_create_role(ozayn_authorization_manager_t *mgr,
                                               const char *role_id) {
    if (!mgr || !role_id) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Check duplicate */
    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (mgr->roles[i].active &&
            strcmp(mgr->roles[i].id, role_id) == 0) {
            return OZAYN_ERR_STATE;
        }
    }

    /* Find free slot */
    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (!mgr->roles[i].active) {
            mgr->roles[i].active = 1;
            strncpy(mgr->roles[i].id, role_id, OZAYN_AUTHZ_MAX_ROLE_ID_LEN - 1);
            mgr->roles[i].permission_count = 0;
            mgr->role_count++;

            if (mgr->audit_logging) {
                LOG_INFO("AUTHORIZATION", "Role created: '%s'", role_id);
            }
            return OZAYN_OK;
        }
    }

    LOG_ERROR("AUTHORIZATION", "Role registry full — cannot create '%s'", role_id);
    return OZAYN_ERR;
}

ozayn_result_t ozayn_authorization_destroy_role(ozayn_authorization_manager_t *mgr,
                                                const char *role_id) {
    if (!mgr || !role_id) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (mgr->roles[i].active &&
            strcmp(mgr->roles[i].id, role_id) == 0) {
            /* Remove all identity bindings for this role */
            for (int j = 0; j < OZAYN_AUTHZ_MAX_PERMISSIONS; j++) {
                if (mgr->identity_roles[j].active) {
                    for (int k = 0; k < mgr->identity_roles[j].role_count; k++) {
                        if (strcmp(mgr->identity_roles[j].role_ids[k], role_id) == 0) {
                            /* Shift remaining roles down */
                            for (int l = k; l < mgr->identity_roles[j].role_count - 1; l++) {
                                strcpy(mgr->identity_roles[j].role_ids[l],
                                       mgr->identity_roles[j].role_ids[l + 1]);
                            }
                            mgr->identity_roles[j].role_count--;
                            k--;
                        }
                    }
                    /* If no roles left, deactivate binding */
                    if (mgr->identity_roles[j].role_count == 0) {
                        mgr->identity_roles[j].active = 0;
                        mgr->identity_role_count--;
                    }
                }
            }

            mgr->roles[i].active = 0;
            mgr->role_count--;

            if (mgr->audit_logging) {
                LOG_INFO("AUTHORIZATION", "Role destroyed: '%s'", role_id);
            }
            return OZAYN_OK;
        }
    }

    return OZAYN_ERR;
}

ozayn_result_t ozayn_authorization_role_add_permission(ozayn_authorization_manager_t *mgr,
                                                       const char *role_id,
                                                       const char *permission_name) {
    if (!mgr || !role_id || !permission_name) return OZAYN_ERR_NULL;

    /* Validate permission exists in catalog */
    if (!ozayn_authorization_permission_exists(mgr, permission_name)) {
        LOG_WARN("AUTHORIZATION", "Unknown permission '%s' — rejected for role '%s'",
                 permission_name, role_id);
        return OZAYN_ERR;
    }

    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (mgr->roles[i].active &&
            strcmp(mgr->roles[i].id, role_id) == 0) {

            /* Check duplicate */
            for (int j = 0; j < mgr->roles[i].permission_count; j++) {
                if (strcmp(mgr->roles[i].permissions[j], permission_name) == 0) {
                    return OZAYN_ERR_STATE; /* already has it */
                }
            }

            if (mgr->roles[i].permission_count >= OZAYN_AUTHZ_MAX_ROLE_PERMS) {
                LOG_ERROR("AUTHORIZATION", "Role '%s' permission list full", role_id);
                return OZAYN_ERR;
            }

            strncpy(mgr->roles[i].permissions[mgr->roles[i].permission_count],
                    permission_name, OZAYN_AUTHZ_MAX_PERM_LEN - 1);
            mgr->roles[i].permission_count++;

            if (mgr->audit_logging) {
                LOG_INFO("AUTHORIZATION", "Permission '%s' added to role '%s'",
                         permission_name, role_id);
            }
            return OZAYN_OK;
        }
    }

    return OZAYN_ERR; /* role not found */
}

ozayn_result_t ozayn_authorization_role_remove_permission(ozayn_authorization_manager_t *mgr,
                                                          const char *role_id,
                                                          const char *permission_name) {
    if (!mgr || !role_id || !permission_name) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (mgr->roles[i].active &&
            strcmp(mgr->roles[i].id, role_id) == 0) {

            for (int j = 0; j < mgr->roles[i].permission_count; j++) {
                if (strcmp(mgr->roles[i].permissions[j], permission_name) == 0) {
                    /* Shift remaining down */
                    for (int k = j; k < mgr->roles[i].permission_count - 1; k++) {
                        strcpy(mgr->roles[i].permissions[k],
                               mgr->roles[i].permissions[k + 1]);
                    }
                    mgr->roles[i].permission_count--;

                    if (mgr->audit_logging) {
                        LOG_INFO("AUTHORIZATION", "Permission '%s' removed from role '%s'",
                                 permission_name, role_id);
                    }
                    return OZAYN_OK;
                }
            }

            return OZAYN_ERR; /* permission not in role */
        }
    }

    return OZAYN_ERR; /* role not found */
}

const ozayn_role_t *ozayn_authorization_find_role(const ozayn_authorization_manager_t *mgr,
                                                  const char *role_id) {
    if (!mgr || !role_id) return NULL;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        if (mgr->roles[i].active &&
            strcmp(mgr->roles[i].id, role_id) == 0) {
            return &mgr->roles[i];
        }
    }
    return NULL;
}

int ozayn_authorization_role_count(const ozayn_authorization_manager_t *mgr) {
    return mgr ? mgr->role_count : 0;
}

int ozayn_authorization_role_has_permission(const ozayn_authorization_manager_t *mgr,
                                            const char *role_id,
                                            const char *permission_name) {
    if (!mgr || !role_id || !permission_name) return 0;
    const ozayn_role_t *role = ozayn_authorization_find_role(mgr, role_id);
    if (!role) return 0;
    for (int i = 0; i < role->permission_count; i++) {
        if (strcmp(role->permissions[i], permission_name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 * IDENTITY-TO-ROLE ASSIGNMENT
 * ================================================================ */

ozayn_result_t ozayn_authorization_assign_role(ozayn_authorization_manager_t *mgr,
                                               const char *identity_id,
                                               const char *role_id) {
    if (!mgr || !identity_id || !role_id) return OZAYN_ERR_NULL;
    if (!mgr->initialized) return OZAYN_ERR_STATE;

    /* Validate role exists */
    if (!ozayn_authorization_find_role(mgr, role_id)) {
        LOG_WARN("AUTHORIZATION", "Role '%s' does not exist — cannot assign to '%s'",
                 role_id, identity_id);
        return OZAYN_ERR;
    }

    /* Find or create binding for this identity */
    ozayn_identity_role_t *binding = NULL;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->identity_roles[i].active &&
            strcmp(mgr->identity_roles[i].identity_id, identity_id) == 0) {
            binding = &mgr->identity_roles[i];
            break;
        }
    }

    if (!binding) {
        /* Create new binding */
        for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
            if (!mgr->identity_roles[i].active) {
                binding = &mgr->identity_roles[i];
                memset(binding, 0, sizeof(ozayn_identity_role_t));
                binding->active = 1;
                strncpy(binding->identity_id, identity_id, OZAYN_AUTHZ_MAX_IDENTITY_ID - 1);
                mgr->identity_role_count++;
                break;
            }
        }
    }

    if (!binding) {
        LOG_ERROR("AUTHORIZATION", "Identity role bindings full");
        return OZAYN_ERR;
    }

    /* Check if already assigned */
    for (int i = 0; i < binding->role_count; i++) {
        if (strcmp(binding->role_ids[i], role_id) == 0) {
            return OZAYN_ERR_STATE; /* already assigned */
        }
    }

    if (binding->role_count >= OZAYN_AUTHZ_MAX_IDENTITY_ROLES) {
        LOG_ERROR("AUTHORIZATION", "Identity '%s' role list full", identity_id);
        return OZAYN_ERR;
    }

    strncpy(binding->role_ids[binding->role_count], role_id, OZAYN_AUTHZ_MAX_ROLE_ID_LEN - 1);
    binding->role_count++;

    if (mgr->audit_logging) {
        LOG_INFO("AUTHORIZATION", "Role '%s' assigned to identity '%s'", role_id, identity_id);
    }

    /* Publish event */
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             OZAYN_EVENT_ROLE_ASSIGNED,
                             OZAYN_SRC_SECURITY, (void *)identity_id);
    }

    return OZAYN_OK;
}

ozayn_result_t ozayn_authorization_revoke_role(ozayn_authorization_manager_t *mgr,
                                               const char *identity_id,
                                               const char *role_id) {
    if (!mgr || !identity_id || !role_id) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->identity_roles[i].active &&
            strcmp(mgr->identity_roles[i].identity_id, identity_id) == 0) {

            for (int j = 0; j < mgr->identity_roles[i].role_count; j++) {
                if (strcmp(mgr->identity_roles[i].role_ids[j], role_id) == 0) {
                    /* Shift remaining down */
                    for (int k = j; k < mgr->identity_roles[i].role_count - 1; k++) {
                        strcpy(mgr->identity_roles[i].role_ids[k],
                               mgr->identity_roles[i].role_ids[k + 1]);
                    }
                    mgr->identity_roles[i].role_count--;

                    if (mgr->identity_roles[i].role_count == 0) {
                        mgr->identity_roles[i].active = 0;
                        mgr->identity_role_count--;
                    }

                    if (mgr->audit_logging) {
                        LOG_INFO("AUTHORIZATION", "Role '%s' revoked from identity '%s'",
                                 role_id, identity_id);
                    }

                    /* Publish event */
                    if (mgr->events) {
                        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                             OZAYN_EVENT_ROLE_REVOKED,
                                             OZAYN_SRC_SECURITY, (void *)identity_id);
                    }

                    return OZAYN_OK;
                }
            }

            return OZAYN_ERR; /* role not assigned */
        }
    }

    return OZAYN_ERR; /* identity not found */
}

int ozayn_authorization_identity_has_role(const ozayn_authorization_manager_t *mgr,
                                          const char *identity_id,
                                          const char *role_id) {
    if (!mgr || !identity_id || !role_id) return 0;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->identity_roles[i].active &&
            strcmp(mgr->identity_roles[i].identity_id, identity_id) == 0) {
            for (int j = 0; j < mgr->identity_roles[i].role_count; j++) {
                if (strcmp(mgr->identity_roles[i].role_ids[j], role_id) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int ozayn_authorization_identity_role_count(const ozayn_authorization_manager_t *mgr,
                                            const char *identity_id) {
    if (!mgr || !identity_id) return 0;
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->identity_roles[i].active &&
            strcmp(mgr->identity_roles[i].identity_id, identity_id) == 0) {
            return mgr->identity_roles[i].role_count;
        }
    }
    return 0;
}

/* ================================================================
 * CENTRAL AUTHORIZATION CHECK
 * ================================================================ */

ozayn_authz_result_t ozayn_authorize(const ozayn_authorization_manager_t *mgr,
                                     const char *identity_id,
                                     const char *action,
                                     const char *resource) {
    ozayn_authz_result_t result;
    result.decision = OZAYN_AUTHZ_DENY;
    result.reason = OZAYN_DENY_REASON_NONE;

    if (!mgr || !identity_id || !action || !resource) {
        result.reason = OZAYN_DENY_REASON_UNAUTHENTICATED;
        return result;
    }

    if (!mgr->initialized) {
        result.reason = OZAYN_DENY_REASON_POLICY;
        return result;
    }

    /* If authorization is disabled, allow everything */
    if (!mgr->enabled) {
        result.decision = OZAYN_AUTHZ_ALLOW;
        return result;
    }

    /* Step 1: Verify identity is authenticated via security manager */
    if (mgr->security) {
        ozayn_security_manager_t *sec = (ozayn_security_manager_t *)mgr->security;
        if (!ozayn_security_is_trusted(sec, identity_id)) {
            result.reason = OZAYN_DENY_REASON_NOT_TRUSTED;
            if (mgr->audit_logging) {
                LOG_WARN("AUTHORIZATION", "DENY: identity '%s' not trusted (action=%s.%s)",
                         identity_id, action, resource);
            }
            return result;
        }
    }

    /* Step 2: Build permission name from action.resource */
    char perm_name[OZAYN_AUTHZ_MAX_PERM_LEN];
    snprintf(perm_name, sizeof(perm_name), "%s.%s", action, resource);

    /* Step 3: Validate permission exists in catalog */
    if (!ozayn_authorization_permission_exists(mgr, perm_name)) {
        result.reason = OZAYN_DENY_REASON_UNKNOWN_PERMISSION;
        if (mgr->audit_logging) {
            LOG_WARN("AUTHORIZATION", "DENY: unknown permission '%s' for identity '%s'",
                     perm_name, identity_id);
        }
        return result;
    }

    /* Step 4: Find identity's roles and check for matching permission */
    for (int i = 0; i < OZAYN_AUTHZ_MAX_PERMISSIONS; i++) {
        if (mgr->identity_roles[i].active &&
            strcmp(mgr->identity_roles[i].identity_id, identity_id) == 0) {

            for (int j = 0; j < mgr->identity_roles[i].role_count; j++) {
                const char *role_id = mgr->identity_roles[i].role_ids[j];
                if (ozayn_authorization_role_has_permission(mgr, role_id, perm_name)) {
                    /* Permission found — ALLOW */
                    result.decision = OZAYN_AUTHZ_ALLOW;
                    result.reason = OZAYN_DENY_REASON_NONE;

                    if (mgr->audit_logging) {
                        LOG_INFO("AUTHORIZATION", "ALLOW: '%s' -> %s (via role '%s')",
                                 identity_id, perm_name, role_id);
                    }

                    /* Publish event */
                    if (mgr->events) {
                        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                                             OZAYN_EVENT_AUTHORIZATION_ALLOWED,
                                             OZAYN_SRC_SECURITY, (void *)identity_id);
                    }

                    return result;
                }
            }
        }
    }

    /* Step 5: No matching permission found — DENY (deny by default) */
    result.reason = OZAYN_DENY_REASON_MISSING_PERMISSION;

    if (mgr->audit_logging) {
        LOG_WARN("AUTHORIZATION", "DENY: '%s' lacks '%s' (no role grants it)",
                 identity_id, perm_name);
    }

    /* Publish event */
    if (mgr->events) {
        ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                             OZAYN_EVENT_AUTHORIZATION_DENIED,
                             OZAYN_SRC_SECURITY, (void *)identity_id);
    }

    return result;
}

/* ================================================================
 * QUERY
 * ================================================================ */

int ozayn_authorization_is_enabled(const ozayn_authorization_manager_t *mgr) {
    return mgr ? mgr->enabled : 0;
}

int ozayn_authorization_is_initialized(const ozayn_authorization_manager_t *mgr) {
    return mgr ? mgr->initialized : 0;
}
