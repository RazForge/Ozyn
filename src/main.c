#include "ozayn.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

/* Simple subscriber for demonstration */
static void on_event(const ozayn_event_t *event, void *context) {
    (void)context;
    LOG_INFO("EVENTS", "Subscriber received: %s from %s",
             ozayn_event_type_name(event->type),
             ozayn_event_source_name(event->source));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* Initialize error recovery */
    ozayn_recovery_t recovery;
    ozayn_recovery_init(&recovery);

    /* Load configuration */
    ozayn_config_object_t cfg;
    if (ozayn_config_load(&cfg) != OZAYN_OK) {
        ozayn_recovery_raise(&recovery, OZAYN_ERRCAT_CONFIG, OZAYN_LOG_CRITICAL,
                             OZAYN_SCOPE_CORE, "CONFIG", "Failed to load configuration");
        fprintf(stderr, "[%s] Failed to load configuration.\n", OZAYN_NAME);
        return 1;
    }

    if (ozayn_config_validate(&cfg) != OZAYN_OK) {
        ozayn_recovery_raise(&recovery, OZAYN_ERRCAT_CONFIG, OZAYN_LOG_CRITICAL,
                             OZAYN_SCOPE_CORE, "CONFIG", "Configuration validation failed");
        fprintf(stderr, "[%s] Configuration validation failed.\n", OZAYN_NAME);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Initialize logger from config */
    ozayn_logger_t logger;
    ozayn_log_config_t lcfg = {
        .console_enabled = cfg.values.log_console,
        .file_enabled    = cfg.values.log_file,
        .min_level       = cfg.values.log_level,
    };
    snprintf(lcfg.directory, sizeof(lcfg.directory), "%s",
             cfg.values.log_directory[0] ? cfg.values.log_directory : "logs");

    if (ozayn_logger_init(&logger, &lcfg) != OZAYN_OK) {
        ozayn_recovery_raise(&recovery, OZAYN_ERRCAT_RESOURCE, OZAYN_LOG_CRITICAL,
                             OZAYN_SCOPE_CORE, "LOGGER", "Failed to initialize logger");
        fprintf(stderr, "[%s] Failed to initialize logger.\n", OZAYN_NAME);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    LOG_INFO("CORE", "OZAYN Core v%s starting", OZAYN_VERSION);
    LOG_INFO("CONFIG", "Configuration loaded (log_level=%s)",
             ozayn_log_level_name(cfg.values.log_level));

    /* Initialize event engine */
    ozayn_event_engine_t events;
    ozayn_event_config_t ecfg = {
        .queue_capacity  = 256,
        .max_subscribers = 32,
    };

    if (ozayn_events_init(&events, &ecfg) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize event engine");
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Subscribe to all events for demonstration */
    ozayn_events_subscribe(&events, OZAYN_EVENT_NONE, on_event, NULL);

    /* Create runtime */
    ozayn_runtime_t *rt = ozayn_runtime_create();
    if (!rt) {
        ozayn_recovery_raise(&recovery, OZAYN_ERRCAT_RUNTIME, OZAYN_LOG_CRITICAL,
                             OZAYN_SCOPE_CORE, "RUNTIME", "Failed to create runtime");
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Bind config, events to runtime */
    ozayn_runtime_set_config(rt, &cfg.values);
    ozayn_runtime_set_events(rt, &events);

    /* Initialize runtime */
    if (ozayn_runtime_init(rt) != OZAYN_OK) {
        ozayn_recovery_raise(&recovery, OZAYN_ERRCAT_RUNTIME, OZAYN_LOG_CRITICAL,
                             OZAYN_SCOPE_CORE, "RUNTIME", "Failed to initialize runtime");
        ozayn_runtime_destroy(rt);
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Publish startup events */
    ozayn_events_publish(&events, OZAYN_EVENT_CONFIG_LOADED, OZAYN_SRC_CONFIG, NULL);
    ozayn_events_publish(&events, OZAYN_EVENT_LOGGER_READY, OZAYN_SRC_LOGGER, NULL);
    ozayn_events_publish(&events, OZAYN_EVENT_RUNTIME_STARTED, OZAYN_SRC_RUNTIME, NULL);

    /* Process startup events before entering main loop */
    ozayn_events_process(&events);

    ozayn_core_print_status(&rt->core);
    LOG_INFO("RUNTIME", "Runtime started (interval=%ds)", cfg.values.runtime_interval);

    ozayn_runtime_set_stop_flag(rt, &g_stop);
    ozayn_runtime_run(rt);

    /* Publish shutdown events */
    ozayn_events_publish(&events, OZAYN_EVENT_RUNTIME_STOPPING, OZAYN_SRC_RUNTIME, NULL);
    ozayn_events_process(&events);

    LOG_INFO("RUNTIME", "Runtime shutting down");
    ozayn_runtime_shutdown(rt);
    ozayn_runtime_destroy(rt);

    LOG_INFO("CORE", "OZAYN Core shutdown complete");
    ozayn_events_shutdown(&events);
    ozayn_logger_shutdown(&logger);
    ozayn_config_destroy(&cfg);

    return 0;
}
