#ifndef OZAYN_AUTHORIZATION_H
#define OZAYN_AUTHORIZATION_H

#include "ozayn.h"
#include <stdint.h>

/*
 * authorization.h — Permission & Authorization Engine (Stage 15).
 *
 * Answers: "What are you allowed to do?"
 *
 * Concepts:
 *   Permission — action.resource (e.g. camera.read)
 *   Role       — named collection of permissions
 *   Policy     — identity → role(s) → permission(s)
 *   Decision   — ALLOW / DENY
 *
 * Deny by default. Least privilege. Centralized enforcement.
 */

/* ---- Constants ---- */
#define OZAYN_AUTHZ_MAX_PERMISSIONS    64
#define OZAYN_AUTHZ_MAX_ROLES          16
#define OZAYN_AUTHZ_MAX_ROLE_PERMS     32
#define OZAYN_AUTHZ_MAX_ROLE_ID_LEN    64
#define OZAYN_AUTHZ_MAX_PERM_LEN       64
#define OZAYN_AUTHZ_MAX_IDENTITY_ID    64
#define OZAYN_AUTHZ_MAX_IDENTITY_ROLES 4
#define OZAYN_AUTHZ_MAX_DESCRIPTION    128

/* ---- Permission ---- */
typedef struct {
    int      active;
    char     name[OZAYN_AUTHZ_MAX_PERM_LEN];   /* e.g. "camera.read" */
    char     description[OZAYN_AUTHZ_MAX_DESCRIPTION];
} ozayn_permission_t;

/* ---- Role ---- */
typedef struct {
    int      active;
    char     id[OZAYN_AUTHZ_MAX_ROLE_ID_LEN];  /* e.g. "VISION_SERVICE" */
    char     permissions[OZAYN_AUTHZ_MAX_ROLE_PERMS][OZAYN_AUTHZ_MAX_PERM_LEN];
    int      permission_count;
} ozayn_role_t;

/* ---- Identity-to-role binding ---- */
typedef struct {
    int      active;
    char     identity_id[OZAYN_AUTHZ_MAX_IDENTITY_ID];
    char     role_ids[OZAYN_AUTHZ_MAX_IDENTITY_ROLES][OZAYN_AUTHZ_MAX_ROLE_ID_LEN];
    int      role_count;
} ozayn_identity_role_t;

/* ---- Authorization decision ---- */
typedef enum {
    OZAYN_AUTHZ_ALLOW     = 0,
    OZAYN_AUTHZ_DENY      = 1,
    OZAYN_AUTHZ_ERROR     = 2,
} ozayn_authz_decision_t;

/* ---- Denial reason ---- */
typedef enum {
    OZAYN_DENY_REASON_NONE              = 0,
    OZAYN_DENY_REASON_UNAUTHENTICATED   = 1,
    OZAYN_DENY_REASON_NOT_TRUSTED       = 2,
    OZAYN_DENY_REASON_NO_ROLE           = 3,
    OZAYN_DENY_REASON_MISSING_PERMISSION = 4,
    OZAYN_DENY_REASON_UNKNOWN_PERMISSION = 5,
    OZAYN_DENY_REASON_REVOKED           = 6,
    OZAYN_DENY_REASON_POLICY            = 7,
} ozayn_deny_reason_t;

/* ---- Authorization request ---- */
typedef struct {
    const char *identity_id;
    const char *action;
    const char *resource;
} ozayn_authz_request_t;

/* ---- Authorization result ---- */
typedef struct {
    ozayn_authz_decision_t decision;
    ozayn_deny_reason_t    reason;
} ozayn_authz_result_t;

/* ---- Authorization manager ---- */
typedef struct {
    int                       enabled;
    int                       audit_logging;
    int                       initialized;
    /* Permission catalog */
    ozayn_permission_t        permissions[OZAYN_AUTHZ_MAX_PERMISSIONS];
    int                       permission_count;
    /* Roles */
    ozayn_role_t              roles[OZAYN_AUTHZ_MAX_ROLES];
    int                       role_count;
    /* Identity-to-role bindings */
    ozayn_identity_role_t     identity_roles[OZAYN_AUTHZ_MAX_PERMISSIONS];
    int                       identity_role_count;
    /* Bindings */
    void                     *security;    /* security manager pointer */
    void                     *events;      /* event engine pointer */
    void                     *recovery;    /* recovery pointer */
} ozayn_authorization_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_authorization_init(ozayn_authorization_manager_t *mgr, int enabled);
void           ozayn_authorization_shutdown(ozayn_authorization_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_authorization_set_security(ozayn_authorization_manager_t *mgr, void *security);
void ozayn_authorization_set_events(ozayn_authorization_manager_t *mgr, void *events);
void ozayn_authorization_set_recovery(ozayn_authorization_manager_t *mgr, void *recovery);

/* ---- Configuration ---- */
void ozayn_authorization_set_enabled(ozayn_authorization_manager_t *mgr, int enabled);
void ozayn_authorization_set_audit_logging(ozayn_authorization_manager_t *mgr, int enabled);

/* ---- Permission catalog ---- */
ozayn_result_t ozayn_authorization_register_permission(ozayn_authorization_manager_t *mgr,
                                                       const char *name,
                                                       const char *description);
int            ozayn_authorization_permission_exists(const ozayn_authorization_manager_t *mgr,
                                                     const char *name);
const ozayn_permission_t *ozayn_authorization_find_permission(const ozayn_authorization_manager_t *mgr,
                                                              const char *name);
int            ozayn_authorization_permission_count(const ozayn_authorization_manager_t *mgr);

/* ---- Role management ---- */
ozayn_result_t ozayn_authorization_create_role(ozayn_authorization_manager_t *mgr,
                                               const char *role_id);
ozayn_result_t ozayn_authorization_destroy_role(ozayn_authorization_manager_t *mgr,
                                                const char *role_id);
ozayn_result_t ozayn_authorization_role_add_permission(ozayn_authorization_manager_t *mgr,
                                                       const char *role_id,
                                                       const char *permission_name);
ozayn_result_t ozayn_authorization_role_remove_permission(ozayn_authorization_manager_t *mgr,
                                                          const char *role_id,
                                                          const char *permission_name);
const ozayn_role_t *ozayn_authorization_find_role(const ozayn_authorization_manager_t *mgr,
                                                  const char *role_id);
int            ozayn_authorization_role_count(const ozayn_authorization_manager_t *mgr);
int            ozayn_authorization_role_has_permission(const ozayn_authorization_manager_t *mgr,
                                                       const char *role_id,
                                                       const char *permission_name);

/* ---- Identity-to-role assignment ---- */
ozayn_result_t ozayn_authorization_assign_role(ozayn_authorization_manager_t *mgr,
                                               const char *identity_id,
                                               const char *role_id);
ozayn_result_t ozayn_authorization_revoke_role(ozayn_authorization_manager_t *mgr,
                                               const char *identity_id,
                                               const char *role_id);
int            ozayn_authorization_identity_has_role(const ozayn_authorization_manager_t *mgr,
                                                     const char *identity_id,
                                                     const char *role_id);
int            ozayn_authorization_identity_role_count(const ozayn_authorization_manager_t *mgr,
                                                       const char *identity_id);

/* ---- Central authorization check ---- */
ozayn_authz_result_t ozayn_authorize(const ozayn_authorization_manager_t *mgr,
                                     const char *identity_id,
                                     const char *action,
                                     const char *resource);

/* ---- Query ---- */
int  ozayn_authorization_is_enabled(const ozayn_authorization_manager_t *mgr);
int  ozayn_authorization_is_initialized(const ozayn_authorization_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_authz_decision_name(ozayn_authz_decision_t decision);
const char *ozayn_deny_reason_name(ozayn_deny_reason_t reason);

#endif
