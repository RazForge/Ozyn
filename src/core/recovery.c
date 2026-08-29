#include "recovery.h"
#include "logger.h"
#include <string.h>

/* ---------- Names ---------- */

const char *ozayn_errcat_name(ozayn_errcat_t cat) {
    switch (cat) {
        case OZAYN_ERRCAT_CONFIG:   return "CONFIG";
        case OZAYN_ERRCAT_RUNTIME:  return "RUNTIME";
        case OZAYN_ERRCAT_RESOURCE: return "RESOURCE";
        case OZAYN_ERRCAT_SYSTEM:   return "SYSTEM";
        case OZAYN_ERRCAT_MODULE:   return "MODULE";
        case OZAYN_ERRCAT_PLUGIN:   return "PLUGIN";
        case OZAYN_ERRCAT_IPC:      return "IPC";
        case OZAYN_ERRCAT_SECURITY: return "SECURITY";
        case OZAYN_ERRCAT_INTERNAL: return "INTERNAL";
    }
    return "UNKNOWN";
}

const char *ozayn_scope_name(ozayn_scope_t scope) {
    switch (scope) {
        case OZAYN_SCOPE_OPERATION: return "OPERATION";
        case OZAYN_SCOPE_COMPONENT: return "COMPONENT";
        case OZAYN_SCOPE_SUBSYSTEM: return "SUBSYSTEM";
        case OZAYN_SCOPE_CORE:      return "CORE";
    }
    return "UNKNOWN";
}

const char *ozayn_recovery_action_name(ozayn_recovery_action_t action) {
    switch (action) {
        case OZAYN_RECOVERY_IGNORE:   return "IGNORE";
        case OZAYN_RECOVERY_RETRY:    return "RETRY";
        case OZAYN_RECOVERY_RESET:    return "RESET";
        case OZAYN_RECOVERY_ISOLATE:  return "ISOLATE";
        case OZAYN_RECOVERY_DEGRADE:  return "DEGRADE";
        case OZAYN_RECOVERY_SHUTDOWN: return "SHUTDOWN";
    }
    return "UNKNOWN";
}

/* ---------- Init ---------- */

void ozayn_recovery_init(ozayn_recovery_t *rec) {
    if (!rec) return;
    memset(rec, 0, sizeof(ozayn_recovery_t));
}

/* ---------- Raise error ---------- */

#define MAX_RETRIES 3

void ozayn_recovery_raise(ozayn_recovery_t *rec,
                          ozayn_errcat_t category,
                          int severity,
                          ozayn_scope_t scope,
                          const char *component,
                          const char *message) {
    if (!rec) return;

    rec->has_error = 1;
    rec->total_errors++;

    rec->last_error.category   = category;
    rec->last_error.severity   = severity;
    rec->last_error.scope      = scope;
    rec->last_error.component  = component;
    rec->last_error.message    = message;
    rec->last_error.retry_count = 0;

    /* Evaluate recovery action */
    rec->last_error.action = ozayn_recovery_evaluate(rec);

    /* Log the error */
    const char *cat_name = ozayn_errcat_name(category);
    const char *scope_name = ozayn_scope_name(scope);

    if (severity >= OZAYN_LOG_CRITICAL) {
        LOG_CRITICAL("RECOVERY", "[%s] [%s] %s", cat_name, scope_name, message);
    } else if (severity >= OZAYN_LOG_ERROR) {
        LOG_ERROR("RECOVERY", "[%s] [%s] %s", cat_name, scope_name, message);
    } else if (severity >= OZAYN_LOG_WARNING) {
        LOG_WARN("RECOVERY", "[%s] [%s] %s", cat_name, scope_name, message);
    } else {
        LOG_INFO("RECOVERY", "[%s] [%s] %s", cat_name, scope_name, message);
    }

    /* Log the action */
    LOG_INFO("RECOVERY", "Action: %s", ozayn_recovery_action_name(rec->last_error.action));
}

/* ---------- Evaluate ---------- */

ozayn_recovery_action_t ozayn_recovery_evaluate(const ozayn_recovery_t *rec) {
    if (!rec || !rec->has_error) return OZAYN_RECOVERY_IGNORE;

    const ozayn_error_t *e = &rec->last_error;

    /* Core scope → always shutdown */
    if (e->scope == OZAYN_SCOPE_CORE) return OZAYN_RECOVERY_SHUTDOWN;

    /* Security → never retry, shutdown */
    if (e->category == OZAYN_ERRCAT_SECURITY) return OZAYN_RECOVERY_SHUTDOWN;

    /* Subsystem scope → degrade */
    if (e->scope == OZAYN_SCOPE_SUBSYSTEM) return OZAYN_RECOVERY_DEGRADE;

    /* Component scope → isolate */
    if (e->scope == OZAYN_SCOPE_COMPONENT) return OZAYN_RECOVERY_ISOLATE;

    /* Operation scope with critical severity → shutdown */
    if (e->scope == OZAYN_SCOPE_OPERATION && e->severity >= OZAYN_LOG_CRITICAL)
        return OZAYN_RECOVERY_SHUTDOWN;

    /* Operation scope with error severity → retry if under limit */
    if (e->scope == OZAYN_SCOPE_OPERATION && e->severity >= OZAYN_LOG_ERROR) {
        if (rec->consecutive_retries < MAX_RETRIES)
            return OZAYN_RECOVERY_RETRY;
        else
            return OZAYN_RECOVERY_SHUTDOWN; /* escalate after max retries */
    }

    /* Operation scope with warning → ignore (transient) */
    if (e->severity <= OZAYN_LOG_WARNING) return OZAYN_RECOVERY_IGNORE;

    return OZAYN_RECOVERY_IGNORE;
}

/* ---------- Should shutdown ---------- */

int ozayn_recovery_should_shutdown(const ozayn_recovery_t *rec) {
    if (!rec || !rec->has_error) return 0;
    return rec->last_error.action == OZAYN_RECOVERY_SHUTDOWN;
}

/* ---------- Clear ---------- */

void ozayn_recovery_clear(ozayn_recovery_t *rec) {
    if (!rec) return;
    rec->has_error = 0;
    rec->consecutive_retries = 0;
}
