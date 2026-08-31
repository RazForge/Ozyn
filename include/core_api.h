#ifndef OZAYN_CORE_API_H
#define OZAYN_CORE_API_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * core_api.h — Core API & Internal Communication (Stage 26).
 *
 * Self-contained header — no circular includes.
 * Defines the official internal interfaces for cross-component communication.
 *
 * Responsibilities:
 *   - Service contracts: versioned API interfaces for each component
 *   - Request/response structures for cross-component calls
 *   - Error codes and structured error propagation
 *   - API version checking (compatible, compatible-with-revision, incompatible)
 *   - Stability levels (stable, experimental, deprecated, internal)
 *   - Message bus for typed cross-component messages
 *   - API registry: track which component provides which interface
 *   - Request ID tracking for correlated request/response flows
 */

/* ---- Constants ---- */

#define OZAYN_API_MAX_INTERFACES    64
#define OZAYN_API_MAX_NAME          64
#define OZAYN_API_MAX_DESC          128
#define OZAYN_API_MAX_REQUESTS      128   /* pending request queue */
#define OZAYN_API_MAX_RESPONSE_SIZE 1024

/* ---- API version ---- */

typedef struct {
    uint16_t major;   /* breaking changes */
    uint16_t minor;   /* new features, backward-compatible */
    uint16_t patch;   /* bug fixes, backward-compatible */
    uint32_t revision;/* build/commit revision */
} ozayn_api_version_t;

/* ---- Stability level ---- */

typedef enum {
    OZAYN_API_STABLE       = 0,   /* guaranteed backward-compatible */
    OZAYN_API_EXPERIMENTAL = 1,   /* may change without notice */
    OZAYN_API_DEPRECATED   = 2,   /* will be removed */
    OZAYN_API_INTERNAL     = 3,   /* not for external use */
} ozayn_api_stability_t;

/* ---- API compatibility result ---- */

typedef enum {
    OZAYN_API_COMPAT_OK            = 0,  /* fully compatible */
    OZAYN_API_COMPAT_REVISION_DIFF = 1,  /* compatible, revision differs */
    OZAYN_API_COMPAT_MINOR_BEHIND  = 2,  /* caller is behind (may miss features) */
    OZAYN_API_COMPAT_INCOMPATIBLE  = 3,  /* major version mismatch */
} ozayn_api_compat_t;

/* ---- Error codes ---- */

typedef enum {
    OZAYN_API_ERR_OK               = 0,
    OZAYN_API_ERR_NULL             = -100,
    OZAYN_API_ERR_INVALID          = -101,
    OZAYN_API_ERR_NOT_FOUND        = -102,
    OZAYN_API_ERR_EXISTS           = -103,
    OZAYN_API_ERR_BUSY             = -104,
    OZAYN_API_ERR_TIMEOUT          = -105,
    OZAYN_API_ERR_NO_MEMORY        = -106,
    OZAYN_API_ERR_STATE            = -107,
    OZAYN_API_ERR_PERMISSION       = -108,
    OZAYN_API_ERR_DEPENDENCY       = -109,
    OZAYN_API_ERR_VERSION_MISMATCH = -110,
    OZAYN_API_ERR_NOT_IMPLEMENTED  = -111,
    OZAYN_API_ERR_INTERNAL         = -112,
    OZAYN_API_ERR_FULL             = -113,
    OZAYN_API_ERR_EMPTY            = -114,
} ozayn_api_error_t;

/* ---- Request method ---- */

typedef enum {
    OZAYN_METHOD_NONE     = 0,
    OZAYN_METHOD_GET      = 1,
    OZAYN_METHOD_SET      = 2,
    OZAYN_METHOD_CALL     = 3,
    OZAYN_METHOD_QUERY    = 4,
    OZAYN_METHOD_NOTIFY   = 5,
    OZAYN_METHOD_CREATE   = 6,
    OZAYN_METHOD_DESTROY  = 7,
    OZAYN_METHOD_START    = 8,
    OZAYN_METHOD_STOP     = 9,
    OZAYN_METHOD_RESTART  = 10,
    OZAYN_METHOD_STATUS   = 11,
    OZAYN_METHOD_CUSTOM   = 32,
} ozayn_api_method_t;

/* ---- Request status ---- */

typedef enum {
    OZAYN_REQ_PENDING    = 0,
    OZAYN_REQ_PROCESSING = 1,
    OZAYN_REQ_COMPLETED  = 2,
    OZAYN_REQ_FAILED     = 3,
    OZAYN_REQ_CANCELLED  = 4,
    OZAYN_REQ_TIMEOUT    = 5,
} ozayn_api_req_status_t;

/* ---- Request header ---- */

typedef struct {
    uint32_t                id;
    ozayn_api_method_t      method;
    char                    source[OZAYN_API_MAX_NAME];
    char                    target[OZAYN_API_MAX_NAME];
    char                    operation[OZAYN_API_MAX_NAME];
    uint32_t                api_version_major;
    time_t                  created_at;
    time_t                  timeout_ms;
} ozayn_api_request_t;

/* ---- Response header ---- */

typedef struct {
    uint32_t                request_id;
    ozayn_api_error_t       error;
    char                    error_msg[OZAYN_API_MAX_DESC];
    time_t                  completed_at;
} ozayn_api_response_t;

/* ---- Request/response pair ---- */

typedef struct {
    int                     active;
    ozayn_api_request_t     request;
    ozayn_api_response_t    response;
    int                     has_response;
    void                   *payload;
    uint32_t                payload_size;
} ozayn_api_transaction_t;

/* ---- Interface definition ---- */

typedef struct {
    int                      active;
    char                     name[OZAYN_API_MAX_NAME];
    char                     description[OZAYN_API_MAX_DESC];
    ozayn_api_version_t      version;
    ozayn_api_stability_t    stability;
    char                     provider[OZAYN_API_MAX_NAME];
    uint32_t                 method_count;
    ozayn_api_method_t       methods[32];
    uint32_t                 total_requests;
    uint32_t                 total_errors;
    time_t                   registered_at;
} ozayn_api_interface_t;

/* ---- API stats ---- */

typedef struct {
    uint32_t total_interfaces;
    uint32_t total_requests;
    uint32_t total_responses;
    uint32_t total_errors;
    uint32_t pending_requests;
    uint32_t completed_requests;
} ozayn_api_stats_t;

/* ---- Forward declarations for events ---- */
typedef struct ozayn_event_engine_s ozayn_event_engine_t;

/* ---- Manager ---- */

typedef struct {
    ozayn_api_interface_t   interfaces[OZAYN_API_MAX_INTERFACES];
    ozayn_api_transaction_t requests[OZAYN_API_MAX_REQUESTS];
    uint32_t                next_request_id;
    uint32_t                total_requests;
    uint32_t                total_responses;
    uint32_t                total_errors;
    ozayn_event_engine_t   *events;
} ozayn_api_manager_t;

/* ================================================================
 * API
 * ================================================================ */

/* Lifecycle */
int  ozayn_api_init(ozayn_api_manager_t *mgr);
void ozayn_api_shutdown(ozayn_api_manager_t *mgr);
void ozayn_api_set_events(ozayn_api_manager_t *mgr, ozayn_event_engine_t *events);

/* Interface registration */
int  ozayn_api_register(ozayn_api_manager_t *mgr, const char *name,
                         const char *provider,
                         const ozayn_api_version_t *version,
                         ozayn_api_stability_t stability,
                         const char *description);
int  ozayn_api_unregister(ozayn_api_manager_t *mgr, const char *name);

/* Interface method registration */
int  ozayn_api_add_method(ozayn_api_manager_t *mgr, const char *interface_name,
                           ozayn_api_method_t method);

/* Version checking */
ozayn_api_compat_t ozayn_api_check_compat(const ozayn_api_version_t *provided,
                                           const ozayn_api_version_t *required);
int  ozayn_api_check_provider(ozayn_api_manager_t *mgr, const char *interface_name,
                               uint16_t required_major);

/* Request/response flow */
uint32_t ozayn_api_request_begin(ozayn_api_manager_t *mgr, const char *source,
                                  const char *target, const char *operation,
                                  ozayn_api_method_t method);
int  ozayn_api_request_complete(ozayn_api_manager_t *mgr, uint32_t request_id,
                                 ozayn_api_error_t error, const char *error_msg);
int  ozayn_api_request_cancel(ozayn_api_manager_t *mgr, uint32_t request_id);

/* Query */
const ozayn_api_interface_t *ozayn_api_find_interface(ozayn_api_manager_t *mgr,
                                                       const char *name);
ozayn_api_error_t ozayn_api_last_error(ozayn_api_manager_t *mgr);
int  ozayn_api_has_interface(ozayn_api_manager_t *mgr, const char *name,
                              uint16_t required_major);

/* Stats */
ozayn_api_stats_t ozayn_api_stats(ozayn_api_manager_t *mgr);

/* Print / debug */
void ozayn_api_print_interfaces(ozayn_api_manager_t *mgr);
void ozayn_api_print_interface(ozayn_api_manager_t *mgr, const char *name);
void ozayn_api_print_pending(ozayn_api_manager_t *mgr);

/* Names */
const char *ozayn_api_error_name(ozayn_api_error_t error);
const char *ozayn_api_method_name(ozayn_api_method_t method);
const char *ozayn_api_stability_name(ozayn_api_stability_t stability);
const char *ozayn_api_compat_name(ozayn_api_compat_t compat);
const char *ozayn_api_req_status_name(ozayn_api_req_status_t status);

#endif /* OZAYN_CORE_API_H */
