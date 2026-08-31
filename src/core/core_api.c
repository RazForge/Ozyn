#include "core_api.h"
#include "events.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * core_api.c — Core API & Internal Communication (Stage 26).
 *
 * Implements the formal interface layer for cross-component communication:
 *   - Interface registration and versioning
 *   - Request/response lifecycle tracking
 *   - API compatibility checking
 *   - Error propagation
 *   - Transaction tracking
 */

/* ================================================================
 * Helpers
 * ================================================================ */

static void publish_event(ozayn_api_manager_t *mgr, ozayn_event_type_t type) {
    if (!mgr || !mgr->events) return;
    ozayn_events_publish(mgr->events, type, OZAYN_SRC_CORE, NULL);
}

static ozayn_api_interface_t *find_interface(ozayn_api_manager_t *mgr, const char *name) {
    if (!name || !name[0]) return NULL;
    for (uint32_t i = 0; i < OZAYN_API_MAX_INTERFACES; i++) {
        if (mgr->interfaces[i].active &&
            strcmp(mgr->interfaces[i].name, name) == 0) {
            return &mgr->interfaces[i];
        }
    }
    return NULL;
}

static ozayn_api_transaction_t *find_transaction(ozayn_api_manager_t *mgr, uint32_t id) {
    for (uint32_t i = 0; i < OZAYN_API_MAX_REQUESTS; i++) {
        if (mgr->requests[i].active && mgr->requests[i].request.id == id) {
            return &mgr->requests[i];
        }
    }
    return NULL;
}

static ozayn_api_transaction_t *find_free_transaction(ozayn_api_manager_t *mgr) {
    for (uint32_t i = 0; i < OZAYN_API_MAX_REQUESTS; i++) {
        if (!mgr->requests[i].active) return &mgr->requests[i];
    }
    return NULL;
}

/* ================================================================
 * Names
 * ================================================================ */

const char *ozayn_api_error_name(ozayn_api_error_t error) {
    switch (error) {
        case OZAYN_API_ERR_OK:               return "OK";
        case OZAYN_API_ERR_NULL:             return "NULL";
        case OZAYN_API_ERR_INVALID:          return "INVALID";
        case OZAYN_API_ERR_NOT_FOUND:        return "NOT_FOUND";
        case OZAYN_API_ERR_EXISTS:           return "EXISTS";
        case OZAYN_API_ERR_BUSY:             return "BUSY";
        case OZAYN_API_ERR_TIMEOUT:          return "TIMEOUT";
        case OZAYN_API_ERR_NO_MEMORY:        return "NO_MEMORY";
        case OZAYN_API_ERR_STATE:            return "STATE";
        case OZAYN_API_ERR_PERMISSION:       return "PERMISSION";
        case OZAYN_API_ERR_DEPENDENCY:       return "DEPENDENCY";
        case OZAYN_API_ERR_VERSION_MISMATCH: return "VERSION_MISMATCH";
        case OZAYN_API_ERR_NOT_IMPLEMENTED:  return "NOT_IMPLEMENTED";
        case OZAYN_API_ERR_INTERNAL:         return "INTERNAL";
        case OZAYN_API_ERR_FULL:             return "FULL";
        case OZAYN_API_ERR_EMPTY:            return "EMPTY";
    }
    return "UNKNOWN";
}

const char *ozayn_api_method_name(ozayn_api_method_t method) {
    switch (method) {
        case OZAYN_METHOD_NONE:    return "NONE";
        case OZAYN_METHOD_GET:     return "GET";
        case OZAYN_METHOD_SET:     return "SET";
        case OZAYN_METHOD_CALL:    return "CALL";
        case OZAYN_METHOD_QUERY:   return "QUERY";
        case OZAYN_METHOD_NOTIFY:  return "NOTIFY";
        case OZAYN_METHOD_CREATE:  return "CREATE";
        case OZAYN_METHOD_DESTROY: return "DESTROY";
        case OZAYN_METHOD_START:   return "START";
        case OZAYN_METHOD_STOP:    return "STOP";
        case OZAYN_METHOD_RESTART: return "RESTART";
        case OZAYN_METHOD_STATUS:  return "STATUS";
        case OZAYN_METHOD_CUSTOM:  return "CUSTOM";
    }
    return "UNKNOWN";
}

const char *ozayn_api_stability_name(ozayn_api_stability_t stability) {
    switch (stability) {
        case OZAYN_API_STABLE:       return "STABLE";
        case OZAYN_API_EXPERIMENTAL: return "EXPERIMENTAL";
        case OZAYN_API_DEPRECATED:   return "DEPRECATED";
        case OZAYN_API_INTERNAL:     return "INTERNAL";
    }
    return "UNKNOWN";
}

const char *ozayn_api_compat_name(ozayn_api_compat_t compat) {
    switch (compat) {
        case OZAYN_API_COMPAT_OK:            return "OK";
        case OZAYN_API_COMPAT_REVISION_DIFF: return "REVISION_DIFF";
        case OZAYN_API_COMPAT_MINOR_BEHIND:  return "MINOR_BEHIND";
        case OZAYN_API_COMPAT_INCOMPATIBLE:  return "INCOMPATIBLE";
    }
    return "UNKNOWN";
}

const char *ozayn_api_req_status_name(ozayn_api_req_status_t status) {
    switch (status) {
        case OZAYN_REQ_PENDING:    return "PENDING";
        case OZAYN_REQ_PROCESSING: return "PROCESSING";
        case OZAYN_REQ_COMPLETED:  return "COMPLETED";
        case OZAYN_REQ_FAILED:     return "FAILED";
        case OZAYN_REQ_CANCELLED:  return "CANCELLED";
        case OZAYN_REQ_TIMEOUT:    return "TIMEOUT";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

int ozayn_api_init(ozayn_api_manager_t *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(*mgr));
    mgr->next_request_id = 1;
    LOG_INFO("CORE_API", "Core API manager initialized (max_interfaces=%u, max_requests=%u)",
             OZAYN_API_MAX_INTERFACES, OZAYN_API_MAX_REQUESTS);
    return 0;
}

void ozayn_api_shutdown(ozayn_api_manager_t *mgr) {
    if (!mgr) return;
    uint32_t pending = 0;
    for (uint32_t i = 0; i < OZAYN_API_MAX_REQUESTS; i++) {
        if (mgr->requests[i].active) pending++;
    }
    LOG_INFO("CORE_API", "Core API manager shut down (interfaces=%u, requests=%u, errors=%u, pending=%u)",
             mgr->total_requests > 0 ? 0u : 0u, mgr->total_requests,
             mgr->total_errors, pending);
    memset(mgr, 0, sizeof(*mgr));
}

void ozayn_api_set_events(ozayn_api_manager_t *mgr, ozayn_event_engine_t *events) {
    if (mgr) mgr->events = events;
}

/* ================================================================
 * Interface registration
 * ================================================================ */

int ozayn_api_register(ozayn_api_manager_t *mgr, const char *name,
                         const char *provider,
                         const ozayn_api_version_t *version,
                         ozayn_api_stability_t stability,
                         const char *description) {
    if (!mgr || !name || !name[0] || !provider || !provider[0]) return -1;
    if (find_interface(mgr, name)) return -2; /* already registered */

    for (uint32_t i = 0; i < OZAYN_API_MAX_INTERFACES; i++) {
        if (!mgr->interfaces[i].active) {
            ozayn_api_interface_t *iface = &mgr->interfaces[i];
            memset(iface, 0, sizeof(*iface));
            iface->active = 1;
            strncpy(iface->name, name, OZAYN_API_MAX_NAME - 1);
            strncpy(iface->provider, provider, OZAYN_API_MAX_NAME - 1);
            if (version) iface->version = *version;
            iface->stability = stability;
            if (description)
                strncpy(iface->description, description, OZAYN_API_MAX_DESC - 1);
            iface->registered_at = time(NULL);

            LOG_INFO("CORE_API", "Registered interface '%s' v%u.%u.%u by '%s' [%s]",
                     name,
                     version ? version->major : 0,
                     version ? version->minor : 0,
                     version ? version->patch : 0,
                     provider,
                     ozayn_api_stability_name(stability));
            publish_event(mgr, OZAYN_API_EVENT_INTERFACE_REGISTERED);
            return 0;
        }
    }
    return -3; /* no slot */
}

int ozayn_api_unregister(ozayn_api_manager_t *mgr, const char *name) {
    ozayn_api_interface_t *iface = find_interface(mgr, name);
    if (!iface) return -1;
    LOG_INFO("CORE_API", "Unregistered interface '%s'", iface->name);
    iface->active = 0;
    return 0;
}

/* ================================================================
 * Method registration
 * ================================================================ */

int ozayn_api_add_method(ozayn_api_manager_t *mgr, const char *interface_name,
                           ozayn_api_method_t method) {
    ozayn_api_interface_t *iface = find_interface(mgr, interface_name);
    if (!iface) return -1;
    if (iface->method_count >= 32) return -2;

    /* Check not already added */
    for (uint32_t i = 0; i < iface->method_count; i++) {
        if (iface->methods[i] == method) return 0;
    }

    iface->methods[iface->method_count++] = method;
    return 0;
}

/* ================================================================
 * Version checking
 * ================================================================ */

ozayn_api_compat_t ozayn_api_check_compat(const ozayn_api_version_t *provided,
                                           const ozayn_api_version_t *required) {
    if (!provided || !required) return OZAYN_API_COMPAT_INCOMPATIBLE;

    if (provided->major != required->major)
        return OZAYN_API_COMPAT_INCOMPATIBLE;

    if (provided->minor < required->minor)
        return OZAYN_API_COMPAT_MINOR_BEHIND;

    if (provided->revision != required->revision)
        return OZAYN_API_COMPAT_REVISION_DIFF;

    return OZAYN_API_COMPAT_OK;
}

int ozayn_api_check_provider(ozayn_api_manager_t *mgr, const char *interface_name,
                               uint16_t required_major) {
    ozayn_api_interface_t *iface = find_interface(mgr, interface_name);
    if (!iface) return 0;
    return iface->version.major == required_major;
}

/* ================================================================
 * Request/response flow
 * ================================================================ */

uint32_t ozayn_api_request_begin(ozayn_api_manager_t *mgr, const char *source,
                                  const char *target, const char *operation,
                                  ozayn_api_method_t method) {
    if (!mgr || !source || !target || !operation) return 0;

    ozayn_api_transaction_t *txn = find_free_transaction(mgr);
    if (!txn) return 0; /* queue full */

    memset(txn, 0, sizeof(*txn));
    txn->active = 1;
    txn->has_response = 0;

    ozayn_api_request_t *req = &txn->request;
    req->id = mgr->next_request_id++;
    req->method = method;
    strncpy(req->source, source, OZAYN_API_MAX_NAME - 1);
    strncpy(req->target, target, OZAYN_API_MAX_NAME - 1);
    strncpy(req->operation, operation, OZAYN_API_MAX_NAME - 1);
    req->created_at = time(NULL);
    req->timeout_ms = 5000; /* default 5s timeout */

    mgr->total_requests++;

    /* Update provider stats */
    ozayn_api_interface_t *iface = find_interface(mgr, target);
    if (iface) iface->total_requests++;

    LOG_INFO("CORE_API", "Request #%u: %s -> %s [%s] %s",
             req->id, source, target, operation, ozayn_api_method_name(method));
    return req->id;
}

int ozayn_api_request_complete(ozayn_api_manager_t *mgr, uint32_t request_id,
                                 ozayn_api_error_t error, const char *error_msg) {
    ozayn_api_transaction_t *txn = find_transaction(mgr, request_id);
    if (!txn) return -1;

    txn->has_response = 1;
    txn->response.request_id = request_id;
    txn->response.error = error;
    txn->response.completed_at = time(NULL);
    if (error_msg)
        strncpy(txn->response.error_msg, error_msg, OZAYN_API_MAX_DESC - 1);

    mgr->total_responses++;
    if (error != OZAYN_API_ERR_OK) {
        mgr->total_errors++;
        /* Update provider error stats */
        ozayn_api_interface_t *iface = find_interface(mgr, txn->request.target);
        if (iface) iface->total_errors++;
    }

    LOG_INFO("CORE_API", "Response #%u: %s (%s)",
             request_id, ozayn_api_error_name(error),
             error_msg ? error_msg : "ok");
    return 0;
}

int ozayn_api_request_cancel(ozayn_api_manager_t *mgr, uint32_t request_id) {
    ozayn_api_transaction_t *txn = find_transaction(mgr, request_id);
    if (!txn) return -1;

    txn->has_response = 1;
    txn->response.request_id = request_id;
    txn->response.error = OZAYN_API_ERR_BUSY;
    strncpy(txn->response.error_msg, "Cancelled", OZAYN_API_MAX_DESC - 1);
    txn->response.completed_at = time(NULL);

    LOG_INFO("CORE_API", "Request #%u cancelled", request_id);
    return 0;
}

/* ================================================================
 * Query
 * ================================================================ */

const ozayn_api_interface_t *ozayn_api_find_interface(ozayn_api_manager_t *mgr,
                                                       const char *name) {
    return find_interface(mgr, name);
}

ozayn_api_error_t ozayn_api_last_error(ozayn_api_manager_t *mgr) {
    if (!mgr) return OZAYN_API_ERR_NULL;
    return mgr->total_errors > 0 ? OZAYN_API_ERR_INTERNAL : OZAYN_API_ERR_OK;
}

int ozayn_api_has_interface(ozayn_api_manager_t *mgr, const char *name,
                              uint16_t required_major) {
    ozayn_api_interface_t *iface = find_interface(mgr, name);
    if (!iface) return 0;
    if (required_major > 0 && iface->version.major != required_major) return 0;
    return 1;
}

/* ================================================================
 * Stats
 * ================================================================ */

ozayn_api_stats_t ozayn_api_stats(ozayn_api_manager_t *mgr) {
    ozayn_api_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    for (uint32_t i = 0; i < OZAYN_API_MAX_INTERFACES; i++) {
        if (mgr->interfaces[i].active) s.total_interfaces++;
    }

    s.total_requests = mgr->total_requests;
    s.total_responses = mgr->total_responses;
    s.total_errors = mgr->total_errors;

    for (uint32_t i = 0; i < OZAYN_API_MAX_REQUESTS; i++) {
        if (mgr->requests[i].active && !mgr->requests[i].has_response)
            s.pending_requests++;
        if (mgr->requests[i].active && mgr->requests[i].has_response)
            s.completed_requests++;
    }

    return s;
}

/* ================================================================
 * Print / debug
 * ================================================================ */

void ozayn_api_print_interfaces(ozayn_api_manager_t *mgr) {
    if (!mgr) return;
    LOG_INFO("CORE_API", "=== Registered Interfaces ===");
    for (uint32_t i = 0; i < OZAYN_API_MAX_INTERFACES; i++) {
        if (mgr->interfaces[i].active) {
            ozayn_api_interface_t *iface = &mgr->interfaces[i];
            LOG_INFO("CORE_API", "  %-24s v%u.%u.%u [%s] by %-16s reqs=%u errs=%u",
                     iface->name,
                     iface->version.major, iface->version.minor, iface->version.patch,
                     ozayn_api_stability_name(iface->stability),
                     iface->provider,
                     iface->total_requests, iface->total_errors);
        }
    }
}

void ozayn_api_print_interface(ozayn_api_manager_t *mgr, const char *name) {
    ozayn_api_interface_t *iface = find_interface(mgr, name);
    if (!iface) {
        LOG_INFO("CORE_API", "Interface '%s' not found", name ? name : "(null)");
        return;
    }
    LOG_INFO("CORE_API", "=== Interface: %s ===", iface->name);
    LOG_INFO("CORE_API", "  Provider:    %s", iface->provider);
    LOG_INFO("CORE_API", "  Version:     %u.%u.%u (rev %u)",
             iface->version.major, iface->version.minor,
             iface->version.patch, iface->version.revision);
    LOG_INFO("CORE_API", "  Stability:   %s", ozayn_api_stability_name(iface->stability));
    LOG_INFO("CORE_API", "  Description: %s", iface->description);
    LOG_INFO("CORE_API", "  Requests:    %u", iface->total_requests);
    LOG_INFO("CORE_API", "  Errors:      %u", iface->total_errors);
    LOG_INFO("CORE_API", "  Methods:     %u", iface->method_count);
    for (uint32_t i = 0; i < iface->method_count; i++) {
        LOG_INFO("CORE_API", "    - %s", ozayn_api_method_name(iface->methods[i]));
    }
}

void ozayn_api_print_pending(ozayn_api_manager_t *mgr) {
    if (!mgr) return;
    LOG_INFO("CORE_API", "=== Pending Requests ===");
    int found = 0;
    for (uint32_t i = 0; i < OZAYN_API_MAX_REQUESTS; i++) {
        if (mgr->requests[i].active && !mgr->requests[i].has_response) {
            ozayn_api_request_t *req = &mgr->requests[i].request;
            LOG_INFO("CORE_API", "  #%u: %s -> %s [%s] %s",
                     req->id, req->source, req->target,
                     req->operation, ozayn_api_method_name(req->method));
            found++;
        }
    }
    if (!found) LOG_INFO("CORE_API", "  (none)");
}
