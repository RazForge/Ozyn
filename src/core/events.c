#include "events.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

/* ---------- Names ---------- */

const char *ozayn_event_type_name(ozayn_event_type_t type) {
    switch (type) {
        case OZAYN_EVENT_NONE:             return "NONE";
        case OZAYN_EVENT_CORE_STARTED:     return "CORE_STARTED";
        case OZAYN_EVENT_CORE_STOPPING:    return "CORE_STOPPING";
        case OZAYN_EVENT_RUNTIME_STARTED:  return "RUNTIME_STARTED";
        case OZAYN_EVENT_RUNTIME_STOPPING: return "RUNTIME_STOPPING";
        case OZAYN_EVENT_CONFIG_LOADED:    return "CONFIG_LOADED";
        case OZAYN_EVENT_LOGGER_READY:     return "LOGGER_READY";
        case OZAYN_EVENT_RECOVERY_RAISED:  return "RECOVERY_RAISED";
        case OZAYN_EVENT_TASK_CREATED:     return "TASK_CREATED";
        case OZAYN_EVENT_TASK_COMPLETED:   return "TASK_COMPLETED";
        case OZAYN_EVENT_TASK_FAILED:      return "TASK_FAILED";
        case OZAYN_EVENT_TASK_CANCELLED:   return "TASK_CANCELLED";
        case OZAYN_EVENT_PROCESS_STARTED:  return "PROCESS_STARTED";
        case OZAYN_EVENT_PROCESS_EXITED:   return "PROCESS_EXITED";
        case OZAYN_EVENT_PROCESS_FAILED:   return "PROCESS_FAILED";
        case OZAYN_EVENT_MODULE_REGISTERED:  return "MODULE_REGISTERED";
        case OZAYN_EVENT_MODULE_INITIALIZED: return "MODULE_INITIALIZED";
        case OZAYN_EVENT_MODULE_STARTED:     return "MODULE_STARTED";
        case OZAYN_EVENT_MODULE_STOPPED:     return "MODULE_STOPPED";
        case OZAYN_EVENT_MODULE_SHUTDOWN:    return "MODULE_SHUTDOWN";
        case OZAYN_EVENT_MODULE_FAILED:      return "MODULE_FAILED";
        case OZAYN_EVENT_PLUGIN_DISCOVERED:  return "PLUGIN_DISCOVERED";
        case OZAYN_EVENT_PLUGIN_LOADED:      return "PLUGIN_LOADED";
        case OZAYN_EVENT_PLUGIN_INITIALIZED: return "PLUGIN_INITIALIZED";
        case OZAYN_EVENT_PLUGIN_STARTED:     return "PLUGIN_STARTED";
        case OZAYN_EVENT_PLUGIN_STOPPED:     return "PLUGIN_STOPPED";
        case OZAYN_EVENT_PLUGIN_UNLOADED:    return "PLUGIN_UNLOADED";
        case OZAYN_EVENT_PLUGIN_FAILED:      return "PLUGIN_FAILED";
        case OZAYN_EVENT_IPC_STARTED:        return "IPC_STARTED";
        case OZAYN_EVENT_IPC_STOPPING:       return "IPC_STOPPING";
        case OZAYN_EVENT_IPC_CLIENT_CONNECTED:    return "IPC_CLIENT_CONNECTED";
        case OZAYN_EVENT_IPC_CLIENT_DISCONNECTED: return "IPC_CLIENT_DISCONNECTED";
        case OZAYN_EVENT_IPC_REQUEST_RECEIVED:    return "IPC_REQUEST_RECEIVED";
        case OZAYN_EVENT_IPC_RESPONSE_SENT:       return "IPC_RESPONSE_SENT";
        case OZAYN_EVENT_IPC_ERROR:          return "IPC_ERROR";
    }
    return "UNKNOWN";
}

const char *ozayn_event_source_name(ozayn_event_source_t src) {
    switch (src) {
        case OZAYN_SRC_CORE:     return "CORE";
        case OZAYN_SRC_RUNTIME:  return "RUNTIME";
        case OZAYN_SRC_CONFIG:   return "CONFIG";
        case OZAYN_SRC_LOGGER:   return "LOGGER";
        case OZAYN_SRC_RECOVERY: return "RECOVERY";
        case OZAYN_SRC_USER:     return "USER";
        case OZAYN_SRC_MODULE:   return "MODULE";
        case OZAYN_SRC_PLUGIN:   return "PLUGIN";
        case OZAYN_SRC_IPC:      return "IPC";
    }
    return "UNKNOWN";
}

/* ---------- Init ---------- */

ozayn_result_t ozayn_events_init(ozayn_event_engine_t *engine, const ozayn_event_config_t *cfg) {
    if (!engine || !cfg) return OZAYN_ERR_NULL;

    memset(engine, 0, sizeof(ozayn_event_engine_t));

    engine->queue_capacity = cfg->queue_capacity;
    engine->queue = calloc((size_t)cfg->queue_capacity, sizeof(ozayn_event_t));
    if (!engine->queue) return OZAYN_ERR;

    engine->max_subscribers = cfg->max_subscribers;
    engine->subscriptions = calloc((size_t)cfg->max_subscribers, sizeof(ozayn_subscription_t));
    if (!engine->subscriptions) {
        free(engine->queue);
        return OZAYN_ERR;
    }

    engine->queue_head = 0;
    engine->queue_tail = 0;
    engine->queue_count = 0;
    engine->sub_count = 0;
    engine->dispatching = 0;
    engine->initialized = 1;

    LOG_INFO("EVENTS", "Event engine initialized (queue=%d, subscribers=%d)",
             cfg->queue_capacity, cfg->max_subscribers);

    return OZAYN_OK;
}

/* ---------- Shutdown ---------- */

void ozayn_events_shutdown(ozayn_event_engine_t *engine) {
    if (!engine || !engine->initialized) return;

    free(engine->queue);
    engine->queue = NULL;

    free(engine->subscriptions);
    engine->subscriptions = NULL;

    engine->initialized = 0;
    engine->queue_count = 0;
    engine->sub_count = 0;

    LOG_INFO("EVENTS", "Event engine shut down");
}

/* ---------- Publish ---------- */

ozayn_result_t ozayn_events_publish(ozayn_event_engine_t *engine,
                                    ozayn_event_type_t type,
                                    ozayn_event_source_t source,
                                    void *payload) {
    if (!engine || !engine->initialized) return OZAYN_ERR_STATE;
    if (type == OZAYN_EVENT_NONE) return OZAYN_ERR;

    /* Queue full → drop event, log warning */
    if (engine->queue_count >= engine->queue_capacity) {
        LOG_WARN("EVENTS", "Queue overflow — dropping %s from %s",
                 ozayn_event_type_name(type), ozayn_event_source_name(source));
        return OZAYN_ERR;
    }

    ozayn_event_t *ev = &engine->queue[engine->queue_tail];
    ev->type      = type;
    ev->source    = source;
    ev->timestamp = time(NULL);
    ev->payload   = payload;

    engine->queue_tail = (engine->queue_tail + 1) % engine->queue_capacity;
    engine->queue_count++;

    LOG_DEBUG("EVENTS", "Published %s from %s",
              ozayn_event_type_name(type), ozayn_event_source_name(source));

    return OZAYN_OK;
}

/* ---------- Subscribe ---------- */

int ozayn_events_subscribe(ozayn_event_engine_t *engine,
                           ozayn_event_type_t type,
                           ozayn_event_handler_t handler,
                           void *context) {
    if (!engine || !engine->initialized) return -1;
    if (!handler) return -1;

    if (engine->sub_count >= engine->max_subscribers) {
        LOG_WARN("EVENTS", "Subscriber limit reached");
        return -1;
    }

    int id = engine->sub_count;
    ozayn_subscription_t *sub = &engine->subscriptions[id];
    sub->active  = 1;
    sub->type    = type;
    sub->handler = handler;
    sub->context = context;

    engine->sub_count++;

    LOG_DEBUG("EVENTS", "Subscribed to %s (id=%d)", ozayn_event_type_name(type), id);
    return id;
}

/* ---------- Unsubscribe ---------- */

void ozayn_events_unsubscribe(ozayn_event_engine_t *engine, int sub_id) {
    if (!engine || !engine->initialized) return;
    if (sub_id < 0 || sub_id >= engine->max_subscribers) return;

    engine->subscriptions[sub_id].active = 0;
    LOG_DEBUG("EVENTS", "Unsubscribed id=%d", sub_id);
}

/* ---------- Process ---------- */

int ozayn_events_process(ozayn_event_engine_t *engine) {
    if (!engine || !engine->initialized) return 0;
    if (engine->dispatching) return 0; /* prevent reentrant dispatch */

    int processed = 0;
    engine->dispatching = 1;

    while (engine->queue_count > 0) {
        /* Pop front */
        ozayn_event_t ev = engine->queue[engine->queue_head];
        engine->queue_head = (engine->queue_head + 1) % engine->queue_capacity;
        engine->queue_count--;

        /* Dispatch to matching subscribers */
        for (int i = 0; i < engine->sub_count; i++) {
            ozayn_subscription_t *sub = &engine->subscriptions[i];
            if (!sub->active) continue;
            if (sub->type != OZAYN_EVENT_NONE && sub->type != ev.type) continue;

            if (sub->handler) {
                sub->handler(&ev, sub->context);
            }
        }

        processed++;
    }

    engine->dispatching = 0;
    return processed;
}

/* ---------- Query ---------- */

int ozayn_events_queue_count(const ozayn_event_engine_t *engine) {
    if (!engine) return 0;
    return engine->queue_count;
}
