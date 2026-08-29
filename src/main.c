#include "ozayn.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* Load configuration */
    ozayn_config_object_t cfg;
    if (ozayn_config_load(&cfg) != OZAYN_OK) {
        fprintf(stderr, "[%s] Failed to load configuration.\n", OZAYN_NAME);
        return 1;
    }

    if (ozayn_config_validate(&cfg) != OZAYN_OK) {
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
        fprintf(stderr, "[%s] Failed to initialize logger.\n", OZAYN_NAME);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    LOG_INFO("CORE", "OZAYN Core v%s starting", OZAYN_VERSION);
    LOG_INFO("CONFIG", "Configuration loaded (log_level=%s)",
             ozayn_log_level_name(cfg.values.log_level));

    /* Create runtime */
    ozayn_runtime_t *rt = ozayn_runtime_create();
    if (!rt) {
        LOG_CRITICAL("CORE", "Failed to create runtime");
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Bind config and logger to runtime */
    ozayn_runtime_set_config(rt, &cfg.values);

    /* Initialize runtime */
    if (ozayn_runtime_init(rt) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize runtime");
        ozayn_runtime_destroy(rt);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    ozayn_core_print_status(&rt->core);
    LOG_INFO("RUNTIME", "Runtime started (interval=%ds)", cfg.values.runtime_interval);

    ozayn_runtime_set_stop_flag(rt, &g_stop);
    ozayn_runtime_run(rt);

    LOG_INFO("RUNTIME", "Runtime shutting down");
    ozayn_runtime_shutdown(rt);
    ozayn_runtime_destroy(rt);

    LOG_INFO("CORE", "OZAYN Core shutdown complete");
    ozayn_logger_shutdown(&logger);
    ozayn_config_destroy(&cfg);

    return 0;
}
