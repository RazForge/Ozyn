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
} ozayn_event_type_t;

/* Event source */
typedef enum {
    OZAYN_SRC_CORE     = 0,
    OZAYN_SRC_RUNTIME  = 1,
    OZAYN_SRC_CONFIG   = 2,
    OZAYN_SRC_LOGGER   = 3,
    OZAYN_SRC_RECOVERY = 4,
    OZAYN_SRC_USER     = 5,
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
