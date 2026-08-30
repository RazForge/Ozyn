#ifndef OZAYN_SECURITY_H
#define OZAYN_SECURITY_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * security.h — Security & Identity Foundation (Stage 14).
 *
 * Establishes component identity, authentication, and trust.
 * Every component must prove its identity before accessing
 * privileged Core subsystems.
 *
 * Concepts:
 *   Identity    — Who are you? (unique string)
 *   Authentication — Can you prove it? (peer credentials)
 *   Trust       — Should OZAYN trust you? (state machine)
 *
 * Authentication uses Unix socket peer credentials:
 *   Linux:  SO_PEERCRED (kernel-verified UID/GID/PID)
 *   macOS:  getpeereid() (kernel-verified UID/GID)
 *
 * These cannot be forged by the connecting process.
 */

/* ---- Identity types ---- */
typedef enum {
    OZAYN_IDENTITY_CORE     = 0,  /* OZAYN Core itself */
    OZAYN_IDENTITY_MODULE   = 1,  /* Built-in module */
    OZAYN_IDENTITY_PLUGIN   = 2,  /* Loaded plugin (.so) */
    OZAYN_IDENTITY_SERVICE  = 3,  /* External service provider */
    OZAYN_IDENTITY_EXTERNAL = 4,  /* Unknown external process */
    OZAYN_IDENTITY_USER     = 5,  /* Human user */
} ozayn_identity_type_t;

/* ---- Trust state ---- */
typedef enum {
    OZAYN_TRUST_UNREGISTERED = 0,  /* Identity not registered */
    OZAYN_TRUST_PENDING      = 1,  /* Awaiting verification */
    OZAYN_TRUST_TRUSTED      = 2,  /* Fully authenticated */
    OZAYN_TRUST_REVOKED      = 3,  /* Trust revoked */
    OZAYN_TRUST_DENIED       = 4,  /* Explicitly denied */
} ozayn_trust_state_t;

/* ---- Authentication method ---- */
typedef enum {
    OZAYN_AUTH_NONE      = 0,  /* No authentication */
    OZAYN_AUTH_TRUST     = 1,  /* Trust-all (development only) */
    OZAYN_AUTH_UID       = 2,  /* Unix UID verification */
    OZAYN_AUTH_CREDENTIAL = 3, /* Token/credential based (future) */
} ozayn_auth_method_t;

/* ---- Authentication result ---- */
typedef enum {
    OZAYN_AUTH_OK              = 0,  /* Authentication succeeded */
    OZAYN_AUTH_ERR_NOT_FOUND   = 1,  /* Identity not registered */
    OZAYN_AUTH_ERR_REVOKED     = 2,  /* Identity was revoked */
    OZAYN_AUTH_ERR_CREDENTIAL  = 3,  /* Credential mismatch */
    OZAYN_AUTH_ERR_DENIED      = 4,  /* Explicitly denied */
    OZAYN_AUTH_ERR_DISABLED    = 5,  /* Security disabled */
    OZAYN_AUTH_ERR_POLICY      = 6,  /* Policy violation */
} ozayn_auth_result_t;

/* ---- Identity record ---- */
#define OZAYN_SECURITY_MAX_IDENTITIES  32
#define OZAYN_SECURITY_MAX_ID_LEN      64
#define OZAYN_SECURITY_MAX_NAME_LEN    128

typedef struct {
    int                      active;
    char                     id[OZAYN_SECURITY_MAX_ID_LEN];     /* unique identity string */
    char                     name[OZAYN_SECURITY_MAX_NAME_LEN]; /* human-readable name */
    ozayn_identity_type_t    type;
    ozayn_trust_state_t      trust_state;
    ozayn_auth_method_t      auth_method;
    uint32_t                 auth_uid;        /* expected Unix UID (for UID auth) */
    uint32_t                 auth_gid;        /* expected Unix GID */
    int                      auth_verified;   /* 1 = credentials verified */
    time_t                   created_at;
    time_t                   last_auth_at;
    time_t                   revoked_at;
    int                      auth_fail_count;
    char                     role_id[64];       /* assigned authorization role */
} ozayn_identity_record_t;

/* ---- Peer credentials (extracted from Unix socket) ---- */
typedef struct {
    uint32_t uid;     /* effective user ID */
    uint32_t gid;     /* effective group ID */
    uint32_t pid;     /* process ID */
    int      valid;   /* 1 = kernel-verified, 0 = not available */
} ozayn_peer_creds_t;

/* ---- Security manager ---- */
typedef struct {
    int                       enabled;
    ozayn_auth_method_t       auth_mode;
    ozayn_identity_record_t   identities[OZAYN_SECURITY_MAX_IDENTITIES];
    int                       identity_count;
    uint32_t                  allowed_uids[16];  /* UIDs allowed to connect */
    int                       allowed_uid_count;
    int                       audit_logging;     /* 1 = log security events */
    int                       initialized;
    void                     *events;            /* event engine pointer */
    void                     *recovery;          /* recovery pointer */
} ozayn_security_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_security_init(ozayn_security_manager_t *mgr, int enabled);
void           ozayn_security_shutdown(ozayn_security_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_security_set_events(ozayn_security_manager_t *mgr, void *events);
void ozayn_security_set_recovery(ozayn_security_manager_t *mgr, void *recovery);

/* ---- Configuration ---- */
void ozayn_security_set_auth_mode(ozayn_security_manager_t *mgr, ozayn_auth_method_t mode);
void ozayn_security_set_allowed_uid(ozayn_security_manager_t *mgr, uint32_t uid);
void ozayn_security_set_audit_logging(ozayn_security_manager_t *mgr, int enabled);

/* ---- Identity management ---- */
ozayn_result_t ozayn_security_register_identity(ozayn_security_manager_t *mgr,
                                                 const char *id,
                                                 const char *name,
                                                 ozayn_identity_type_t type,
                                                 ozayn_auth_method_t auth_method,
                                                 uint32_t auth_uid,
                                                 uint32_t auth_gid);
ozayn_result_t ozayn_security_revoke_identity(ozayn_security_manager_t *mgr, const char *id);
ozayn_result_t ozayn_security_remove_identity(ozayn_security_manager_t *mgr, const char *id);

/* ---- Authentication ---- */
ozayn_auth_result_t ozayn_security_authenticate(ozayn_security_manager_t *mgr,
                                                 const char *claimed_id,
                                                 const ozayn_peer_creds_t *creds);

/* ---- Trust queries ---- */
int                     ozayn_security_is_trusted(const ozayn_security_manager_t *mgr, const char *id);
ozayn_trust_state_t     ozayn_security_get_trust_state(const ozayn_security_manager_t *mgr, const char *id);
const ozayn_identity_record_t *ozayn_security_find_identity(const ozayn_security_manager_t *mgr,
                                                            const char *id);

/* ---- Utility ---- */
const char *ozayn_identity_type_name(ozayn_identity_type_t type);
const char *ozayn_trust_state_name(ozayn_trust_state_t state);
const char *ozayn_auth_method_name(ozayn_auth_method_t method);
const char *ozayn_auth_result_name(ozayn_auth_result_t result);

/* ---- Query ---- */
int ozayn_security_identity_count(const ozayn_security_manager_t *mgr);
int ozayn_security_is_enabled(const ozayn_security_manager_t *mgr);

#endif
