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

    /* 4. STOP command — triggers shutdown */
    LOG_INFO("COMMANDS", "--- Demonstration: STOP command ---");
    ozayn_command_t cmd_stop = ozayn_command_create(OZAYN_CMD_STOP, OZAYN_CMD_SRC_CLI);
    ozayn_command_result_t r_stop = ozayn_command_engine_execute(&cmd_engine, &cmd_stop);
    LOG_INFO("COMMANDS", "STOP result: %s (status=%s)",
             ozayn_command_result_name(r_stop),
             ozayn_command_status_name(cmd_stop.status));

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

    /* Process process events */
    ozayn_events_process(&events);

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
    ozayn_process_manager_shutdown(&proc_mgr);
    ozayn_task_manager_shutdown(&task_mgr);
    ozayn_command_engine_shutdown(&cmd_engine);
    ozayn_events_shutdown(&events);
    ozayn_logger_shutdown(&logger);
    ozayn_config_destroy(&cfg);

    return 0;
}
