#include "ozayn.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>

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

    /* Initialize security manager */
    ozayn_security_manager_t sec_mgr;
    if (ozayn_security_init(&sec_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize security manager");
        /* Non-fatal: continue without security */
    }

    /* Bind events, recovery to security */
    ozayn_security_set_events(&sec_mgr, &events);
    ozayn_security_set_recovery(&sec_mgr, &recovery);

    /* Configure auth mode from config */
    ozayn_security_set_auth_mode(&sec_mgr, (ozayn_auth_method_t)cfg.values.security_auth_mode);
    ozayn_security_set_audit_logging(&sec_mgr, cfg.values.security_audit_log);

    /* Add current user's UID to allowed list */
    uid_t current_uid = getuid();
    ozayn_security_set_allowed_uid(&sec_mgr, (uint32_t)current_uid);

    /* Register test identities for development */
    ozayn_security_register_identity(&sec_mgr, "ozayn.ai",
                                     "OZAYN AI Service",
                                     OZAYN_IDENTITY_SERVICE, OZAYN_AUTH_UID,
                                     (uint32_t)current_uid, 0);
    ozayn_security_register_identity(&sec_mgr, "ozayn.vision",
                                     "OZAYN Vision Service",
                                     OZAYN_IDENTITY_SERVICE, OZAYN_AUTH_UID,
                                     (uint32_t)current_uid, 0);

    /* Bind security to runtime */
    ozayn_runtime_set_security_mgr(rt, &sec_mgr);

    /* Initialize authorization manager */
    ozayn_authorization_manager_t authz_mgr;
    if (ozayn_authorization_init(&authz_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize authorization manager");
        /* Non-fatal: continue without authorization */
    }

    /* Bind security, events, recovery to authorization */
    ozayn_authorization_set_security(&authz_mgr, &sec_mgr);
    ozayn_authorization_set_events(&authz_mgr, &events);
    ozayn_authorization_set_recovery(&authz_mgr, &recovery);
    ozayn_authorization_set_audit_logging(&authz_mgr, cfg.values.security_audit_log);

    /* Assign roles to test identities */
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.vision", "VISION_SERVICE");
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.ai", "SERVICE_BASE");
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.core", "RESOURCE_ADMIN");

    /* Bind authorization to runtime */
    ozayn_runtime_set_authorization_mgr(rt, &authz_mgr);

    /* Initialize resource manager */
    ozayn_resource_manager_t res_mgr;
    if (ozayn_resource_manager_init(&res_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize resource manager");
        /* Non-fatal: continue without resource manager */
    }

    /* Bind events, recovery, authorization to resource manager */
    ozayn_resource_manager_set_events(&res_mgr, &events);
    ozayn_resource_manager_set_recovery(&res_mgr, &recovery);
    ozayn_resource_manager_set_authorization(&res_mgr, &authz_mgr);

    /* Bind resource manager to runtime */
    ozayn_runtime_set_resource_mgr(rt, &res_mgr);

    /* Initialize scheduler manager */
    ozayn_scheduler_manager_t sched_mgr;
    if (ozayn_scheduler_init(&sched_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize scheduler manager");
        /* Non-fatal: continue without scheduler */
    }

    /* Bind task manager, events, recovery, resource manager to scheduler */
    ozayn_scheduler_set_task_mgr(&sched_mgr, &task_mgr);
    ozayn_scheduler_set_events(&sched_mgr, &events);
    ozayn_scheduler_set_recovery(&sched_mgr, &recovery);
    ozayn_scheduler_set_resource_mgr(&sched_mgr, &res_mgr);

    /* Configure scheduler */
    ozayn_scheduler_set_aging(&sched_mgr, 1);
    ozayn_scheduler_set_max_tasks_per_source(&sched_mgr, 16);

    /* Bind scheduler manager to runtime */
    ozayn_runtime_set_scheduler_mgr(rt, &sched_mgr);

    /* Initialize monitoring manager */
    ozayn_monitoring_manager_t mon_mgr;
    if (ozayn_monitoring_init(&mon_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize monitoring manager");
        /* Non-fatal: continue without monitoring */
    }

    /* Bind events, recovery to monitoring */
    ozayn_monitoring_set_events(&mon_mgr, &events);
    ozayn_monitoring_set_recovery(&mon_mgr, &recovery);

    /* Bind monitoring manager to runtime */
    ozayn_runtime_set_monitoring_mgr(rt, &mon_mgr);

    /* Initialize diagnostics manager */
    ozayn_diagnostics_manager_t diag_mgr;
    if (ozayn_diagnostics_init(&diag_mgr, cfg.values.security_enabled) != OZAYN_OK) {
        LOG_ERROR("CORE", "Failed to initialize diagnostics manager");
        /* Non-fatal: continue without diagnostics */
    }

    /* Bind events, recovery, monitoring to diagnostics */
    ozayn_diagnostics_set_events(&diag_mgr, &events);
    ozayn_diagnostics_set_recovery(&diag_mgr, &recovery);
    ozayn_diagnostics_set_monitoring(&diag_mgr, &mon_mgr);

    /* Bind diagnostics manager to runtime */
    ozayn_runtime_set_diagnostics_mgr(rt, &diag_mgr);

    /* Initialize security boundary manager */
    ozayn_security_boundary_manager_t sec_bnd_mgr;
    if (ozayn_security_boundary_init(&sec_bnd_mgr, cfg.values.security_enabled) != 0) {
        LOG_ERROR("CORE", "Failed to initialize security boundary manager");
    }

    /* Bind events, recovery to security boundary */
    sec_bnd_mgr.events = &events;
    sec_bnd_mgr.recovery = &recovery;

    /* Bind security boundary manager to runtime */
    ozayn_runtime_set_security_boundary_mgr(rt, &sec_bnd_mgr);

    /* Initialize state manager */
    ozayn_state_manager_t state_mgr;
    if (ozayn_state_manager_init(&state_mgr, cfg.values.security_enabled) != 0) {
        LOG_ERROR("CORE", "Failed to initialize state manager");
    }

    /* Set storage path */
    char state_path[512];
    snprintf(state_path, sizeof(state_path), "%s/ozayn.state",
             cfg.values.log_directory[0] ? cfg.values.log_directory : "data");
    ozayn_state_manager_set_storage_path(&state_mgr, state_path);

    /* Bind events, recovery to state manager */
    state_mgr.events = &events;
    state_mgr.recovery = &recovery;

    /* Bind state manager to runtime */
    ozayn_runtime_set_state_mgr(rt, &state_mgr);

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
    ozayn_ipc_manager_set_security(&ipc_mgr, &sec_mgr);
    ozayn_ipc_manager_set_authorization(&ipc_mgr, &authz_mgr);

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

    /* --- Platform Layer demonstration --- */

    ozayn_events_process(&events);

    /* 1. Initialize platform layer */
    LOG_INFO("PLATFORM", "--- Demonstration: Platform initialization ---");
    ozayn_platform_init();

    /* 2. System information */
    LOG_INFO("PLATFORM", "--- Demonstration: System information ---");
    ozayn_system_info_t sys_info;
    ozayn_system_info(&sys_info);
    LOG_INFO("PLATFORM", "OS:       %s %s", sys_info.os_name, sys_info.os_version);
    LOG_INFO("PLATFORM", "Arch:     %s", sys_info.arch);
    LOG_INFO("PLATFORM", "Hostname: %s", sys_info.hostname);
    LOG_INFO("PLATFORM", "Memory:   %llu MB", (unsigned long long)sys_info.total_memory_mb);
    LOG_INFO("PLATFORM", "CPU cores: %u", sys_info.cpu_cores);
    LOG_INFO("PLATFORM", "Uptime:   %llu seconds", (unsigned long long)sys_info.uptime_seconds);

    /* 3. Current process */
    LOG_INFO("PLATFORM", "--- Demonstration: Current process ---");
    uint32_t self_pid = ozayn_process_self();
    LOG_INFO("PLATFORM", "Self PID: %u", self_pid);

    ozayn_process_info_t self_info;
    if (ozayn_process_info(self_pid, &self_info) == OZAYN_OK) {
        LOG_INFO("PLATFORM", "Process name: %s", self_info.name);
        LOG_INFO("PLATFORM", "Executable: %s", self_info.executable);
    }

    /* 4. File system */
    LOG_INFO("PLATFORM", "--- Demonstration: File system ---");
    const char *home = ozayn_fs_home();
    const char *config = ozayn_fs_config_dir();
    LOG_INFO("PLATFORM", "Home dir:     %s", home);
    LOG_INFO("PLATFORM", "Config dir:   %s", config);
    LOG_INFO("PLATFORM", "Home exists:  %s", ozayn_fs_exists(home) ? "yes" : "no");
    LOG_INFO("PLATFORM", "Is directory: %s", ozayn_fs_is_dir(home) ? "yes" : "no");

    /* 5. Display information */
    LOG_INFO("PLATFORM", "--- Demonstration: Display information ---");
    ozayn_display_info_t disp_info;
    ozayn_display_info(&disp_info);
    LOG_INFO("PLATFORM", "Display count: %u", disp_info.count);
    for (uint32_t i = 0; i < disp_info.count && i < 8; i++) {
        LOG_INFO("PLATFORM", "  [%u] %s: %ux%u @ %uHz",
                 i + 1, disp_info.modes[i].name,
                 disp_info.modes[i].width, disp_info.modes[i].height,
                 disp_info.modes[i].refresh_hz);
    }

    /* 6. Network information */
    LOG_INFO("PLATFORM", "--- Demonstration: Network information ---");
    ozayn_network_info_t net_info;
    ozayn_network_info(&net_info);
    LOG_INFO("PLATFORM", "Interfaces: %u", net_info.count);
    for (uint32_t i = 0; i < net_info.count && i < 16; i++) {
        LOG_INFO("PLATFORM", "  [%u] %s: %s (up=%s, loopback=%s)",
                 i + 1, net_info.ifaces[i].name, net_info.ifaces[i].ip,
                 net_info.ifaces[i].is_up ? "yes" : "no",
                 net_info.ifaces[i].is_loopback ? "yes" : "no");
    }

    /* 7. Camera, Audio, Input (stubs) */
    LOG_INFO("PLATFORM", "--- Demonstration: Device stubs ---");
    ozayn_camera_info_t cam_info;
    ozayn_camera_info(&cam_info);
    LOG_INFO("PLATFORM", "Camera available: %s", cam_info.available ? "yes" : "no (stub)");

    ozayn_audio_info_t aud_info;
    ozayn_audio_info(&aud_info);
    LOG_INFO("PLATFORM", "Audio available: %s", aud_info.available ? "yes" : "no (stub)");

    ozayn_input_info_t inp_info;
    ozayn_input_info(&inp_info);
    LOG_INFO("PLATFORM", "Keyboard: %s, Mouse: %s, Touch: %s",
             inp_info.has_keyboard ? "yes" : "no",
             inp_info.has_mouse ? "yes" : "no",
             inp_info.has_touch ? "yes" : "no");

    /* 8. Timestamp */
    LOG_INFO("PLATFORM", "--- Demonstration: Timestamp ---");
    uint64_t now = ozayn_system_time();
    LOG_INFO("PLATFORM", "Current epoch: %llu", (unsigned long long)now);

    /* 9. Shutdown platform */
    ozayn_platform_shutdown();

    /* --- End Platform Layer demonstration --- */

    /* --- Security & Identity Foundation demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query security state */
    LOG_INFO("SECURITY", "--- Demonstration: Security state ---");
    LOG_INFO("SECURITY", "Security enabled: %s",
             ozayn_security_is_enabled(&sec_mgr) ? "yes" : "no");
    LOG_INFO("SECURITY", "Auth mode: %s",
             ozayn_auth_method_name(sec_mgr.auth_mode));
    LOG_INFO("SECURITY", "Identities registered: %d",
             ozayn_security_identity_count(&sec_mgr));

    /* 2. List all registered identities */
    LOG_INFO("SECURITY", "--- Demonstration: Registered identities ---");
    ozayn_command_t cmd_id_list = ozayn_command_create(OZAYN_CMD_IDENTITY_LIST, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_id_list);

    /* 3. Authentication — valid identity with valid UID */
    LOG_INFO("SECURITY", "--- Demonstration: Authentication (valid identity) ---");
    ozayn_peer_creds_t valid_creds = {
        .uid = (uint32_t)current_uid,
        .gid = (uint32_t)getgid(),
        .pid = (uint32_t)getpid(),
        .valid = 1,
    };
    ozayn_auth_result_t auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.core", &valid_creds);
    LOG_INFO("SECURITY", "Auth ozayn.core: %s", ozayn_auth_result_name(auth_r));

    /* 4. Authentication — valid identity, wrong UID */
    LOG_INFO("SECURITY", "--- Demonstration: Authentication (UID mismatch) ---");
    ozayn_peer_creds_t wrong_creds = {
        .uid = 9999, /* wrong UID */
        .gid = 0,
        .pid = 1234,
        .valid = 1,
    };
    auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.vision", &wrong_creds);
    LOG_INFO("SECURITY", "Auth ozayn.vision (wrong UID): %s", ozayn_auth_result_name(auth_r));

    /* 5. Authentication — unknown identity */
    LOG_INFO("SECURITY", "--- Demonstration: Authentication (unknown identity) ---");
    auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.malicious", &valid_creds);
    LOG_INFO("SECURITY", "Auth ozayn.malicious: %s", ozayn_auth_result_name(auth_r));

    /* 6. Authentication — no credentials */
    LOG_INFO("SECURITY", "--- Demonstration: Authentication (no credentials) ---");
    ozayn_peer_creds_t no_creds = { .valid = 0 };
    auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.ai", &no_creds);
    LOG_INFO("SECURITY", "Auth ozayn.ai (no creds): %s", ozayn_auth_result_name(auth_r));

    /* 7. Trust queries */
    LOG_INFO("SECURITY", "--- Demonstration: Trust queries ---");
    LOG_INFO("SECURITY", "ozayn.core trusted: %s",
             ozayn_security_is_trusted(&sec_mgr, "ozayn.core") ? "yes" : "no");
    LOG_INFO("SECURITY", "ozayn.ai trusted: %s",
             ozayn_security_is_trusted(&sec_mgr, "ozayn.ai") ? "yes" : "no");
    LOG_INFO("SECURITY", "ozayn.unknown trusted: %s",
             ozayn_security_is_trusted(&sec_mgr, "ozayn.unknown") ? "yes" : "no (deny by default)");

    /* 8. Identity revocation */
    LOG_INFO("SECURITY", "--- Demonstration: Identity revocation ---");
    ozayn_security_revoke_identity(&sec_mgr, "ozayn.vision");
    LOG_INFO("SECURITY", "ozayn.vision trust after revoke: %s",
             ozayn_trust_state_name(ozayn_security_get_trust_state(&sec_mgr, "ozayn.vision")));
    auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.vision", &valid_creds);
    LOG_INFO("SECURITY", "Auth ozayn.vision after revoke: %s", ozayn_auth_result_name(auth_r));

    /* 9. AUTH STATUS command */
    LOG_INFO("SECURITY", "--- Demonstration: AUTH STATUS command ---");
    ozayn_command_t cmd_auth = ozayn_command_create(OZAYN_CMD_AUTH_STATUS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_auth);

    /* 10. Process security events */
    ozayn_events_process(&events);

    /* --- End Security & Identity Foundation demonstration --- */

    /* Re-register ozayn.vision (revoked in security demo) for authorization demos */
    ozayn_security_remove_identity(&sec_mgr, "ozayn.vision");
    ozayn_security_register_identity(&sec_mgr, "ozayn.vision",
                                     "OZAYN Vision Service",
                                     OZAYN_IDENTITY_SERVICE, OZAYN_AUTH_UID,
                                     (uint32_t)current_uid, 0);

    /* --- Permission & Authorization Engine demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query authorization state */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Authorization state ---");
    LOG_INFO("AUTHORIZATION", "Authorization enabled: %s",
             ozayn_authorization_is_enabled(&authz_mgr) ? "yes" : "no");
    LOG_INFO("AUTHORIZATION", "Permissions registered: %d",
             ozayn_authorization_permission_count(&authz_mgr));
    LOG_INFO("AUTHORIZATION", "Roles registered: %d",
             ozayn_authorization_role_count(&authz_mgr));

    /* 2. List all roles */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Registered roles ---");
    ozayn_command_t cmd_role_list = ozayn_command_create(OZAYN_CMD_ROLE_LIST, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_role_list);

    /* 3. Authorization — ozayn.vision + camera.read (should ALLOW) */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Authorization (allowed) ---");
    ozayn_authz_result_t az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "camera", "read");
    LOG_INFO("AUTHORIZATION", "ozayn.vision camera.read -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 4. Authorization — ozayn.vision + process.stop (should DENY) */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Authorization (denied) ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "process", "stop");
    LOG_INFO("AUTHORIZATION", "ozayn.vision process.stop -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 5. Authorization — unknown permission (should DENY) */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Authorization (unknown permission) ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "hack", "system");
    LOG_INFO("AUTHORIZATION", "ozayn.vision hack.system -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 6. Authorization — unauthenticated identity (should DENY) */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Authorization (untrusted identity) ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.malicious", "camera", "read");
    LOG_INFO("AUTHORIZATION", "ozayn.malicious camera.read -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 7. Cross-service isolation — vision can't access microphone */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Cross-service isolation ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "microphone", "read");
    LOG_INFO("AUTHORIZATION", "ozayn.vision microphone.read -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 8. Core protection — low-privilege can't shutdown */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Core protection ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "core", "shutdown");
    LOG_INFO("AUTHORIZATION", "ozayn.vision core.shutdown -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 9. Core identity CAN shutdown (has CORE_ADMIN) */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Core identity allowed ---");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.core", "core", "shutdown");
    LOG_INFO("AUTHORIZATION", "ozayn.core core.shutdown -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 10. Permission CHECK command */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: PERMISSION CHECK command ---");
    ozayn_command_t cmd_perm_check = ozayn_command_create(OZAYN_CMD_PERMISSION_CHECK, OZAYN_CMD_SRC_CLI);
    const char *check_args = "ozayn.vision camera read";
    cmd_perm_check.payload = check_args;
    cmd_perm_check.payload_size = strlen(check_args);
    ozayn_command_engine_execute(&cmd_engine, &cmd_perm_check);

    /* 11. Role revocation — remove VISION_SERVICE from ozayn.vision */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Role revocation ---");
    ozayn_authorization_revoke_role(&authz_mgr, "ozayn.vision", "VISION_SERVICE");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "camera", "read");
    LOG_INFO("AUTHORIZATION", "ozayn.vision camera.read after revoke -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 12. Re-assign role and verify */
    LOG_INFO("AUTHORIZATION", "--- Demonstration: Re-assign role ---");
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.vision", "VISION_SERVICE");
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "camera", "read");
    LOG_INFO("AUTHORIZATION", "ozayn.vision camera.read after reassign -> %s (reason=%s)",
             ozayn_authz_decision_name(az_r.decision),
             ozayn_deny_reason_name(az_r.reason));

    /* 13. Process authorization events */
    ozayn_events_process(&events);

    /* --- End Permission & Authorization Engine demonstration --- */

    /* --- Resource Manager demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query resource manager state */
    LOG_INFO("RESOURCE", "--- Demonstration: Resource manager state ---");
    LOG_INFO("RESOURCE", "Resource manager enabled: %s",
             res_mgr.enabled ? "yes" : "no");
    LOG_INFO("RESOURCE", "Resources registered: %d",
             ozayn_resource_manager_count(&res_mgr));

    /* 2. Create resources */
    LOG_INFO("RESOURCE", "--- Demonstration: Create resources ---");
    ozayn_resource_result_t rr;

    rr = ozayn_resource_create(&res_mgr, "camera-01", "Primary Camera",
                               OZAYN_RESOURCE_TYPE_DEVICE, 1);
    LOG_INFO("RESOURCE", "Create camera-01: %s", ozayn_resource_result_name(rr));

    rr = ozayn_resource_create(&res_mgr, "ipc-conn-1", "IPC Connection 1",
                               OZAYN_RESOURCE_TYPE_IPC, 0);
    LOG_INFO("RESOURCE", "Create ipc-conn-1: %s", ozayn_resource_result_name(rr));

    rr = ozayn_resource_create(&res_mgr, "buffer-01", "Frame Buffer",
                               OZAYN_RESOURCE_TYPE_BUFFER, 0);
    LOG_INFO("RESOURCE", "Create buffer-01: %s", ozayn_resource_result_name(rr));

    rr = ozayn_resource_create(&res_mgr, "module-instance-1", "Vision Module",
                               OZAYN_RESOURCE_TYPE_MODULE, 1);
    LOG_INFO("RESOURCE", "Create module-instance-1: %s", ozayn_resource_result_name(rr));

    /* 3. Duplicate creation — should be rejected */
    LOG_INFO("RESOURCE", "--- Demonstration: Duplicate creation ---");
    rr = ozayn_resource_create(&res_mgr, "camera-01", "Duplicate Camera",
                               OZAYN_RESOURCE_TYPE_DEVICE, 1);
    LOG_INFO("RESOURCE", "Duplicate camera-01: %s (expected: ERROR)",
             ozayn_resource_result_name(rr));

    /* 4. Query resource count by type */
    LOG_INFO("RESOURCE", "--- Demonstration: Query by type ---");
    LOG_INFO("RESOURCE", "Device resources: %d",
             ozayn_resource_count_by_type(&res_mgr, OZAYN_RESOURCE_TYPE_DEVICE));
    LOG_INFO("RESOURCE", "IPC resources: %d",
             ozayn_resource_count_by_type(&res_mgr, OZAYN_RESOURCE_TYPE_IPC));

    /* 5. Allocate resources */
    LOG_INFO("RESOURCE", "--- Demonstration: Allocate resources ---");
    rr = ozayn_resource_allocate(&res_mgr, "camera-01", "ozayn.vision");
    LOG_INFO("RESOURCE", "Allocate camera-01 to ozayn.vision: %s",
             ozayn_resource_result_name(rr));

    rr = ozayn_resource_allocate(&res_mgr, "ipc-conn-1", "ozayn.ai");
    LOG_INFO("RESOURCE", "Allocate ipc-conn-1 to ozayn.ai: %s",
             ozayn_resource_result_name(rr));

    /* 6. Activate resources */
    LOG_INFO("RESOURCE", "--- Demonstration: Activate resources ---");
    rr = ozayn_resource_activate(&res_mgr, "camera-01", "ozayn.vision");
    LOG_INFO("RESOURCE", "Activate camera-01: %s",
             ozayn_resource_result_name(rr));

    /* 7. Query resource state */
    LOG_INFO("RESOURCE", "--- Demonstration: Query resource state ---");
    const ozayn_resource_record_t *cam = ozayn_resource_find(&res_mgr, "camera-01");
    if (cam) {
        LOG_INFO("RESOURCE", "camera-01: type=%s state=%s owner=%s ref_count=%d",
                 ozayn_resource_type_name(cam->type),
                 ozayn_resource_state_name(cam->state),
                 cam->owner, cam->ref_count);
    }

    LOG_INFO("RESOURCE", "camera-01 available: %s",
             ozayn_resource_is_available(&res_mgr, "camera-01") ? "yes" : "no");
    LOG_INFO("RESOURCE", "camera-01 owner: %s",
             ozayn_resource_owner(&res_mgr, "camera-01") ? ozayn_resource_owner(&res_mgr, "camera-01") : "none");

    /* 8. Handle-based query */
    LOG_INFO("RESOURCE", "--- Demonstration: Handle-based query ---");
    ozayn_resource_handle_t cam_handle = ozayn_resource_get_handle(cam);
    const ozayn_resource_record_t *cam_by_handle = ozayn_resource_from_handle(&res_mgr, cam_handle);
    LOG_INFO("RESOURCE", "Handle lookup: %s (slot=%u, gen=%u)",
             cam_by_handle ? "valid" : "stale",
             cam_handle.slot, cam_handle.generation);

    /* 9. Cross-service isolation — ai can't allocate camera (exclusive) */
    LOG_INFO("RESOURCE", "--- Demonstration: Cross-service isolation ---");
    rr = ozayn_resource_allocate(&res_mgr, "camera-01", "ozayn.ai");
    LOG_INFO("RESOURCE", "ozayn.ai allocate camera-01: %s (expected: INVALID_STATE — exclusive)",
             ozayn_resource_result_name(rr));

    /* 10. Non-exclusive resource — both can use */
    LOG_INFO("RESOURCE", "--- Demonstration: Shared resource ---");
    rr = ozayn_resource_allocate(&res_mgr, "buffer-01", "ozayn.vision");
    LOG_INFO("RESOURCE", "ozayn.vision allocate buffer-01: %s", ozayn_resource_result_name(rr));
    rr = ozayn_resource_allocate(&res_mgr, "buffer-01", "ozayn.ai");
    LOG_INFO("RESOURCE", "ozayn.ai allocate buffer-01: %s", ozayn_resource_result_name(rr));

    const ozayn_resource_record_t *buf = ozayn_resource_find(&res_mgr, "buffer-01");
    if (buf) {
        LOG_INFO("RESOURCE", "buffer-01 ref_count: %d (shared)", buf->ref_count);
    }

    /* 11. Release resources */
    LOG_INFO("RESOURCE", "--- Demonstration: Release resources ---");
    rr = ozayn_resource_release(&res_mgr, "ipc-conn-1", "ozayn.ai");
    LOG_INFO("RESOURCE", "Release ipc-conn-1: %s", ozayn_resource_result_name(rr));

    rr = ozayn_resource_release(&res_mgr, "buffer-01", "ozayn.vision");
    LOG_INFO("RESOURCE", "Release buffer-01 (ozayn.vision): %s", ozayn_resource_result_name(rr));

    /* 12. Invalid operations */
    LOG_INFO("RESOURCE", "--- Demonstration: Invalid operations ---");
    rr = ozayn_resource_allocate(&res_mgr, "nonexistent-999", "ozayn.vision");
    LOG_INFO("RESOURCE", "Allocate nonexistent: %s (expected: NOT_FOUND)",
             ozayn_resource_result_name(rr));

    rr = ozayn_resource_release(&res_mgr, "camera-01", "ozayn.vision");
    LOG_INFO("RESOURCE", "Release active camera-01: %s", ozayn_resource_result_name(rr));

    /* 13. Double release */
    LOG_INFO("RESOURCE", "--- Demonstration: Double release ---");
    rr = ozayn_resource_release(&res_mgr, "ipc-conn-1", "ozayn.ai");
    LOG_INFO("RESOURCE", "Double release ipc-conn-1: %s (expected: ALREADY_RELEASED)",
             ozayn_resource_result_name(rr));

    /* 14. Statistics */
    LOG_INFO("RESOURCE", "--- Demonstration: Statistics ---");
    ozayn_resource_stats_t stats = ozayn_resource_manager_stats(&res_mgr);
    LOG_INFO("RESOURCE", "Total: %d, Available: %d, Allocated: %d, Active: %d",
             stats.total, stats.available, stats.allocated, stats.active);

    /* 15. Destroy resource */
    LOG_INFO("RESOURCE", "--- Demonstration: Destroy resource ---");
    rr = ozayn_resource_destroy(&res_mgr, "module-instance-1", NULL);
    LOG_INFO("RESOURCE", "Destroy module-instance-1: %s",
             ozayn_resource_result_name(rr));
    LOG_INFO("RESOURCE", "Resources after destroy: %d",
             ozayn_resource_manager_count(&res_mgr));

    /* 16. Process resource events */
    ozayn_events_process(&events);

    /* --- End Resource Manager demonstration --- */

    /* --- Scheduler & Priority Engine demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query scheduler state */
    LOG_INFO("SCHEDULER", "--- Demonstration: Scheduler state ---");
    LOG_INFO("SCHEDULER", "Scheduler enabled: %s",
             sched_mgr.enabled ? "yes" : "no");
    LOG_INFO("SCHEDULER", "Aging enabled: %s",
             sched_mgr.aging_enabled ? "yes" : "no");
    LOG_INFO("SCHEDULER", "Max tasks per source: %d",
             sched_mgr.max_tasks_per_source);

    /* 2. Submit tasks with different priorities */
    LOG_INFO("SCHEDULER", "--- Demonstration: Submit tasks with priorities ---");

    /* Create task records first */
    ozayn_task_t *sched_task1 = ozayn_task_manager_submit(&task_mgr,
                                                           OZAYN_TASK_DEMO,
                                                           OZAYN_TASK_SRC_CORE);
    if (sched_task1) {
        sched_task1->priority = OZAYN_SCHED_PRIORITY_LOW;
        ozayn_scheduler_submit(&sched_mgr, sched_task1->id,
                               OZAYN_SCHED_PRIORITY_LOW, "core");
        LOG_INFO("SCHEDULER", "Submitted task #%u with LOW priority", sched_task1->id);
    }

    ozayn_task_t *sched_task2 = ozayn_task_manager_submit(&task_mgr,
                                                           OZAYN_TASK_DEMO,
                                                           OZAYN_TASK_SRC_CORE);
    if (sched_task2) {
        sched_task2->priority = OZAYN_SCHED_PRIORITY_CRITICAL;
        ozayn_scheduler_submit(&sched_mgr, sched_task2->id,
                               OZAYN_SCHED_PRIORITY_CRITICAL, "core");
        LOG_INFO("SCHEDULER", "Submitted task #%u with CRITICAL priority", sched_task2->id);
    }

    ozayn_task_t *sched_task3 = ozayn_task_manager_submit(&task_mgr,
                                                           OZAYN_TASK_DEMO,
                                                           OZAYN_TASK_SRC_COMMAND);
    if (sched_task3) {
        sched_task3->priority = OZAYN_SCHED_PRIORITY_NORMAL;
        ozayn_scheduler_submit(&sched_mgr, sched_task3->id,
                               OZAYN_SCHED_PRIORITY_NORMAL, "command");
        LOG_INFO("SCHEDULER", "Submitted task #%u with NORMAL priority", sched_task3->id);
    }

    /* 3. Query queue state */
    LOG_INFO("SCHEDULER", "--- Demonstration: Queue state ---");
    LOG_INFO("SCHEDULER", "Ready count: %d", ozayn_scheduler_ready_count(&sched_mgr));

    /* 4. Priority override — change NORMAL to HIGH */
    if (sched_task3) {
        LOG_INFO("SCHEDULER", "--- Demonstration: Priority change ---");
        ozayn_scheduler_set_priority(&sched_mgr, sched_task3->id,
                                      OZAYN_SCHED_PRIORITY_HIGH);
        LOG_INFO("SCHEDULER", "Task #%u new priority: %s",
                 sched_task3->id,
                 ozayn_sched_priority_name(ozayn_scheduler_get_priority(&sched_mgr, sched_task3->id)));
    }

    /* 5. Tick — should dispatch CRITICAL task first */
    LOG_INFO("SCHEDULER", "--- Demonstration: Scheduler tick (dispatch) ---");
    ozayn_scheduler_tick(&sched_mgr);
    LOG_INFO("SCHEDULER", "After tick — ready count: %d", ozayn_scheduler_ready_count(&sched_mgr));

    /* 6. Submit a task, move it to waiting state */
    ozayn_task_t *sched_wait_task = ozayn_task_manager_submit(&task_mgr,
                                                               OZAYN_TASK_DEMO,
                                                               OZAYN_TASK_SRC_CORE);
    if (sched_wait_task) {
        ozayn_scheduler_submit(&sched_mgr, sched_wait_task->id,
                               OZAYN_SCHED_PRIORITY_NORMAL, "core");
        LOG_INFO("SCHEDULER", "--- Demonstration: Move task to WAITING ---");
        ozayn_scheduler_wait(&sched_mgr, sched_wait_task->id,
                             OZAYN_SCHED_WAIT_EVENT);
        LOG_INFO("SCHEDULER", "Waiting count: %d",
                 ozayn_scheduler_waiting_count(&sched_mgr));

        /* Wake it back up */
        LOG_INFO("SCHEDULER", "--- Demonstration: Wake task ---");
        ozayn_scheduler_wake(&sched_mgr, sched_wait_task->id);
        LOG_INFO("SCHEDULER", "Ready count after wake: %d",
                 ozayn_scheduler_ready_count(&sched_mgr));
    }

    /* 7. Block a task */
    ozayn_task_t *sched_block_task = ozayn_task_manager_submit(&task_mgr,
                                                                OZAYN_TASK_DEMO,
                                                                OZAYN_TASK_SRC_CORE);
    if (sched_block_task) {
        ozayn_scheduler_submit(&sched_mgr, sched_block_task->id,
                               OZAYN_SCHED_PRIORITY_LOW, "core");
        LOG_INFO("SCHEDULER", "--- Demonstration: Block task ---");
        ozayn_scheduler_block(&sched_mgr, sched_block_task->id,
                              OZAYN_SCHED_WAIT_RESOURCE);
        LOG_INFO("SCHEDULER", "Blocked count: %d",
                 ozayn_scheduler_blocked_count(&sched_mgr));
    }

    /* 8. Cancel a task */
    ozayn_task_t *sched_cancel_task = ozayn_task_manager_submit(&task_mgr,
                                                                 OZAYN_TASK_DEMO,
                                                                 OZAYN_TASK_SRC_CORE);
    if (sched_cancel_task) {
        ozayn_scheduler_submit(&sched_mgr, sched_cancel_task->id,
                               OZAYN_SCHED_PRIORITY_BACKGROUND, "core");
        LOG_INFO("SCHEDULER", "--- Demonstration: Cancel task ---");
        ozayn_scheduler_cancel(&sched_mgr, sched_cancel_task->id);
        LOG_INFO("SCHEDULER", "Ready count after cancel: %d",
                 ozayn_scheduler_ready_count(&sched_mgr));
    }

    /* 9. Per-source quota test */
    LOG_INFO("SCHEDULER", "--- Demonstration: Per-source quota ---");
    ozayn_result_t quota_r = ozayn_scheduler_submit(&sched_mgr, 999,
                                                     OZAYN_SCHED_PRIORITY_LOW, "quota_test");
    LOG_INFO("SCHEDULER", "First submit to quota_test: %s",
             quota_r == OZAYN_OK ? "OK" : "FAILED");
    /* Submit many more to hit quota */
    for (int qi = 0; qi < 20; qi++) {
        ozayn_scheduler_submit(&sched_mgr, 1000 + qi,
                               OZAYN_SCHED_PRIORITY_LOW, "quota_test");
    }
    quota_r = ozayn_scheduler_submit(&sched_mgr, 2000,
                                      OZAYN_SCHED_PRIORITY_LOW, "quota_test");
    LOG_INFO("SCHEDULER", "Submit after quota hit: %s (expected: FAILED)",
             quota_r == OZAYN_OK ? "OK" : "FAILED");

    /* 10. Dispatch remaining ready tasks */
    LOG_INFO("SCHEDULER", "--- Demonstration: Dispatch remaining ---");
    while (ozayn_scheduler_ready_count(&sched_mgr) > 0) {
        ozayn_scheduler_tick(&sched_mgr);
    }
    LOG_INFO("SCHEDULER", "All tasks dispatched — ready: %d, waiting: %d, blocked: %d",
             ozayn_scheduler_ready_count(&sched_mgr),
             ozayn_scheduler_waiting_count(&sched_mgr),
             ozayn_scheduler_blocked_count(&sched_mgr));

    /* 11. Priority name and state name queries */
    LOG_INFO("SCHEDULER", "--- Demonstration: Name queries ---");
    LOG_INFO("SCHEDULER", "Priority 0 = %s", ozayn_sched_priority_name(OZAYN_SCHED_PRIORITY_BACKGROUND));
    LOG_INFO("SCHEDULER", "Priority 4 = %s", ozayn_sched_priority_name(OZAYN_SCHED_PRIORITY_CRITICAL));
    LOG_INFO("SCHEDULER", "State 1 = %s", ozayn_sched_state_name(OZAYN_SCHED_STATE_READY));
    LOG_INFO("SCHEDULER", "Wait 2 = %s", ozayn_sched_wait_reason_name(OZAYN_SCHED_WAIT_RESOURCE));

    /* 12. Statistics */
    LOG_INFO("SCHEDULER", "--- Demonstration: Statistics ---");
    ozayn_sched_stats_t sched_stats = ozayn_scheduler_stats(&sched_mgr);
    LOG_INFO("SCHEDULER", "Submitted: %d, Executed: %d, Completed: %d, Failed: %d",
             sched_stats.total_submitted, sched_stats.total_executed,
             sched_stats.total_completed, sched_stats.total_failed);
    LOG_INFO("SCHEDULER", "Cancelled: %d, Waited: %d, Blocked: %d",
             sched_stats.total_cancelled, sched_stats.total_waited,
             sched_stats.total_blocked);
    LOG_INFO("SCHEDULER", "Starvation preventions: %d", sched_stats.starvation_preventions);

    /* 13. Process scheduler events */
    ozayn_events_process(&events);

    /* --- End Scheduler & Priority Engine demonstration --- */

    /* --- Monitoring & Health Engine demonstration --- */

    ozayn_events_process(&events);

    /* 1. Query monitoring state */
    LOG_INFO("MONITORING", "--- Demonstration: Monitoring state ---");
    LOG_INFO("MONITORING", "Monitoring enabled: %s",
             ozayn_monitoring_is_enabled(&mon_mgr) ? "yes" : "no");

    /* 2. Self-report component health */
    LOG_INFO("MONITORING", "--- Demonstration: Component health reporting ---");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_SCHEDULER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO,
                                    "operational");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_RESOURCE_MANAGER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO,
                                    "operational");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_IPC,
                                    OZAYN_HEALTH_DEGRADED, OZAYN_SEVERITY_WARNING,
                                    "queue latency high");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_PLUGIN_MANAGER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO,
                                    "operational");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_TASK_MANAGER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO,
                                    "operational");

    /* 3. Query component health */
    LOG_INFO("MONITORING", "--- Demonstration: Component health query ---");
    LOG_INFO("MONITORING", "SCHEDULER: %s",
             ozayn_health_state_name(ozayn_monitoring_get_health(&mon_mgr, OZAYN_COMP_SCHEDULER)));
    LOG_INFO("MONITORING", "IPC: %s",
             ozayn_health_state_name(ozayn_monitoring_get_health(&mon_mgr, OZAYN_COMP_IPC)));
    LOG_INFO("MONITORING", "UNKNOWN component: %s",
             ozayn_health_state_name(ozayn_monitoring_get_health(&mon_mgr, OZAYN_COMP_CORE)));

    /* 4. Overall health (should be DEGRADED because IPC is degraded) */
    LOG_INFO("MONITORING", "--- Demonstration: Overall health ---");
    ozayn_health_state_t overall = ozayn_monitoring_overall_health(&mon_mgr);
    LOG_INFO("MONITORING", "Overall system health: %s",
             ozayn_health_state_name(overall));

    /* 5. Register metrics */
    LOG_INFO("MONITORING", "--- Demonstration: Register metrics ---");
    ozayn_monitoring_register_metric(&mon_mgr, "task.total_created",
                                      OZAYN_METRIC_COUNTER, OZAYN_COMP_TASK_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "task.active",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_TASK_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "scheduler.ready_queue",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_SCHEDULER);
    ozayn_monitoring_register_metric(&mon_mgr, "scheduler.total_executed",
                                      OZAYN_METRIC_COUNTER, OZAYN_COMP_SCHEDULER);
    ozayn_monitoring_register_metric(&mon_mgr, "resource.total",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_RESOURCE_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "resource.allocated",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_RESOURCE_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "ipc.connections",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_IPC);
    ozayn_monitoring_register_metric(&mon_mgr, "process.active",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_PROCESS_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "event.queue_depth",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_EVENT_ENGINE);
    ozayn_monitoring_register_metric(&mon_mgr, "module.active",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_MODULE_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "plugin.active",
                                      OZAYN_METRIC_GAUGE, OZAYN_COMP_PLUGIN_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "error.total",
                                      OZAYN_METRIC_COUNTER, OZAYN_COMP_ERROR_RECOVERY);

    LOG_INFO("MONITORING", "Registered %d metrics",
             ozayn_monitoring_metric_count(&mon_mgr));

    /* 6. Update metrics from current state */
    LOG_INFO("MONITORING", "--- Demonstration: Update metrics ---");
    ozayn_monitoring_update_metric(&mon_mgr, "task.total_created",
                                    (int64_t)task_mgr.next_id - 1);
    ozayn_monitoring_update_metric(&mon_mgr, "task.active",
                                    (int64_t)ozayn_task_manager_active_count(&task_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "scheduler.ready_queue",
                                    (int64_t)ozayn_scheduler_ready_count(&sched_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "scheduler.total_executed",
                                    (int64_t)sched_mgr.stats.total_executed);
    ozayn_monitoring_update_metric(&mon_mgr, "resource.total",
                                    (int64_t)res_mgr.resource_count);
    ozayn_monitoring_update_metric(&mon_mgr, "ipc.connections",
                                    (int64_t)ozayn_ipc_manager_connection_count(&ipc_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "process.active",
                                    (int64_t)ozayn_process_manager_active_count(&proc_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "event.queue_depth",
                                    (int64_t)ozayn_events_queue_count(&events));
    ozayn_monitoring_update_metric(&mon_mgr, "module.active",
                                    (int64_t)ozayn_module_manager_active_count(&mod_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "plugin.active",
                                    (int64_t)ozayn_plugin_manager_active_count(&plug_mgr));
    ozayn_monitoring_update_metric(&mon_mgr, "error.total",
                                    (int64_t)recovery.total_errors);

    /* 7. Query individual metric */
    LOG_INFO("MONITORING", "--- Demonstration: Query metric ---");
    const ozayn_metric_record_t *m = ozayn_monitoring_get_metric(&mon_mgr, "task.total_created");
    if (m) {
        LOG_INFO("MONITORING", "task.total_created = %lld (type=%s, component=%s)",
                 (long long)m->value,
                 ozayn_metric_type_name(m->type),
                 ozayn_component_name(m->component));
    }

    /* 8. Counter increment */
    LOG_INFO("MONITORING", "--- Demonstration: Counter increment ---");
    ozayn_monitoring_increment_metric(&mon_mgr, "error.total", 1);
    m = ozayn_monitoring_get_metric(&mon_mgr, "error.total");
    if (m) {
        LOG_INFO("MONITORING", "error.total after increment = %lld", (long long)m->value);
    }

    /* 9. Create incidents */
    LOG_INFO("MONITORING", "--- Demonstration: Create incidents ---");
    ozayn_monitoring_create_incident(&mon_mgr, OZAYN_COMP_IPC,
                                      OZAYN_SEVERITY_WARNING,
                                      "IPC queue latency above threshold");
    ozayn_monitoring_create_incident(&mon_mgr, OZAYN_COMP_SCHEDULER,
                                      OZAYN_SEVERITY_ERROR,
                                      "scheduler unresponsive");

    int open_incidents = ozayn_monitoring_open_incident_count(&mon_mgr);
    LOG_INFO("MONITORING", "Open incidents: %d", open_incidents);

    /* 10. Resolve an incident */
    LOG_INFO("MONITORING", "--- Demonstration: Resolve incident ---");
    ozayn_monitoring_resolve_incident(&mon_mgr, 1);
    open_incidents = ozayn_monitoring_open_incident_count(&mon_mgr);
    LOG_INFO("MONITORING", "Open incidents after resolve: %d", open_incidents);

    /* 11. Health transitions */
    LOG_INFO("MONITORING", "--- Demonstration: Health transitions ---");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_IPC,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO,
                                    "queue recovered");
    overall = ozayn_monitoring_overall_health(&mon_mgr);
    LOG_INFO("MONITORING", "Overall after IPC recovery: %s",
             ozayn_health_state_name(overall));

    /* 12. HEALTH command */
    LOG_INFO("MONITORING", "--- Demonstration: HEALTH command ---");
    ozayn_command_t cmd_health = ozayn_command_create(OZAYN_CMD_HEALTH, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_health);

    /* 13. METRICS command */
    LOG_INFO("MONITORING", "--- Demonstration: METRICS command ---");
    ozayn_command_t cmd_metrics = ozayn_command_create(OZAYN_CMD_METRICS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_metrics);

    /* 14. Statistics */
    LOG_INFO("MONITORING", "--- Demonstration: Statistics ---");
    ozayn_monitor_stats_t mon_stats = ozayn_monitoring_stats(&mon_mgr);
    LOG_INFO("MONITORING", "Total checks: %d, Health changes: %d",
             mon_stats.total_checks, mon_stats.health_changes);
    LOG_INFO("MONITORING", "Incidents created: %d, Resolved: %d",
             mon_stats.incidents_created, mon_stats.incidents_resolved);
    LOG_INFO("MONITORING", "Metrics registered: %d, Updated: %d",
             mon_stats.metrics_registered, mon_stats.metrics_updated);

    /* 15. Duplicate metric registration (should fail) */
    LOG_INFO("MONITORING", "--- Demonstration: Duplicate metric ---");
    ozayn_result_t dup_r = ozayn_monitoring_register_metric(&mon_mgr, "task.total_created",
                                                              OZAYN_METRIC_COUNTER,
                                                              OZAYN_COMP_TASK_MANAGER);
    LOG_INFO("MONITORING", "Duplicate metric registration: %s (expected: ERROR)",
             dup_r == OZAYN_ERR_STATE ? "ERROR" : "OK");

    /* 16. Process monitoring events */
    ozayn_events_process(&events);

    /* --- End Monitoring & Health Engine demonstration --- */

    /* --- Diagnostics & Debugging Engine demonstration --- */

    /* 1. Diagnostics state */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Diagnostics state ---");
    LOG_INFO("DIAGNOSTICS", "Diagnostics enabled: %s",
             ozayn_diagnostics_is_enabled(&diag_mgr) ? "yes" : "no");
    LOG_INFO("DIAGNOSTICS", "Level: %s",
             ozayn_diag_level_name(ozayn_diagnostics_get_level(&diag_mgr)));

    /* 2. Set diagnostic level */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Set level ---");
    ozayn_diagnostics_set_level(&diag_mgr, OZAYN_DIAG_LEVEL_DETAILED);
    LOG_INFO("DIAGNOSTICS", "Level after set: %s",
             ozayn_diag_level_name(ozayn_diagnostics_get_level(&diag_mgr)));

    /* 3. Record evidence */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Record evidence ---");
    uint32_t ev1 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                                       OZAYN_DIAG_TARGET_IPC,
                                                       "REQ-1001",
                                                       "IPC queue latency above threshold");
    uint32_t ev2 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_SCHEDULER,
                                                       OZAYN_DIAG_TARGET_SCHEDULER,
                                                       "REQ-1001",
                                                       "Scheduler queue depth increased");
    uint32_t ev3 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                                       OZAYN_DIAG_TARGET_TASK,
                                                       "REQ-1001",
                                                       "Task #10 entered WAITING state");
    uint32_t ev4 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                                       OZAYN_DIAG_TARGET_TASK,
                                                       "REQ-1001",
                                                       "Task #10 timeout occurred");
    uint32_t ev5 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                                       OZAYN_DIAG_TARGET_TASK,
                                                       "REQ-1001",
                                                       "Task #10 failed");
    (void)ev2; (void)ev3; (void)ev4; (void)ev5;

    LOG_INFO("DIAGNOSTICS", "Evidence recorded: %d",
             ozayn_diagnostics_evidence_count(&diag_mgr));

    /* 4. Query evidence */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Query evidence ---");
    const ozayn_evidence_t *ev = ozayn_diagnostics_get_evidence(&diag_mgr, ev1);
    if (ev) {
        LOG_INFO("DIAGNOSTICS", "Evidence #%u: [%s] %s (corr=%s)",
                 ev->id, ozayn_diag_component_name(ev->component),
                 ev->description, ev->correlation_id);
    }

    /* 5. Add timeline entries */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Timeline ---");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_COMMAND_ENGINE,
                                    "REQ-1001", "Command received");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                    "REQ-1001", "Task #10 created");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_RESOURCE_MANAGER,
                                    "REQ-1001", "Resource camera-01 allocated");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_SCHEDULER,
                                    "REQ-1001", "Task #10 started");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                    "REQ-1001", "IPC latency increased");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                    "REQ-1001", "Task #10 blocked");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                    "REQ-1001", "Timeout — task #10 failed");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_ERROR_RECOVERY,
                                    "REQ-1001", "Recovery attempted — restart task");
    ozayn_diagnostics_timeline_add(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                    "REQ-1001", "Task #10 restarted — SUCCESS");

    LOG_INFO("DIAGNOSTICS", "Timeline entries: %d",
             ozayn_diagnostics_timeline_count(&diag_mgr));

    /* 6. Print timeline for correlation ID */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Timeline (REQ-1001) ---");
    ozayn_diagnostics_timeline_print(&diag_mgr, "REQ-1001");

    /* 7. Generate findings */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Generate findings ---");
    ozayn_diagnostics_add_finding(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                    OZAYN_DIAG_SEV_WARNING, "REQ-1001",
                                    "IPC queue latency above threshold",
                                    "workload spike",
                                    OZAYN_DIAG_CONFIDENCE_HIGH);
    ozayn_diagnostics_add_finding(&diag_mgr, OZAYN_DIAG_COMP_TASK_MANAGER,
                                    OZAYN_DIAG_SEV_ERROR, "REQ-1001",
                                    "Task timeout after IPC degradation",
                                    "IPC latency caused task starvation",
                                    OZAYN_DIAG_CONFIDENCE_MEDIUM);
    ozayn_diagnostics_add_finding(&diag_mgr, OZAYN_DIAG_COMP_SCHEDULER,
                                    OZAYN_DIAG_SEV_INFO, "REQ-1001",
                                    "Scheduler queue increased during IPC issue",
                                    "correlated with IPC latency",
                                    OZAYN_DIAG_CONFIDENCE_LOW);

    LOG_INFO("DIAGNOSTICS", "Findings: %d",
             ozayn_diagnostics_finding_count(&diag_mgr));

    /* 8. Query finding */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Query finding ---");
    const ozayn_diagnostic_finding_t *finding = ozayn_diagnostics_get_finding(&diag_mgr, 1);
    if (finding) {
        LOG_INFO("DIAGNOSTICS", "Finding #%u: [%s] %s",
                 finding->id,
                 ozayn_diag_component_name(finding->component),
                 finding->observation);
        LOG_INFO("DIAGNOSTICS", "  Cause: %s (confidence=%s)",
                 finding->possible_cause,
                 ozayn_diag_confidence_name(finding->confidence));
    }

    /* 9. Diagnostic session */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Diagnostic session ---");
    uint32_t session_id = ozayn_diagnostics_session_start(&diag_mgr,
                                                            OZAYN_DIAG_TARGET_IPC,
                                                            "REQ-1001");
    LOG_INFO("DIAGNOSTICS", "Session started: #%u", session_id);

    ozayn_diagnostics_session_set_state(&diag_mgr, session_id,
                                         OZAYN_DIAG_SESSION_COLLECTING);
    ozayn_diagnostics_session_set_state(&diag_mgr, session_id,
                                         OZAYN_DIAG_SESSION_ANALYZING);
    ozayn_diagnostics_session_set_state(&diag_mgr, session_id,
                                         OZAYN_DIAG_SESSION_FINDINGS);
    ozayn_diagnostics_session_set_state(&diag_mgr, session_id,
                                         OZAYN_DIAG_SESSION_COMPLETED);

    const ozayn_diag_session_t *session = ozayn_diagnostics_session_get(&diag_mgr, session_id);
    if (session) {
        LOG_INFO("DIAGNOSTICS", "Session #%u: %s (evidence=%d, findings=%d)",
                 session->id,
                 ozayn_diag_session_state_name(session->state),
                 session->evidence_count,
                 session->finding_count);
    }

    /* 10. Snapshot capture */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Snapshot ---");
    uint32_t snap_id = ozayn_diagnostics_snapshot_capture(&diag_mgr, &mon_mgr);
    const ozayn_diag_snapshot_t *snap = ozayn_diagnostics_snapshot_get(&diag_mgr, snap_id);
    if (snap) {
        LOG_INFO("DIAGNOSTICS", "Snapshot #%u: overall=%s, incidents=%d",
                 snap->id,
                 ozayn_diag_health_name(snap->overall_health),
                 snap->open_incidents);
    }

    /* 11. Failure tracking */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Failure tracking ---");
    ozayn_diagnostics_record_failure(&diag_mgr, OZAYN_DIAG_COMP_IPC);
    ozayn_diagnostics_record_failure(&diag_mgr, OZAYN_DIAG_COMP_IPC);
    ozayn_diagnostics_record_failure(&diag_mgr, OZAYN_DIAG_COMP_IPC);
    ozayn_diagnostics_record_failure(&diag_mgr, OZAYN_DIAG_COMP_IPC);

    LOG_INFO("DIAGNOSTICS", "IPC failures: %d",
             ozayn_diagnostics_failure_count(&diag_mgr, OZAYN_DIAG_COMP_IPC));
    LOG_INFO("DIAGNOSTICS", "IPC repeated (threshold=3): %s",
             ozayn_diagnostics_is_repeated_failure(&diag_mgr, OZAYN_DIAG_COMP_IPC, 3)
             ? "YES" : "NO");

    /* 12. Auto-diagnose: task failure */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Auto-diagnose task failure ---");
    ozayn_diagnostics_on_task_failure(&diag_mgr, 10, OZAYN_DIAG_COMP_TASK_MANAGER,
                                       "resource unavailable");

    /* 13. Auto-diagnose: resource failure */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Auto-diagnose resource failure ---");
    ozayn_diagnostics_on_resource_failure(&diag_mgr, "camera-01",
                                            OZAYN_DIAG_COMP_RESOURCE_MANAGER,
                                            "device disconnected");

    /* 14. Auto-diagnose: health change */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Auto-diagnose health change ---");
    ozayn_diagnostics_on_health_change(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                         OZAYN_DIAG_HEALTH_HEALTHY, OZAYN_DIAG_HEALTH_DEGRADED);

    /* 15. Failure summary */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Failure summary ---");
    ozayn_diagnostics_print_failure_summary(&diag_mgr);

    /* 16. DIAGNOSE command */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: DIAGNOSE command ---");
    ozayn_command_t cmd_diag = ozayn_command_create(OZAYN_CMD_DIAGNOSE, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_diag);

    /* 17. SNAPSHOT command */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: SNAPSHOT command ---");
    ozayn_command_t cmd_snap = ozayn_command_create(OZAYN_CMD_SNAPSHOT, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_snap);

    /* 18. INCIDENTS command */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: INCIDENTS command ---");
    ozayn_command_t cmd_inc = ozayn_command_create(OZAYN_CMD_INCIDENTS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_inc);

    /* 19. TRACE command */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: TRACE command ---");
    ozayn_command_t cmd_trace = ozayn_command_create(OZAYN_CMD_TRACE, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_trace);

    /* 20. Statistics */
    LOG_INFO("DIAGNOSTICS", "--- Demonstration: Statistics ---");
    ozayn_diag_stats_t diag_stats = ozayn_diagnostics_stats(&diag_mgr);
    LOG_INFO("DIAGNOSTICS", "Evidence recorded: %d", diag_stats.evidence_recorded);
    LOG_INFO("DIAGNOSTICS", "Findings generated: %d", diag_stats.findings_generated);
    LOG_INFO("DIAGNOSTICS", "Timeline entries: %d", diag_stats.timeline_entries);
    LOG_INFO("DIAGNOSTICS", "Sessions: %d created, %d completed",
             diag_stats.sessions_created, diag_stats.sessions_completed);
    LOG_INFO("DIAGNOSTICS", "Snapshots: %d", diag_stats.snapshots_captured);
    LOG_INFO("DIAGNOSTICS", "Repeated failures detected: %d",
             diag_stats.repeated_failures_detected);

    /* 21. Process diagnostics events */
    ozayn_events_process(&events);

    /* --- End Diagnostics & Debugging Engine demonstration --- */

    /* ================================================================
     * Security & Isolation Boundary demonstration
     * ================================================================ */

    /* 1. Security boundary state */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Security boundary state ---");
    LOG_INFO("SECURITY_BOUNDARY", "Enabled: %s", ozayn_security_boundary_is_enabled(&sec_bnd_mgr) ? "yes" : "no");
    LOG_INFO("SECURITY_BOUNDARY", "Fail closed: %s", sec_bnd_mgr.policy.fail_closed ? "yes" : "no");

    /* 2. Trust level names */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Trust level names ---");
    LOG_INFO("SECURITY_BOUNDARY", "Trust 0 = %s", ozayn_sb_trust_level_name(OZAYN_SB_TRUST_CORE));
    LOG_INFO("SECURITY_BOUNDARY", "Trust 3 = %s", ozayn_sb_trust_level_name(OZAYN_SB_TRUST_CONTROLLED));
    LOG_INFO("SECURITY_BOUNDARY", "Trust 5 = %s", ozayn_sb_trust_level_name(OZAYN_SB_TRUST_UNTRUSTED));

    /* 3. Capability names */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Capability names ---");
    LOG_INFO("SECURITY_BOUNDARY", "Cap 1 = %s", ozayn_capability_name(OZAYN_CAP_CAMERA_READ));
    LOG_INFO("SECURITY_BOUNDARY", "Cap 20 = %s", ozayn_capability_name(OZAYN_CAP_TASK_CREATE));
    LOG_INFO("SECURITY_BOUNDARY", "Cap 61 = %s", ozayn_capability_name(OZAYN_CAP_SECURITY_ADMIN));

    /* 4. Register security contexts */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Register contexts ---");

    /* Core component — full trust */
    uint32_t ctx_core = ozayn_security_boundary_register_context(
        &sec_bnd_mgr, "ozayn.core", OZAYN_SB_TRUST_CORE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_SECURITY_ADMIN);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_CORE_SHUTDOWN);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_TASK_CREATE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_CONFIG_WRITE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_PROCESS_STOP);
    LOG_INFO("SECURITY_BOUNDARY", "Register ozayn.core: ctx=%u (trust=CORE)", ctx_core);

    /* Vision plugin — limited trust */
    uint32_t ctx_vision = ozayn_security_boundary_register_context(
        &sec_bnd_mgr, "plugin.vision", OZAYN_SB_TRUST_LIMITED);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_CAMERA_READ);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_IPC_SEND);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_TASK_CREATE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_METRICS_READ);
    LOG_INFO("SECURITY_BOUNDARY", "Register plugin.vision: ctx=%u (trust=LIMITED)", ctx_vision);

    /* Unknown plugin — untrusted */
    uint32_t ctx_unknown = ozayn_security_boundary_register_context(
        &sec_bnd_mgr, "plugin.unknown", OZAYN_SB_TRUST_UNTRUSTED);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_unknown, OZAYN_CAP_METRICS_READ);
    LOG_INFO("SECURITY_BOUNDARY", "Register plugin.unknown: ctx=%u (trust=UNTRUSTED)", ctx_unknown);

    /* 5. Query contexts */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Query context ---");
    ozayn_security_context_t *ctx = ozayn_security_boundary_get_context(&sec_bnd_mgr, ctx_vision);
    if (ctx) {
        LOG_INFO("SECURITY_BOUNDARY", "Context '%s' trust=%s state=%s caps=%d",
                 ctx->component_id,
                 ozayn_sb_trust_level_name(ctx->trust_level),
                 ozayn_component_sec_state_name(ctx->state),
                 ctx->capability_count);
    }

    /* 6. Capability check — allowed */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Capability check (allowed) ---");
    ozayn_security_check_result_t check1 = ozayn_security_boundary_check(
        &sec_bnd_mgr, ctx_vision, OZAYN_CAP_CAMERA_READ);
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision camera.read -> %s (reason=%s)",
             check1.allowed ? "ALLOWED" : "DENIED", check1.reason);

    /* 7. Capability check — denied */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Capability check (denied) ---");
    ozayn_security_check_result_t check2 = ozayn_security_boundary_check(
        &sec_bnd_mgr, ctx_vision, OZAYN_CAP_SECURITY_ADMIN);
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision security.admin -> %s (reason=%s)",
             check2.allowed ? "ALLOWED" : "DENIED", check2.reason);

    /* 8. Capability check — sensitive (critical severity) */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Capability check (sensitive) ---");
    ozayn_security_check_result_t check3 = ozayn_security_boundary_check(
        &sec_bnd_mgr, ctx_vision, OZAYN_CAP_CORE_SHUTDOWN);
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision core.shutdown -> %s (severity=%s, action=%s)",
             check3.allowed ? "ALLOWED" : "DENIED",
             ozayn_security_severity_name(check3.severity),
             ozayn_security_action_name(check3.action));

    /* 9. IPC boundary — allowed */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: IPC check (allowed) ---");
    ozayn_security_check_result_t ipc1 = ozayn_security_boundary_check_ipc(
        &sec_bnd_mgr, ctx_vision, "TASK_MANAGER");
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision -> TASK_MANAGER IPC: %s",
             ipc1.allowed ? "ALLOWED" : "DENIED");

    /* 10. IPC boundary — denied (untrusted to security) */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: IPC check (denied) ---");
    ozayn_security_check_result_t ipc2 = ozayn_security_boundary_check_ipc(
        &sec_bnd_mgr, ctx_unknown, "SECURITY");
    LOG_INFO("SECURITY_BOUNDARY", "plugin.unknown -> SECURITY IPC: %s (reason=%s)",
             ipc2.allowed ? "ALLOWED" : "DENIED", ipc2.reason);

    /* 11. Task check — allowed */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Task check (allowed) ---");
    ozayn_security_check_result_t sec_task1 = ozayn_security_boundary_check_task(
        &sec_bnd_mgr, ctx_vision);
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision task create: %s",
             sec_task1.allowed ? "ALLOWED" : "DENIED");

    /* 12. Resource check */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Resource check ---");
    ozayn_security_check_result_t res1 = ozayn_security_boundary_check_resource(
        &sec_bnd_mgr, ctx_vision, "camera-01");
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision resource camera-01: %s",
             res1.allowed ? "ALLOWED" : "DENIED");

    /* 13. Context inheritance (task creates child context) */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Context inheritance ---");
    uint32_t ctx_task = ozayn_security_boundary_inherit_context(&sec_bnd_mgr, ctx_vision);
    LOG_INFO("SECURITY_BOUNDARY", "Task inherited from plugin.vision: ctx=%u", ctx_task);
    ozayn_security_context_t *child_ctx = ozayn_security_boundary_get_context(&sec_bnd_mgr, ctx_task);
    if (child_ctx) {
        LOG_INFO("SECURITY_BOUNDARY", "Child trust=%s caps=%d",
                 ozayn_sb_trust_level_name(child_ctx->trust_level),
                 child_ctx->capability_count);
    }

    /* 14. Resource limits */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Resource limits ---");
    ozayn_resource_limits_t vision_limits = {
        .max_tasks = 8,
        .max_resources = 4,
        .max_ipc_channels = 4,
        .max_memory_bytes = 128 * 1024 * 1024,
        .max_cpu_percent = 25,
        .max_request_rate = 100,
    };
    ozayn_security_boundary_set_limits(&sec_bnd_mgr, ctx_vision, &vision_limits);
    const ozayn_resource_limits_t *lim = ozayn_security_boundary_get_limits(&sec_bnd_mgr, ctx_vision);
    if (lim) {
        LOG_INFO("SECURITY_BOUNDARY", "plugin.vision limits: tasks=%d, resources=%d, memory=%lld",
                 lim->max_tasks, lim->max_resources, (long long)lim->max_memory_bytes);
    }

    /* 15. Violation reporting (simulate repeated violations) */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Violation reporting ---");
    ozayn_security_boundary_report_violation(&sec_bnd_mgr, ctx_unknown,
                                              OZAYN_VIOLATION_CAPABILITY_DENIED,
                                              OZAYN_SEC_SEV_MEDIUM,
                                              "tried to access config.write");
    ozayn_security_boundary_report_violation(&sec_bnd_mgr, ctx_unknown,
                                              OZAYN_VIOLATION_PRIVILEGE_ESCALATION,
                                              OZAYN_SEC_SEV_HIGH,
                                              "attempted security.admin without capability");
    ozayn_security_boundary_report_violation(&sec_bnd_mgr, ctx_unknown,
                                              OZAYN_VIOLATION_SANDBOX_BREACH,
                                              OZAYN_SEC_SEV_CRITICAL,
                                              "filesystem escape attempt");

    /* 16. Violation count */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Violation count ---");
    LOG_INFO("SECURITY_BOUNDARY", "Total violations: %d",
             ozayn_security_boundary_violation_count(&sec_bnd_mgr));

    /* 17. Print all violations */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Print violations ---");
    ozayn_security_boundary_print_violations(&sec_bnd_mgr);

    /* 18. State transition — manual restrict/isolate/restore */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: State transitions ---");
    ozayn_security_boundary_restrict(&sec_bnd_mgr, ctx_vision);
    ctx = ozayn_security_boundary_find_context(&sec_bnd_mgr, "plugin.vision");
    if (ctx) {
        LOG_INFO("SECURITY_BOUNDARY", "plugin.vision state: %s",
                 ozayn_component_sec_state_name(ctx->state));
    }
    ozayn_security_boundary_isolate(&sec_bnd_mgr, ctx_vision);
    ctx = ozayn_security_boundary_find_context(&sec_bnd_mgr, "plugin.vision");
    if (ctx) {
        LOG_INFO("SECURITY_BOUNDARY", "plugin.vision state after isolate: %s",
                 ozayn_component_sec_state_name(ctx->state));
    }
    /* Check while isolated — should be denied */
    ozayn_security_check_result_t isolated_check = ozayn_security_boundary_check(
        &sec_bnd_mgr, ctx_vision, OZAYN_CAP_CAMERA_READ);
    LOG_INFO("SECURITY_BOUNDARY", "plugin.vision camera.read while ISOLATED: %s (reason=%s)",
             isolated_check.allowed ? "ALLOWED" : "DENIED", isolated_check.reason);
    ozayn_security_boundary_restore(&sec_bnd_mgr, ctx_vision);
    ctx = ozayn_security_boundary_find_context(&sec_bnd_mgr, "plugin.vision");
    if (ctx) {
        LOG_INFO("SECURITY_BOUNDARY", "plugin.vision state after restore: %s",
                 ozayn_component_sec_state_name(ctx->state));
    }

    /* 19. Security event publishing */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Security events ---");
    ozayn_events_publish(&events, OZAYN_EVENT_SEC_VIOLATION,
                         OZAYN_SRC_SECURITY, NULL);
    ozayn_events_publish(&events, OZAYN_EVENT_SEC_COMPONENT_RESTRICTED,
                         OZAYN_SRC_SECURITY, NULL);
    ozayn_events_process(&events);

    /* 20. Print all contexts */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: All contexts ---");
    ozayn_security_boundary_print_contexts(&sec_bnd_mgr);

    /* 21. SEC STATUS command */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: SEC STATUS command ---");
    ozayn_command_t cmd_sec = ozayn_command_create(OZAYN_CMD_SEC_STATUS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_sec);

    /* 22. SEC CONTEXTS command */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: SEC CONTEXTS command ---");
    ozayn_command_t cmd_sec_ctx = ozayn_command_create(OZAYN_CMD_SEC_CONTEXTS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_sec_ctx);

    /* 23. SEC VIOLATIONS command */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: SEC VIOLATIONS command ---");
    ozayn_command_t cmd_sec_vio = ozayn_command_create(OZAYN_CMD_SEC_VIOLATIONS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_sec_vio);

    /* 24. Statistics */
    LOG_INFO("SECURITY_BOUNDARY", "--- Demonstration: Statistics ---");
    ozayn_security_boundary_stats_t sec_stats = ozayn_security_boundary_stats(&sec_bnd_mgr);
    LOG_INFO("SECURITY_BOUNDARY", "Checks: %d (allowed=%d, denied=%d)",
             sec_stats.total_checks, sec_stats.total_allowed, sec_stats.total_denied);
    LOG_INFO("SECURITY_BOUNDARY", "Violations: %d", sec_stats.total_violations);
    LOG_INFO("SECURITY_BOUNDARY", "Contexts: %d (restricted=%d, isolated=%d)",
             sec_stats.components_registered,
             sec_stats.restricted_components, sec_stats.isolated_components);

    /* 25. Process security events */
    ozayn_events_process(&events);

    /* --- End Security & Isolation Boundary demonstration --- */

    /* ================================================================
     * Persistence & State Management demonstration
     * ================================================================ */

    /* 1. State manager state */
    LOG_INFO("STATE", "--- Demonstration: State manager state ---");
    LOG_INFO("STATE", "State manager enabled: %s",
             state_mgr.enabled ? "yes" : "no");
    LOG_INFO("STATE", "Storage path: %s", state_mgr.storage_path);
    LOG_INFO("STATE", "Entries: %d", ozayn_state_count(&state_mgr));

    /* 2. Category and namespace names */
    LOG_INFO("STATE", "--- Demonstration: Category/namespace names ---");
    LOG_INFO("STATE", "Category 0 = %s", ozayn_state_category_name(OZAYN_STATE_CAT_TRANSIENT));
    LOG_INFO("STATE", "Category 1 = %s", ozayn_state_category_name(OZAYN_STATE_CAT_PERSISTENT));
    LOG_INFO("STATE", "Namespace 0 = %s", ozayn_state_namespace_name(OZAYN_STATE_NS_CORE));
    LOG_INFO("STATE", "Recovery 2 = %s", ozayn_state_recovery_name(OZAYN_STATE_RECOVER_ON_RESTART));

    /* 3. Create persistent state entries */
    LOG_INFO("STATE", "--- Demonstration: Create persistent state ---");

    /* Core configuration */
    const char *log_level = "detailed";
    uint32_t id1 = ozayn_state_create(&state_mgr, "core.log_level", "core",
                                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                                       OZAYN_STATE_RECOVER_ON_RESTART,
                                       log_level, (uint32_t)strlen(log_level) + 1);
    LOG_INFO("STATE", "Create core.log_level: id=%u", id1);

    int32_t interval = 2;
    uint32_t id2 = ozayn_state_create(&state_mgr, "core.interval", "core",
                                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                                       OZAYN_STATE_RECOVER_ON_RESTART,
                                       &interval, sizeof(interval));
    LOG_INFO("STATE", "Create core.interval: id=%u", id2);

    /* Security policy */
    int enabled_flag = 1;
    uint32_t id3 = ozayn_state_create(&state_mgr, "security.enabled", "security",
                                       OZAYN_STATE_NS_SECURITY, OZAYN_STATE_CAT_PERSISTENT,
                                       OZAYN_STATE_RECOVER_ALWAYS,
                                       &enabled_flag, sizeof(enabled_flag));
    LOG_INFO("STATE", "Create security.enabled: id=%u", id3);

    /* Plugin registry */
    const char *plugin_list = "test_plugin";
    uint32_t id4 = ozayn_state_create(&state_mgr, "plugins.registry", "plugin_mgr",
                                       OZAYN_STATE_NS_PLUGINS, OZAYN_STATE_CAT_PERSISTENT,
                                       OZAYN_STATE_RECOVER_ON_RESTART,
                                       plugin_list, (uint32_t)strlen(plugin_list) + 1);
    LOG_INFO("STATE", "Create plugins.registry: id=%u", id4);

    /* Task definition (recoverable) */
    const char *task_def = "demo_task_v1";
    uint32_t id5 = ozayn_state_create(&state_mgr, "tasks.definition.1", "scheduler",
                                       OZAYN_STATE_NS_TASKS, OZAYN_STATE_CAT_RECOVERABLE,
                                       OZAYN_STATE_RECOVER_ON_FAILURE,
                                       task_def, (uint32_t)strlen(task_def) + 1);
    LOG_INFO("STATE", "Create tasks.definition.1: id=%u", id5);

    /* Transient state (won't be saved) */
    const char *runtime_state = "running";
    uint32_t id6 = ozayn_state_create(&state_mgr, "core.runtime_state", "runtime",
                                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_TRANSIENT,
                                       OZAYN_STATE_RECOVER_NEVER,
                                       runtime_state, (uint32_t)strlen(runtime_state) + 1);
    LOG_INFO("STATE", "Create core.runtime_state (transient): id=%u", id6);

    /* 4. Duplicate creation — should fail */
    LOG_INFO("STATE", "--- Demonstration: Duplicate creation ---");
    uint32_t id_dup = ozayn_state_create(&state_mgr, "core.log_level", "core",
                                          OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                                          OZAYN_STATE_RECOVER_ON_RESTART,
                                          "normal", 7);
    LOG_INFO("STATE", "Duplicate core.log_level: id=%u (expected: 0)", id_dup);

    /* 5. Query state entries */
    LOG_INFO("STATE", "--- Demonstration: Query state entries ---");
    const ozayn_state_entry_t *e1 = ozayn_state_get(&state_mgr, "core.log_level");
    if (e1) {
        LOG_INFO("STATE", "core.log_level: v%u, cat=%s, ns=%s, size=%u",
                 e1->version,
                 ozayn_state_category_name(e1->category),
                 ozayn_state_namespace_name(e1->ns),
                 e1->data_size);
        LOG_INFO("STATE", "  Data: '%s'", (const char *)e1->data);
    }

    const ozayn_state_entry_t *e2 = ozayn_state_get(&state_mgr, "core.interval");
    if (e2) {
        LOG_INFO("STATE", "core.interval: v%u, size=%u, data=%d",
                 e2->version, e2->data_size, *(const int32_t *)e2->data);
    }

    /* 6. Query by ID */
    LOG_INFO("STATE", "--- Demonstration: Query by ID ---");
    const ozayn_state_entry_t *e_by_id = ozayn_state_get_by_id(&state_mgr, id3);
    if (e_by_id) {
        LOG_INFO("STATE", "ID %u -> '%s' (owner=%s)", id3, e_by_id->key, e_by_id->owner);
    }

    /* 7. Count by category */
    LOG_INFO("STATE", "--- Demonstration: Count by category ---");
    LOG_INFO("STATE", "Persistent: %d",
             ozayn_state_count_by_category(&state_mgr, OZAYN_STATE_CAT_PERSISTENT));
    LOG_INFO("STATE", "Transient: %d",
             ozayn_state_count_by_category(&state_mgr, OZAYN_STATE_CAT_TRANSIENT));
    LOG_INFO("STATE", "Recoverable: %d",
             ozayn_state_count_by_category(&state_mgr, OZAYN_STATE_CAT_RECOVERABLE));

    /* 8. Count by namespace */
    LOG_INFO("STATE", "--- Demonstration: Count by namespace ---");
    LOG_INFO("STATE", "Core ns: %d", ozayn_state_count_by_namespace(&state_mgr, OZAYN_STATE_NS_CORE));
    LOG_INFO("STATE", "Security ns: %d", ozayn_state_count_by_namespace(&state_mgr, OZAYN_STATE_NS_SECURITY));
    LOG_INFO("STATE", "Plugins ns: %d", ozayn_state_count_by_namespace(&state_mgr, OZAYN_STATE_NS_PLUGINS));

    /* 9. Exists check */
    LOG_INFO("STATE", "--- Demonstration: Exists check ---");
    LOG_INFO("STATE", "core.log_level exists: %s",
             ozayn_state_exists(&state_mgr, "core.log_level") ? "yes" : "no");
    LOG_INFO("STATE", "nonexistent exists: %s",
             ozayn_state_exists(&state_mgr, "nonexistent") ? "yes" : "no");

    /* 10. Dirty tracking */
    LOG_INFO("STATE", "--- Demonstration: Dirty tracking ---");
    LOG_INFO("STATE", "Dirty count before update: %d", ozayn_state_dirty_count(&state_mgr));
    ozayn_state_update(&state_mgr, "core.log_level", "debug", 6);
    LOG_INFO("STATE", "Dirty count after update: %d", ozayn_state_dirty_count(&state_mgr));
    LOG_INFO("STATE", "core.log_level dirty: %s",
             ozayn_state_is_dirty(&state_mgr, "core.log_level") ? "yes" : "no");

    /* 11. Mark clean */
    ozayn_state_mark_clean(&state_mgr, "core.log_level");
    LOG_INFO("STATE", "core.log_level dirty after mark_clean: %s",
             ozayn_state_is_dirty(&state_mgr, "core.log_level") ? "yes" : "no");

    /* Re-mark dirty for save */
    ozayn_state_mark_dirty(&state_mgr, "core.log_level");

    /* 12. Update with version increment */
    LOG_INFO("STATE", "--- Demonstration: Update with version ---");
    int32_t new_interval = 5;
    ozayn_state_update(&state_mgr, "core.interval", &new_interval, sizeof(new_interval));
    const ozayn_state_entry_t *e_interval = ozayn_state_get(&state_mgr, "core.interval");
    if (e_interval) {
        LOG_INFO("STATE", "core.interval after update: v%u, data=%d",
                 e_interval->version, *(const int32_t *)e_interval->data);
    }

    /* 13. Seal state (read-only) */
    LOG_INFO("STATE", "--- Demonstration: Seal state ---");
    ozayn_state_update_sealed(&state_mgr, "security.enabled", 1);
    const ozayn_state_entry_t *e_sec = ozayn_state_get(&state_mgr, "security.enabled");
    if (e_sec) {
        LOG_INFO("STATE", "security.enabled sealed: %s",
                 (e_sec->flags & OZAYN_STATE_FLAG_SEALED) ? "yes" : "no");
    }
    /* Try to update sealed — should fail */
    int seal_result = ozayn_state_update(&state_mgr, "security.enabled", "no", 3);
    LOG_INFO("STATE", "Update sealed state: %s (expected: FAILED)",
             seal_result == 0 ? "OK" : "FAILED");
    /* Unseal */
    ozayn_state_update_sealed(&state_mgr, "security.enabled", 0);

    /* 14. Find by owner */
    LOG_INFO("STATE", "--- Demonstration: Find by owner ---");
    int owner_idx = 0;
    const ozayn_state_entry_t *owner_entry = ozayn_state_find_by_owner(&state_mgr, "core", &owner_idx);
    while (owner_entry) {
        LOG_INFO("STATE", "  Owner 'core': %s (v%u)", owner_entry->key, owner_entry->version);
        owner_entry = ozayn_state_find_by_owner(&state_mgr, "core", &owner_idx);
    }

    /* 15. Delete state */
    LOG_INFO("STATE", "--- Demonstration: Delete state ---");
    int del_result = ozayn_state_delete(&state_mgr, "core.runtime_state");
    LOG_INFO("STATE", "Delete core.runtime_state: %s", del_result == 0 ? "OK" : "FAILED");
    LOG_INFO("STATE", "Entries after delete: %d", ozayn_state_count(&state_mgr));

    /* Delete nonexistent — should fail */
    del_result = ozayn_state_delete(&state_mgr, "nonexistent");
    LOG_INFO("STATE", "Delete nonexistent: %s (expected: FAILED)",
             del_result == 0 ? "OK" : "FAILED");

    /* 16. Validation */
    LOG_INFO("STATE", "--- Demonstration: Validation ---");
    ozayn_state_validation_t val = ozayn_state_validate(&state_mgr);
    LOG_INFO("STATE", "Validation (before save): %s", ozayn_state_validation_name(val));

    /* 17. Save state */
    LOG_INFO("STATE", "--- Demonstration: Save state ---");
    int save_result = ozayn_state_save(&state_mgr);
    LOG_INFO("STATE", "Save: %s", save_result == 0 ? "OK" : "FAILED");

    /* 18. Validate after save */
    val = ozayn_state_validate(&state_mgr);
    LOG_INFO("STATE", "Validation (after save): %s", ozayn_state_validation_name(val));

    /* 19. Dirty count after save */
    LOG_INFO("STATE", "Dirty count after save: %d", ozayn_state_dirty_count(&state_mgr));

    /* 20. Backup verification */
    LOG_INFO("STATE", "--- Demonstration: Backup ---");
    LOG_INFO("STATE", "Backups created: %d", state_mgr.total_backups);

    /* 21. Save again — creates another backup */
    ozayn_state_mark_dirty(&state_mgr, "core.interval");
    ozayn_state_save(&state_mgr);
    LOG_INFO("STATE", "Backups after second save: %d", state_mgr.total_backups);

    /* 22. Statistics */
    LOG_INFO("STATE", "--- Demonstration: Statistics ---");
    ozayn_state_stats_t state_stats = ozayn_state_manager_stats(&state_mgr);
    LOG_INFO("STATE", "Total entries: %d", state_stats.total_entries);
    LOG_INFO("STATE", "  Persistent: %d", state_stats.persistent_entries);
    LOG_INFO("STATE", "  Transient: %d", state_stats.transient_entries);
    LOG_INFO("STATE", "  Recoverable: %d", state_stats.recoverable_entries);
    LOG_INFO("STATE", "  Dirty: %d", state_stats.dirty_entries);
    LOG_INFO("STATE", "Saves: %d, Loads: %d, Backups: %d",
             state_stats.total_saves, state_stats.total_loads, state_stats.total_backups);

    /* 23. Print all entries */
    LOG_INFO("STATE", "--- Demonstration: Print all entries ---");
    ozayn_state_manager_print_entries(&state_mgr);

    /* 24. Simulate reload: clear and load from file */
    LOG_INFO("STATE", "--- Demonstration: Reload from disk ---");
    /* Clear all entries in memory */
    for (int si = 0; si < OZAYN_STATE_MAX_ENTRIES; si++) {
        state_mgr.entries[si].active = 0;
    }
    state_mgr.entry_count = 0;
    state_mgr.dirty_count = 0;
    LOG_INFO("STATE", "Entries after clear: %d", ozayn_state_count(&state_mgr));

    /* Load from disk */
    int load_result = ozayn_state_load(&state_mgr);
    LOG_INFO("STATE", "Load from disk: %s (entries=%d)",
             load_result == 0 ? "OK" : "FAILED", ozayn_state_count(&state_mgr));

    /* Verify loaded data */
    const ozayn_state_entry_t *loaded = ozayn_state_get(&state_mgr, "core.log_level");
    if (loaded) {
        LOG_INFO("STATE", "Loaded core.log_level: v%u, data='%s'",
                 loaded->version, (const char *)loaded->data);
    }

    /* 25. Recovery test: corrupt and recover */
    LOG_INFO("STATE", "--- Demonstration: Recovery from backup ---");
    LOG_INFO("STATE", "Recovery attempts: %d", state_mgr.recoveries_attempted);

    /* 26. Delete and re-create for clean shutdown */
    ozayn_state_delete(&state_mgr, "core.interval");
    ozayn_state_delete(&state_mgr, "security.enabled");
    ozayn_state_delete(&state_mgr, "plugins.registry");
    ozayn_state_delete(&state_mgr, "tasks.definition.1");
    ozayn_state_delete(&state_mgr, "core.log_level");

    /* 27. Final save */
    ozayn_state_save(&state_mgr);

    /* 28. Process state events */
    ozayn_events_process(&events);

    /* 29. STATE STATUS command */
    LOG_INFO("STATE", "--- Demonstration: STATE STATUS command ---");
    ozayn_command_t cmd_state_status = ozayn_command_create(OZAYN_CMD_STATE_STATUS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_state_status);

    /* 30. STATE INFO command */
    LOG_INFO("STATE", "--- Demonstration: STATE INFO command ---");
    ozayn_command_t cmd_state_info = ozayn_command_create(OZAYN_CMD_STATE_INFO, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_state_info);

    /* --- End Persistence & State Management demonstration --- */

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
    ozayn_state_manager_shutdown(&state_mgr);
    ozayn_security_boundary_shutdown(&sec_bnd_mgr);
    ozayn_diagnostics_shutdown(&diag_mgr);
    ozayn_monitoring_shutdown(&mon_mgr);
    ozayn_scheduler_shutdown(&sched_mgr);
    ozayn_resource_manager_shutdown(&res_mgr);
    ozayn_authorization_shutdown(&authz_mgr);
    ozayn_security_shutdown(&sec_mgr);
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
