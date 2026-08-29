#ifndef OZAYN_RECOVERY_H
#define OZAYN_RECOVERY_H

#include "ozayn.h"

/*
 * ozayn_recovery.h — Error handling and recovery system.
 *
 * Classifies failures, logs them, determines recovery strategy.
 * Lightweight: a context struct + policy functions, not a background daemon.
 */

/* Error categories */
typedef enum {
    OZAYN_ERRCAT_CONFIG    = 0,
    OZAYN_ERRCAT_RUNTIME   = 1,
    OZAYN_ERRCAT_RESOURCE  = 2,
    OZAYN_ERRCAT_SYSTEM    = 3,
    OZAYN_ERRCAT_MODULE    = 4,
    OZAYN_ERRCAT_PLUGIN    = 5,
    OZAYN_ERRCAT_IPC       = 6,
    OZAYN_ERRCAT_SECURITY  = 7,
    OZAYN_ERRCAT_INTERNAL  = 8,
} ozayn_errcat_t;

/* Failure scope — what is affected */
typedef enum {
    OZAYN_SCOPE_OPERATION  = 0,  /* one operation failed */
    OZAYN_SCOPE_COMPONENT  = 1,  /* one component failed */
    OZAYN_SCOPE_SUBSYSTEM  = 2,  /* major subsystem failed */
    OZAYN_SCOPE_CORE       = 3,  /* core state compromised */
} ozayn_scope_t;

/* Recovery actions */
typedef enum {
    OZAYN_RECOVERY_IGNORE   = 0,
    OZAYN_RECOVERY_RETRY    = 1,
    OZAYN_RECOVERY_RESET    = 2,
    OZAYN_RECOVERY_ISOLATE  = 3,
    OZAYN_RECOVERY_DEGRADE  = 4,
    OZAYN_RECOVERY_SHUTDOWN = 5,
} ozayn_recovery_action_t;

/* Error record */
typedef struct {
    ozayn_errcat_t   category;
    int              severity;      /* maps to log levels */
    ozayn_scope_t    scope;
    const char      *component;
    const char      *message;
    ozayn_recovery_action_t action;
    int              retry_count;
} ozayn_error_t;

/* Error context — tracks error state */
typedef struct {
    ozayn_error_t last_error;
    int           has_error;
    int           total_errors;
    int           consecutive_retries;
} ozayn_recovery_t;

/* Lifecycle */
void ozayn_recovery_init(ozayn_recovery_t *rec);

/* Report an error */
void ozayn_recovery_raise(ozayn_recovery_t *rec,
                          ozayn_errcat_t category,
                          int severity,
                          ozayn_scope_t scope,
                          const char *component,
                          const char *message);

/* Evaluate and return recovery action */
ozayn_recovery_action_t ozayn_recovery_evaluate(const ozayn_recovery_t *rec);

/* Check if system should shutdown */
int ozayn_recovery_should_shutdown(const ozayn_recovery_t *rec);

/* Reset after successful recovery */
void ozayn_recovery_clear(ozayn_recovery_t *rec);

/* Query */
const char *ozayn_errcat_name(ozayn_errcat_t cat);
const char *ozayn_scope_name(ozayn_scope_t scope);
const char *ozayn_recovery_action_name(ozayn_recovery_action_t action);

#endif
