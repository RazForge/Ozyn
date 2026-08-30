#include "ozayn.h"
#include <stdio.h>
#include <string.h>
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

/* --- Test Module: logger_test --- */

static ozayn_result_t logger_test_init(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module initialized");
    return OZAYN_OK;
}

static ozayn_result_t logger_test_start(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module started — ready to log events");
    return OZAYN_OK;
}

static void logger_test_stop(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module stopped");
}

static void logger_test_shutdown(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module shut down — resources released");
}

static const ozayn_module_entry_t logger_test_module = {
    .name        = "logger_test",
    .version     = "0.1",
    .description = "Test module that logs lifecycle events",
    .init        = logger_test_init,
    .start       = logger_test_start,
    .stop        = logger_test_stop,
    .shutdown    = logger_test_shutdown,
};

/* --- Test Module: failure_test --- */

static ozayn_result_t failure_test_init(void *engine) {
    (void)engine;
    LOG_ERROR("FAILURE_TEST", "Intentional init failure for demonstration");
    return OZAYN_ERR;
}

static const ozayn_module_entry_t failure_test_module = {
    .name        = "failure_test",
    .version     = "0.1",
    .description = "Test module that always fails init to demonstrate isolation",
    .init        = failure_test_init,
    .start       = NULL,
    .stop        = NULL,
    .shutdown    = NULL,
};

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

    /* Initialize command engine */
    ozayn_command_engine_t cmd_engine;
    if (ozayn_command_engine_init(&cmd_engine) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize command engine");
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Initialize task manager */
    ozayn_task_manager_t task_mgr;
    if (ozayn_task_manager_init(&task_mgr) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize task manager");
        ozayn_command_engine_shutdown(&cmd_engine);
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Initialize process manager */
    ozayn_process_manager_t proc_mgr;
    if (ozayn_process_manager_init(&proc_mgr) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize process manager");
        ozayn_task_manager_shutdown(&task_mgr);
        ozayn_command_engine_shutdown(&cmd_engine);
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

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

    /* Bind runtime, events, recovery to command engine */
    ozayn_command_engine_set_runtime(&cmd_engine, rt);
    ozayn_command_engine_set_events(&cmd_engine, &events);
    ozayn_command_engine_set_recovery(&cmd_engine, &recovery);

    /* Bind events, recovery to task manager */
    ozayn_task_manager_set_events(&task_mgr, &events);
    ozayn_task_manager_set_recovery(&task_mgr, &recovery);

    /* Bind events, recovery to process manager */
    ozayn_process_manager_set_events(&proc_mgr, &events);
    ozayn_process_manager_set_recovery(&proc_mgr, &recovery);

    /* Bind process manager to runtime (for reap in loop) */
    ozayn_runtime_set_process_mgr(rt, &proc_mgr);

    /* Initialize module manager */
    ozayn_module_manager_t mod_mgr;
    if (ozayn_module_manager_init(&mod_mgr) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize module manager");
        ozayn_process_manager_shutdown(&proc_mgr);
        ozayn_task_manager_shutdown(&task_mgr);
        ozayn_command_engine_shutdown(&cmd_engine);
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Bind engine pointers to module manager */
    ozayn_module_manager_set_logger(&mod_mgr, &logger);
    ozayn_module_manager_set_events(&mod_mgr, &events);
    ozayn_module_manager_set_recovery(&mod_mgr, &recovery);
    ozayn_module_manager_set_config(&mod_mgr, &cfg);
    ozayn_module_manager_set_runtime(&mod_mgr, rt);

    /* Bind module manager to runtime */
    ozayn_runtime_set_module_mgr(rt, &mod_mgr);

    /* Initialize plugin manager */
    ozayn_plugin_manager_t plug_mgr;
    if (ozayn_plugin_manager_init(&plug_mgr) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize plugin manager");
        ozayn_module_manager_shutdown(&mod_mgr);
        ozayn_process_manager_shutdown(&proc_mgr);
        ozayn_task_manager_shutdown(&task_mgr);
        ozayn_command_engine_shutdown(&cmd_engine);
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Bind engine pointers to plugin manager */
    ozayn_plugin_manager_set_logger(&plug_mgr, &logger);
    ozayn_plugin_manager_set_events(&plug_mgr, &events);
    ozayn_plugin_manager_set_recovery(&plug_mgr, &recovery);
    ozayn_plugin_manager_set_config(&plug_mgr, &cfg);
    ozayn_plugin_manager_set_runtime(&plug_mgr, rt);

    /* Bind plugin manager to runtime */
    ozayn_runtime_set_plugin_mgr(rt, &plug_mgr);

    /* Initialize IPC manager */
    ozayn_ipc_config_t ipc_cfg = {
        .enabled          = cfg.values.ipc_enabled,
        .max_msg_size     = cfg.values.ipc_max_msg_size,
        .max_connections  = cfg.values.ipc_max_connections,
    };
    snprintf(ipc_cfg.endpoint, sizeof(ipc_cfg.endpoint), "%s",
             cfg.values.ipc_endpoint[0] ? cfg.values.ipc_endpoint : "runtime/ipc/ozayn.sock");

    ozayn_ipc_manager_t ipc_mgr;
    if (ozayn_ipc_manager_init(&ipc_mgr, &ipc_cfg) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize IPC manager");
        /* Non-fatal: continue without IPC */
    }

    /* Bind engine pointers to IPC manager */
    ozayn_ipc_manager_set_events(&ipc_mgr, &events);
    ozayn_ipc_manager_set_recovery(&ipc_mgr, &recovery);

    /* Bind IPC manager to runtime */
    ozayn_runtime_set_ipc_mgr(rt, &ipc_mgr);

    /* Initialize service registry */
    ozayn_registry_manager_t reg_mgr;
    if (ozayn_registry_init(&reg_mgr, cfg.values.registry_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize service registry");
        /* Non-fatal: continue without registry */
    }

    /* Bind engine pointers to registry */
    ozayn_registry_set_events(&reg_mgr, &events);
    ozayn_registry_set_recovery(&reg_mgr, &recovery);

    /* Bind registry to runtime */
    ozayn_runtime_set_registry_mgr(rt, &reg_mgr);

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

    /* --- Command Engine demonstration --- */

    /* 1. STATUS command */
    LOG_INFO("COMMANDS", "--- Demonstration: STATUS command ---");
    ozayn_command_t cmd_status = ozayn_command_create(OZAYN_CMD_STATUS, OZAYN_CMD_SRC_CORE);
    ozayn_command_result_t r_status = ozayn_command_engine_execute(&cmd_engine, &cmd_status);
    LOG_INFO("COMMANDS", "STATUS result: %s (status=%s)",
             ozayn_command_result_name(r_status),
             ozayn_command_status_name(cmd_status.status));

    /* 2. Unknown command — should be rejected */
    LOG_INFO("COMMANDS", "--- Demonstration: Unknown command ---");
    ozayn_command_t cmd_unknown = ozayn_command_create((ozayn_command_type_t)99, OZAYN_CMD_SRC_CLI);
    ozayn_command_result_t r_unknown = ozayn_command_engine_execute(&cmd_engine, &cmd_unknown);
    LOG_INFO("COMMANDS", "Unknown result: %s (status=%s)",
             ozayn_command_result_name(r_unknown),
             ozayn_command_status_name(cmd_unknown.status));

    /* 3. NONE command — should be rejected */
    LOG_INFO("COMMANDS", "--- Demonstration: NONE command ---");
    ozayn_command_t cmd_none = ozayn_command_create(OZAYN_CMD_NONE, OZAYN_CMD_SRC_CORE);
    ozayn_command_result_t r_none = ozayn_command_engine_execute(&cmd_engine, &cmd_none);
    LOG_INFO("COMMANDS", "NONE result: %s (status=%s)",
             ozayn_command_result_name(r_none),
             ozayn_command_status_name(cmd_none.status));

    /* 4. STOP command — demonstrates how STOP works (does not execute now) */
    LOG_INFO("COMMANDS", "--- Demonstration: STOP command ---");
    LOG_INFO("COMMANDS", "STOP command registered — triggered by SIGINT/SIGTERM");

    /* --- End Command Engine demonstration --- */

    /* --- Task Manager demonstration --- */

    /* Process events from command demonstrations first */
    ozayn_events_process(&events);

    /* 1. Successful task — DEMO with progress */
    LOG_INFO("TASKS", "--- Demonstration: DEMO task (success) ---");
    ozayn_task_t *task1 = ozayn_task_manager_submit(&task_mgr,
                                                     OZAYN_TASK_DEMO,
                                                     OZAYN_TASK_SRC_CORE);
    if (task1) {
        LOG_INFO("TASKS", "Task #%u state: %s (progress=%d%%)",
                 task1->id,
                 ozayn_task_state_name(task1->state),
                 task1->progress);
    }

    /* 2. Failing task — DEMO_FAIL */
    LOG_INFO("TASKS", "--- Demonstration: DEMO_FAIL task (failure) ---");
    ozayn_task_t *task2 = ozayn_task_manager_submit(&task_mgr,
                                                     OZAYN_TASK_DEMO_FAIL,
                                                     OZAYN_TASK_SRC_COMMAND);
    if (task2) {
        LOG_INFO("TASKS", "Task #%u state: %s",
                 task2->id,
                 ozayn_task_state_name(task2->state));
    }

    /* 3. Unknown task type — should be rejected */
    LOG_INFO("TASKS", "--- Demonstration: Unknown task type ---");
    ozayn_task_t *task3 = ozayn_task_manager_submit(&task_mgr,
                                                     (ozayn_task_type_t)99,
                                                     OZAYN_TASK_SRC_CORE);
    LOG_INFO("TASKS", "Unknown task: %s",
             task3 ? "created (unexpected)" : "rejected (expected)");

    /* 4. Query active task count */
    LOG_INFO("TASKS", "--- Demonstration: Task query ---");
    int active = ozayn_task_manager_active_count(&task_mgr);
    LOG_INFO("TASKS", "Active tasks: %d, total created: %d",
             active, (int)task_mgr.next_id - 1);

    /* --- End Task Manager demonstration --- */

    /* --- Process Manager demonstration --- */

    /* Process task events first */
    ozayn_events_process(&events);

    /* 1. Valid process — echo hello */
    LOG_INFO("PROCESSES", "--- Demonstration: Valid process (echo) ---");
    char *echo_argv[] = { "echo", "Hello from OZAYN process!", NULL };
    ozayn_process_t *proc1 = ozayn_process_manager_create(&proc_mgr,
                                                            "/usr/bin/echo",
                                                            echo_argv);
    if (proc1) {
        LOG_INFO("PROCESSES", "Process #%u: PID %d, state=%s",
                 proc1->id, (int)proc1->pid,
                 ozayn_process_state_name(proc1->state));
    }

    /* 2. Invalid executable — should fail */
    LOG_INFO("PROCESSES", "--- Demonstration: Invalid executable ---");
    char *bad_argv[] = { "/nonexistent/program", NULL };
    ozayn_process_t *proc2 = ozayn_process_manager_create(&proc_mgr,
                                                            "/nonexistent/program",
                                                            bad_argv);
    if (proc2) {
        LOG_INFO("PROCESSES", "Process #%u: PID %d, state=%s",
                 proc2->id, (int)proc2->pid,
                 ozayn_process_state_name(proc2->state));
    }

    /* 3. Long-running process — terminate it */
    LOG_INFO("PROCESSES", "--- Demonstration: Terminate process (sleep) ---");
    char *sleep_argv[] = { "sleep", "300", NULL };
    ozayn_process_t *proc3 = ozayn_process_manager_create(&proc_mgr,
                                                            "/usr/bin/sleep",
                                                            sleep_argv);
    if (proc3) {
        LOG_INFO("PROCESSES", "Process #%u: PID %d, state=%s",
                 proc3->id, (int)proc3->pid,
                 ozayn_process_state_name(proc3->state));

        /* Terminate the sleep process */
        ozayn_process_manager_terminate(&proc_mgr, proc3->id);
        LOG_INFO("PROCESSES", "Process #%u after terminate: state=%s, signal=%d",
                 proc3->id,
                 ozayn_process_state_name(proc3->state),
                 proc3->exit_signal);
    }

    /* Reap echo process (may have exited by now) */
    ozayn_process_manager_reap(&proc_mgr);
    if (proc1) {
        LOG_INFO("PROCESSES", "Process #%u final: state=%s, exit_code=%d",
                 proc1->id,
                 ozayn_process_state_name(proc1->state),
                 proc1->exit_code);
    }

    /* 4. Query active process count */
    LOG_INFO("PROCESSES", "--- Demonstration: Process query ---");
    int active_procs = ozayn_process_manager_active_count(&proc_mgr);
    LOG_INFO("PROCESSES", "Active processes: %d, total created: %d",
             active_procs, (int)proc_mgr.next_id - 1);

    /* --- End Process Manager demonstration --- */

    /* --- Module Manager demonstration --- */

    /* Process process events first */
    ozayn_events_process(&events);

    /* 1. Register logger_test module */
    LOG_INFO("MODULES", "--- Demonstration: Register logger_test module ---");
    ozayn_result_t reg1 = ozayn_module_manager_register(&mod_mgr, &logger_test_module);
    LOG_INFO("MODULES", "Register logger_test: %s",
             reg1 == OZAYN_OK ? "OK" : "FAILED");

    /* 2. Register failure_test module */
    LOG_INFO("MODULES", "--- Demonstration: Register failure_test module ---");
    ozayn_result_t reg2 = ozayn_module_manager_register(&mod_mgr, &failure_test_module);
    LOG_INFO("MODULES", "Register failure_test: %s",
             reg2 == OZAYN_OK ? "OK" : "FAILED");

    /* 3. Duplicate registration — should be rejected */
    LOG_INFO("MODULES", "--- Demonstration: Duplicate registration ---");
    ozayn_result_t reg3 = ozayn_module_manager_register(&mod_mgr, &logger_test_module);
    LOG_INFO("MODULES", "Duplicate logger_test: %s",
             reg3 == OZAYN_ERR_STATE ? "rejected (expected)" : "accepted (unexpected)");

    /* 4. Init all modules — logger_test succeeds, failure_test fails */
    LOG_INFO("MODULES", "--- Demonstration: Init all modules ---");
    ozayn_module_manager_init_all(&mod_mgr);

    /* 5. Start all initialized modules */
    LOG_INFO("MODULES", "--- Demonstration: Start all modules ---");
    ozayn_module_manager_start_all(&mod_mgr);

    /* 6. Query active count */
    LOG_INFO("MODULES", "--- Demonstration: Module query ---");
    int active_mods = ozayn_module_manager_active_count(&mod_mgr);
    int total_mods = ozayn_module_manager_count(&mod_mgr);
    LOG_INFO("MODULES", "Active modules: %d, total registered: %d", active_mods, total_mods);

    /* 7. Find a module by name */
    const ozayn_module_record_t *found = ozayn_module_manager_find(&mod_mgr, "logger_test");
    if (found) {
        LOG_INFO("MODULES", "Found module '%s' v%s — state: %s",
                 found->entry.name,
                 found->entry.version,
                 ozayn_module_state_name(found->state));
    }

    /* 8. Unregister the failed module */
    LOG_INFO("MODULES", "--- Demonstration: Unregister failure_test ---");
    ozayn_module_manager_unregister(&mod_mgr, "failure_test");
    LOG_INFO("MODULES", "Module count after unregister: %d",
             ozayn_module_manager_count(&mod_mgr));

    /* --- End Module Manager demonstration --- */

    /* --- Plugin Manager demonstration --- */

    /* Process module events first */
    ozayn_events_process(&events);

    /* 1. Discover plugins */
    LOG_INFO("PLUGINS", "--- Demonstration: Discover plugins ---");
    const char *pdir = cfg.values.plugin_dir[0] ? cfg.values.plugin_dir : "plugins";
    int discovered = ozayn_plugin_manager_discover(&plug_mgr, pdir);
    LOG_INFO("PLUGINS", "Discovered %d plugin(s)", discovered);

    /* 2. Load valid test_plugin */
    LOG_INFO("PLUGINS", "--- Demonstration: Load test_plugin ---");
    char test_path[512];
    snprintf(test_path, sizeof(test_path), "%s/test_plugin.so", pdir);
    ozayn_result_t load1 = ozayn_plugin_manager_load(&plug_mgr, test_path);
    LOG_INFO("PLUGINS", "Load test_plugin: %s",
             load1 == OZAYN_OK ? "OK" : "FAILED");

    /* 3. Load fail_init_plugin */
    LOG_INFO("PLUGINS", "--- Demonstration: Load fail_init_plugin ---");
    char fail_path[512];
    snprintf(fail_path, sizeof(fail_path), "%s/fail_init_plugin.so", pdir);
    ozayn_result_t load2 = ozayn_plugin_manager_load(&plug_mgr, fail_path);
    LOG_INFO("PLUGINS", "Load fail_init_plugin: %s",
             load2 == OZAYN_OK ? "OK" : "FAILED");

    /* 4. Load bad_api_plugin — should be rejected (API 99 vs 1) */
    LOG_INFO("PLUGINS", "--- Demonstration: Load bad_api_plugin (incompatible) ---");
    char bad_path[512];
    snprintf(bad_path, sizeof(bad_path), "%s/bad_api_plugin.so", pdir);
    ozayn_result_t load3 = ozayn_plugin_manager_load(&plug_mgr, bad_path);
    LOG_INFO("PLUGINS", "Load bad_api_plugin: %s",
             load3 == OZAYN_ERR ? "rejected (expected)" : "accepted (unexpected)");

    /* 5. Load no_entry.so — should fail (missing entry symbol) */
    LOG_INFO("PLUGINS", "--- Demonstration: Load no_entry (missing symbol) ---");
    char noentry_path[512];
    snprintf(noentry_path, sizeof(noentry_path), "%s/no_entry.so", pdir);
    ozayn_result_t load4 = ozayn_plugin_manager_load(&plug_mgr, noentry_path);
    LOG_INFO("PLUGINS", "Load no_entry: %s",
             load4 == OZAYN_ERR ? "rejected (expected)" : "accepted (unexpected)");

    /* 6. Load duplicate_test_plugin — should be rejected (same ID as test_plugin) */
    LOG_INFO("PLUGINS", "--- Demonstration: Load duplicate_test_plugin ---");
    char dup_path[512];
    snprintf(dup_path, sizeof(dup_path), "%s/duplicate_test_plugin.so", pdir);
    ozayn_result_t load5 = ozayn_plugin_manager_load(&plug_mgr, dup_path);
    LOG_INFO("PLUGINS", "Load duplicate_test_plugin: %s",
             (load5 == OZAYN_ERR || load5 == OZAYN_ERR_STATE) ? "rejected (expected)" : "accepted (unexpected)");

    /* 7. Initialize all loaded plugins */
    LOG_INFO("PLUGINS", "--- Demonstration: Init all plugins ---");
    ozayn_plugin_manager_init_all(&plug_mgr);

    /* 8. Start all initialized plugins */
    LOG_INFO("PLUGINS", "--- Demonstration: Start all plugins ---");
    ozayn_plugin_manager_start_all(&plug_mgr);

    /* 9. Query */
    LOG_INFO("PLUGINS", "--- Demonstration: Plugin query ---");
    int active_plugs = ozayn_plugin_manager_active_count(&plug_mgr);
    int total_plugs = ozayn_plugin_manager_count(&plug_mgr);
    LOG_INFO("PLUGINS", "Active plugins: %d, total registered: %d", active_plugs, total_plugs);

    /* 10. Find by ID */
    const ozayn_plugin_record_t *found_plug = ozayn_plugin_manager_find(&plug_mgr, "test_plugin");
    if (found_plug) {
        LOG_INFO("PLUGINS", "Found plugin '%s' v%s — state: %s (author=%s)",
                 found_plug->id,
                 found_plug->version,
                 ozayn_plugin_state_name(found_plug->state),
                 found_plug->info.author ? found_plug->info.author : "unknown");
    }

    /* 11. Unload test_plugin */
    LOG_INFO("PLUGINS", "--- Demonstration: Unload test_plugin ---");
    ozayn_plugin_manager_unload(&plug_mgr, "test_plugin");
    LOG_INFO("PLUGINS", "Active after unload: %d",
             ozayn_plugin_manager_active_count(&plug_mgr));

    /* --- End Plugin Manager demonstration --- */

    /* --- IPC Manager demonstration --- */

    /* Process plugin events first */
    ozayn_events_process(&events);

    /* 1. Query IPC state */
    LOG_INFO("IPC", "--- Demonstration: IPC manager state ---");
    LOG_INFO("IPC", "IPC enabled: %s", ozayn_ipc_manager_is_enabled(&ipc_mgr) ? "yes" : "no");
    LOG_INFO("IPC", "IPC state: %s", ozayn_ipc_state_name(ipc_mgr.state));
    LOG_INFO("IPC", "IPC connections: %d", ozayn_ipc_manager_connection_count(&ipc_mgr));

    /* 2. Message type names */
    LOG_INFO("IPC", "--- Demonstration: Message type names ---");
    LOG_INFO("IPC", "HELLO = %s", ozayn_ipc_msg_type_name(OZAYN_IPC_MSG_HELLO));
    LOG_INFO("IPC", "REQUEST = %s", ozayn_ipc_msg_type_name(OZAYN_IPC_MSG_REQUEST));
    LOG_INFO("IPC", "RESPONSE = %s", ozayn_ipc_msg_type_name(OZAYN_IPC_MSG_RESPONSE));

    /* 3. Component type names */
    LOG_INFO("IPC", "--- Demonstration: Component type names ---");
    LOG_INFO("IPC", "CORE = %s", ozayn_ipc_component_type_name(OZAYN_IPC_COMP_CORE));
    LOG_INFO("IPC", "WORKER = %s", ozayn_ipc_component_type_name(OZAYN_IPC_COMP_WORKER));

    /* 4. Header pack/unpack roundtrip */
    LOG_INFO("IPC", "--- Demonstration: Header roundtrip ---");
    ozayn_ipc_header_t test_hdr = {
        .magic = OZAYN_IPC_MAGIC,
        .version = OZAYN_IPC_VERSION,
        .type = OZAYN_IPC_MSG_REQUEST,
        .id = 42,
        .length = 100,
    };
    uint8_t hdr_buf[OZAYN_IPC_HEADER_SIZE];
    ozayn_ipc_header_pack(&test_hdr, hdr_buf, sizeof(hdr_buf));
    ozayn_ipc_header_t unpacked;
    ozayn_ipc_header_unpack(&unpacked, hdr_buf, sizeof(hdr_buf));
    LOG_INFO("IPC", "Pack/unpack: magic=0x%04X ver=%d type=%d id=%u len=%u",
             unpacked.magic, unpacked.version, unpacked.type,
             unpacked.id, unpacked.length);
    LOG_INFO("IPC", "Header roundtrip: %s",
             (unpacked.magic == OZAYN_IPC_MAGIC &&
              unpacked.version == OZAYN_IPC_VERSION &&
              unpacked.type == OZAYN_IPC_MSG_REQUEST &&
              unpacked.id == 42 &&
              unpacked.length == 100) ? "OK" : "FAIL");

    /* 5. Publish IPC events */
    LOG_INFO("IPC", "--- Demonstration: IPC events ---");
    ozayn_events_publish(&events, OZAYN_EVENT_IPC_STARTED, OZAYN_SRC_IPC, NULL);
    ozayn_events_process(&events);

    LOG_INFO("IPC", "IPC server ready — waiting for client connections...");
    LOG_INFO("IPC", "Endpoint: %s", ipc_mgr.endpoint);

    /* --- End IPC Manager demonstration --- */

    /* --- Service Registry demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query registry state */
    LOG_INFO("REGISTRY", "--- Demonstration: Registry state ---");
    LOG_INFO("REGISTRY", "Registry enabled: %s", ozayn_registry_is_enabled(&reg_mgr) ? "yes" : "no");
    LOG_INFO("REGISTRY", "Service count: %d", ozayn_registry_count(&reg_mgr));

    /* 2. Register a test service: ozayn.core */
    LOG_INFO("REGISTRY", "--- Demonstration: Register ozayn.core ---");
    ozayn_service_registration_t core_reg;
    memset(&core_reg, 0, sizeof(core_reg));
    snprintf(core_reg.id, sizeof(core_reg.id), "ozayn.core");
    snprintf(core_reg.name, sizeof(core_reg.name), "OZAYN Core");
    snprintf(core_reg.version, sizeof(core_reg.version), "0.1");
    core_reg.protocol_version = OZAYN_IPC_VERSION;
    snprintf(core_reg.endpoint, sizeof(core_reg.endpoint), "runtime/ipc/ozayn.sock");
    snprintf(core_reg.provider, sizeof(core_reg.provider), "core");
    snprintf(core_reg.capabilities[0], sizeof(core_reg.capabilities[0]), "system-management");
    snprintf(core_reg.capabilities[1], sizeof(core_reg.capabilities[1]), "service-discovery");
    core_reg.capability_count = 2;

    ozayn_result_t reg_r = ozayn_registry_register(&reg_mgr, &core_reg, -1);
    LOG_INFO("REGISTRY", "Register ozayn.core: %s", reg_r == OZAYN_OK ? "OK" : "FAILED");

    /* 3. Register another service: ozayn.test */
    LOG_INFO("REGISTRY", "--- Demonstration: Register ozayn.test ---");
    ozayn_service_registration_t test_reg;
    memset(&test_reg, 0, sizeof(test_reg));
    snprintf(test_reg.id, sizeof(test_reg.id), "ozayn.test");
    snprintf(test_reg.name, sizeof(test_reg.name), "Test Service");
    snprintf(test_reg.version, sizeof(test_reg.version), "1.0.0");
    test_reg.protocol_version = OZAYN_IPC_VERSION;
    snprintf(test_reg.endpoint, sizeof(test_reg.endpoint), "runtime/ipc/test.sock");
    snprintf(test_reg.provider, sizeof(test_reg.provider), "test-module");
    snprintf(test_reg.capabilities[0], sizeof(test_reg.capabilities[0]), "test.echo");
    snprintf(test_reg.capabilities[1], sizeof(test_reg.capabilities[1]), "test.status");
    test_reg.capability_count = 2;

    reg_r = ozayn_registry_register(&reg_mgr, &test_reg, -1);
    LOG_INFO("REGISTRY", "Register ozayn.test: %s", reg_r == OZAYN_OK ? "OK" : "FAILED");

    /* 4. Duplicate registration — should be rejected */
    LOG_INFO("REGISTRY", "--- Demonstration: Duplicate registration ---");
    reg_r = ozayn_registry_register(&reg_mgr, &test_reg, -1);
    LOG_INFO("REGISTRY", "Duplicate ozayn.test: %s",
             reg_r == OZAYN_ERR_STATE ? "rejected (expected)" : "accepted (unexpected)");

    /* 5. Invalid registration — missing endpoint */
    LOG_INFO("REGISTRY", "--- Demonstration: Invalid registration (no endpoint) ---");
    ozayn_service_registration_t bad_reg;
    memset(&bad_reg, 0, sizeof(bad_reg));
    snprintf(bad_reg.id, sizeof(bad_reg.id), "ozayn.bad");
    bad_reg.protocol_version = OZAYN_IPC_VERSION;
    /* endpoint intentionally left empty */
    reg_r = ozayn_registry_register(&reg_mgr, &bad_reg, -1);
    LOG_INFO("REGISTRY", "Register ozayn.bad (no endpoint): %s",
             reg_r == OZAYN_ERR ? "rejected (expected)" : "accepted (unexpected)");

    /* 6. Incompatible protocol — should be rejected */
    LOG_INFO("REGISTRY", "--- Demonstration: Incompatible protocol ---");
    ozayn_service_registration_t proto_reg;
    memset(&proto_reg, 0, sizeof(proto_reg));
    snprintf(proto_reg.id, sizeof(proto_reg.id), "ozayn.proto");
    snprintf(proto_reg.endpoint, sizeof(proto_reg.endpoint), "/tmp/proto.sock");
    proto_reg.protocol_version = 99; /* incompatible */
    reg_r = ozayn_registry_register(&reg_mgr, &proto_reg, -1);
    LOG_INFO("REGISTRY", "Register ozayn.proto (protocol 99): %s",
             reg_r == OZAYN_ERR ? "rejected (expected)" : "accepted (unexpected)");

    /* 7. Lookup */
    LOG_INFO("REGISTRY", "--- Demonstration: Lookup ---");
    const ozayn_service_record_t *found_svc = ozayn_registry_lookup(&reg_mgr, "ozayn.test");
    if (found_svc) {
        LOG_INFO("REGISTRY", "Found '%s' v%s state=%s endpoint=%s",
                 found_svc->id, found_svc->version,
                 ozayn_service_state_name(found_svc->state),
                 found_svc->endpoint);
    }

    const ozayn_service_record_t *not_found = ozayn_registry_lookup(&reg_mgr, "nonexistent");
    LOG_INFO("REGISTRY", "Lookup 'nonexistent': %s",
             not_found ? "found (unexpected)" : "NOT FOUND (expected)");

    /* 8. Find by capability */
    LOG_INFO("REGISTRY", "--- Demonstration: Find by capability ---");
    const ozayn_service_record_t *cap_svc = ozayn_registry_find_by_capability(&reg_mgr, "test.echo");
    if (cap_svc) {
        LOG_INFO("REGISTRY", "Capability 'test.echo' provided by: '%s'", cap_svc->id);
    }

    /* 9. List all services */
    LOG_INFO("REGISTRY", "--- Demonstration: List all services ---");
    const ozayn_service_record_t *svc_list[OZAYN_REGISTRY_MAX_SERVICES];
    int svc_count = ozayn_registry_list(&reg_mgr, svc_list, OZAYN_REGISTRY_MAX_SERVICES);
    LOG_INFO("REGISTRY", "Total services: %d", svc_count);
    for (int i = 0; i < svc_count; i++) {
        LOG_INFO("REGISTRY", "  [%d] '%s' v%s state=%s caps=%d",
                 i + 1, svc_list[i]->id, svc_list[i]->version,
                 ozayn_service_state_name(svc_list[i]->state),
                 svc_list[i]->capability_count);
    }

    /* 10. SERVICE LIST command */
    LOG_INFO("REGISTRY", "--- Demonstration: SERVICE LIST command ---");
    ozayn_command_t cmd_svc_list = ozayn_command_create(OZAYN_CMD_SERVICE_LIST, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_svc_list);

    /* 11. SERVICE STATUS command */
    LOG_INFO("REGISTRY", "--- Demonstration: SERVICE STATUS command ---");
    ozayn_command_t cmd_svc_status = ozayn_command_create(OZAYN_CMD_SERVICE_STATUS, OZAYN_CMD_SRC_CLI);
    const char *status_id = "ozayn.core";
    cmd_svc_status.payload = status_id;
    cmd_svc_status.payload_size = strlen(status_id);
    ozayn_command_engine_execute(&cmd_engine, &cmd_svc_status);

    /* 12. Update state */
    LOG_INFO("REGISTRY", "--- Demonstration: Update state ---");
    ozayn_registry_update_state(&reg_mgr, "ozayn.test", OZAYN_SVC_DEGRADED);
    ozayn_registry_update_state(&reg_mgr, "ozayn.test", OZAYN_SVC_READY);

    /* 13. Process registry events */
    ozayn_events_process(&events);

    LOG_INFO("REGISTRY", "Service registry ready — accepting registrations via IPC...");

    /* --- End Service Registry demonstration --- */

    /* Runtime runs until stopped (STOP command set should_stop) */
    ozayn_runtime_set_stop_flag(rt, &g_stop);
    ozayn_runtime_run(rt);

    /* Publish shutdown events */
    ozayn_events_publish(&events, OZAYN_EVENT_RUNTIME_STOPPING, OZAYN_SRC_RUNTIME, NULL);
    ozayn_events_process(&events);

    LOG_INFO("RUNTIME", "Runtime shutting down");
    ozayn_runtime_shutdown(rt);
    ozayn_runtime_destroy(rt);

    LOG_INFO("CORE", "OZAYN Core shutdown complete");
    ozayn_registry_shutdown(&reg_mgr);
    ozayn_ipc_manager_shutdown(&ipc_mgr);
    ozayn_plugin_manager_shutdown(&plug_mgr);
    ozayn_module_manager_shutdown(&mod_mgr);
    ozayn_process_manager_shutdown(&proc_mgr);
    ozayn_task_manager_shutdown(&task_mgr);
    ozayn_command_engine_shutdown(&cmd_engine);
    ozayn_events_shutdown(&events);
    ozayn_logger_shutdown(&logger);
    ozayn_config_destroy(&cfg);

    return 0;
}
