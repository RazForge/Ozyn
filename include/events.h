#ifndef OZAYN_EVENTS_H
#define OZAYN_EVENTS_H

#include "ozayn.h"
#include <time.h>

/*
 * ozayn_events.h — Internal event engine.
 *
 * Producers publish events. Subscribers register callbacks.
 * The Runtime loop drains the queue and dispatches synchronously.
 */

/* Event types — only those with actual producers/subscribers */
typedef enum {
    OZAYN_EVENT_NONE               = 0,
    OZAYN_EVENT_CORE_STARTED       = 1,
    OZAYN_EVENT_CORE_STOPPING      = 2,
    OZAYN_EVENT_RUNTIME_STARTED    = 3,
    OZAYN_EVENT_RUNTIME_STOPPING   = 4,
    OZAYN_EVENT_CONFIG_LOADED      = 5,
    OZAYN_EVENT_LOGGER_READY       = 6,
    OZAYN_EVENT_RECOVERY_RAISED    = 7,
    OZAYN_EVENT_TASK_CREATED       = 8,
    OZAYN_EVENT_TASK_COMPLETED     = 9,
    OZAYN_EVENT_TASK_FAILED        = 10,
    OZAYN_EVENT_TASK_CANCELLED     = 11,
    OZAYN_EVENT_PROCESS_STARTED    = 12,
    OZAYN_EVENT_PROCESS_EXITED     = 13,
    OZAYN_EVENT_PROCESS_FAILED     = 14,
    OZAYN_EVENT_MODULE_REGISTERED  = 15,
    OZAYN_EVENT_MODULE_INITIALIZED = 16,
    OZAYN_EVENT_MODULE_STARTED     = 17,
    OZAYN_EVENT_MODULE_STOPPED     = 18,
    OZAYN_EVENT_MODULE_SHUTDOWN    = 19,
    OZAYN_EVENT_MODULE_FAILED      = 20,
    OZAYN_EVENT_PLUGIN_DISCOVERED  = 21,
    OZAYN_EVENT_PLUGIN_LOADED      = 22,
    OZAYN_EVENT_PLUGIN_INITIALIZED = 23,
    OZAYN_EVENT_PLUGIN_STARTED     = 24,
    OZAYN_EVENT_PLUGIN_STOPPED     = 25,
    OZAYN_EVENT_PLUGIN_UNLOADED    = 26,
    OZAYN_EVENT_PLUGIN_FAILED      = 27,
    /* IPC events (28-33) */
    OZAYN_EVENT_IPC_STARTED        = 28,
    OZAYN_EVENT_IPC_STOPPING       = 29,
    OZAYN_EVENT_IPC_CLIENT_CONNECTED  = 30,
    OZAYN_EVENT_IPC_CLIENT_DISCONNECTED = 31,
    OZAYN_EVENT_IPC_REQUEST_RECEIVED = 32,
    OZAYN_EVENT_IPC_RESPONSE_SENT    = 33,
    OZAYN_EVENT_IPC_ERROR          = 34,
    /* Service Registry events (35-40) */
    OZAYN_EVENT_SERVICE_REGISTERED = 35,
    OZAYN_EVENT_SERVICE_READY      = 36,
    OZAYN_EVENT_SERVICE_DEGRADED   = 37,
    OZAYN_EVENT_SERVICE_FAILED     = 38,
    OZAYN_EVENT_SERVICE_OFFLINE    = 39,
    OZAYN_EVENT_SERVICE_UNREGISTERED = 40,
    /* Security events (41-47) */
    OZAYN_EVENT_IDENTITY_REGISTERED = 41,
    OZAYN_EVENT_AUTH_SUCCESS        = 42,
    OZAYN_EVENT_AUTH_FAILURE        = 43,
    OZAYN_EVENT_ACCESS_DENIED       = 44,
    OZAYN_EVENT_IDENTITY_REVOKED    = 45,
    OZAYN_EVENT_CREDENTIAL_EXPIRED  = 46,
    OZAYN_EVENT_SECURITY_ALERT      = 47,
    /* Authorization events (48-52) */
    OZAYN_EVENT_AUTHORIZATION_ALLOWED = 48,
    OZAYN_EVENT_AUTHORIZATION_DENIED  = 49,
    OZAYN_EVENT_ROLE_ASSIGNED         = 50,
    OZAYN_EVENT_ROLE_REVOKED          = 51,
    OZAYN_EVENT_POLICY_CHANGED        = 52,
    /* Resource Manager events (53-57) */
    OZAYN_EVENT_RESOURCE_CREATED      = 53,
    OZAYN_EVENT_RESOURCE_ALLOCATED    = 54,
    OZAYN_EVENT_RESOURCE_RELEASED     = 55,
    OZAYN_EVENT_RESOURCE_FAILED       = 56,
    OZAYN_EVENT_RESOURCE_ORPHANED     = 57,
    /* Scheduler events (58-64) */
    OZAYN_EVENT_SCHED_TASK_READY      = 58,
    OZAYN_EVENT_SCHED_TASK_STARTED    = 59,
    OZAYN_EVENT_SCHED_TASK_WAITING    = 60,
    OZAYN_EVENT_SCHED_TASK_BLOCKED    = 61,
    OZAYN_EVENT_SCHED_TASK_RESUMED    = 62,
    OZAYN_EVENT_SCHED_PRIORITY_CHANGED = 63,
    OZAYN_EVENT_SCHED_TASK_CANCELLED  = 64,
    /* Monitoring events (65-72) */
    OZAYN_EVENT_MONITORING_COLLECTED  = 65,
    OZAYN_EVENT_HEALTH_CHANGED        = 66,
    OZAYN_EVENT_HEALTH_CHECK_FAILED   = 67,
    OZAYN_EVENT_METRIC_UPDATED        = 68,
    OZAYN_EVENT_INCIDENT_CREATED      = 69,
    OZAYN_EVENT_INCIDENT_RESOLVED     = 70,
    OZAYN_EVENT_MONITORING_ERROR      = 71,
    OZAYN_EVENT_MONITORING_STARTED    = 72,
    /* Diagnostics events (73-84) */
    OZAYN_EVENT_DIAG_EVIDENCE_RECORDED = 73,
    OZAYN_EVENT_DIAG_FINDING_CREATED   = 74,
    OZAYN_EVENT_DIAG_SESSION_STARTED   = 75,
    OZAYN_EVENT_DIAG_SESSION_COMPLETED = 76,
    OZAYN_EVENT_DIAG_SNAPSHOT_CAPTURED = 77,
    OZAYN_EVENT_DIAG_FAILURE_RECORDED  = 78,
    OZAYN_EVENT_DIAG_REPEATED_FAILURE  = 79,
    OZAYN_EVENT_DIAG_LEVEL_CHANGED     = 80,
    OZAYN_EVENT_DIAG_TIMELINE_UPDATED  = 81,
    OZAYN_EVENT_DIAG_INVESTIGATION     = 82,
    OZAYN_EVENT_DIAG_ROOT_CAUSE        = 83,
    OZAYN_EVENT_DIAG_REDACTION         = 84,
    /* Security Boundary events (85-96) */
    OZAYN_EVENT_SEC_CONTEXT_REGISTERED = 85,
    OZAYN_EVENT_SEC_CONTEXT_REMOVED    = 86,
    OZAYN_EVENT_SEC_CAP_GRANTED        = 87,
    OZAYN_EVENT_SEC_CAP_REVOKED        = 88,
    OZAYN_EVENT_SEC_VIOLATION          = 89,
    OZAYN_EVENT_SEC_ACCESS_DENIED      = 90,
    OZAYN_EVENT_SEC_PRIVILEGE_BLOCKED  = 91,
    OZAYN_EVENT_SEC_SANDBOX_VIOLATION  = 92,
    OZAYN_EVENT_SEC_RESOURCE_ABUSE     = 93,
    OZAYN_EVENT_SEC_COMPONENT_RESTRICTED = 94,
    OZAYN_EVENT_SEC_COMPONENT_ISOLATED = 95,
    OZAYN_EVENT_SEC_COMPONENT_RESTORED = 96,
    OZAYN_EVENT_SEC_CHECK_PERFORMED    = 97,
} ozayn_event_type_t;

/* Event source */
typedef enum {
    OZAYN_SRC_CORE     = 0,
    OZAYN_SRC_RUNTIME  = 1,
    OZAYN_SRC_CONFIG   = 2,
    OZAYN_SRC_LOGGER   = 3,
    OZAYN_SRC_RECOVERY = 4,
    OZAYN_SRC_USER     = 5,
    OZAYN_SRC_MODULE   = 6,
    OZAYN_SRC_PLUGIN   = 7,
    OZAYN_SRC_IPC      = 8,
    OZAYN_SRC_REGISTRY = 9,
    OZAYN_SRC_SECURITY = 10,
} ozayn_event_source_t;

/* Event structure */
typedef struct {
    ozayn_event_type_t   type;
    ozayn_event_source_t source;
    time_t               timestamp;
    void                *payload;  /* owned by producer, opaque to engine */
} ozayn_event_t;

/* Subscriber callback */
typedef void (*ozayn_event_handler_t)(const ozayn_event_t *event, void *context);

/* Subscription handle */
typedef struct {
    int                   active;
    ozayn_event_type_t    type;
    ozayn_event_handler_t handler;
    void                 *context;
} ozayn_subscription_t;

/* Event engine configuration */
typedef struct {
    int queue_capacity;      /* max queued events */
    int max_subscribers;     /* max subscriptions */
} ozayn_event_config_t;

/* Event engine */
typedef struct ozayn_event_engine_s {
    ozayn_event_t      *queue;
    int                 queue_capacity;
    int                 queue_head;
    int                 queue_tail;
    int                 queue_count;

    ozayn_subscription_t *subscriptions;
    int                 max_subscribers;
    int                 sub_count;

    int                 dispatching;
    int                 initialized;
} ozayn_event_engine_t;

/* Lifecycle */
ozayn_result_t ozayn_events_init(ozayn_event_engine_t *engine, const ozayn_event_config_t *cfg);
void           ozayn_events_shutdown(ozayn_event_engine_t *engine);

/* Publish */
ozayn_result_t ozayn_events_publish(ozayn_event_engine_t *engine,
                                    ozayn_event_type_t type,
                                    ozayn_event_source_t source,
                                    void *payload);

/* Subscribe / unsubscribe */
int            ozayn_events_subscribe(ozayn_event_engine_t *engine,
                                      ozayn_event_type_t type,
                                      ozayn_event_handler_t handler,
                                      void *context);
void           ozayn_events_unsubscribe(ozayn_event_engine_t *engine, int sub_id);

/* Process — called from Runtime loop */
int            ozayn_events_process(ozayn_event_engine_t *engine);

/* Query */
const char    *ozayn_event_type_name(ozayn_event_type_t type);
const char    *ozayn_event_source_name(ozayn_event_source_t src);
int            ozayn_events_queue_count(const ozayn_event_engine_t *engine);

#endif
