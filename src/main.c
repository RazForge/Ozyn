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
    LOG_INFO("LOGGER_TEST", "Module started");
    return OZAYN_OK;
}
static void logger_test_stop(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module stopped");
}
static void logger_test_shutdown(void *engine) {
    (void)engine;
    LOG_INFO("LOGGER_TEST", "Module shut down");
}
static const ozayn_module_entry_t logger_test_module = {
    .name = "logger_test", .version = "0.1",
    .description = "Test module",
    .init = logger_test_init, .start = logger_test_start,
    .stop = logger_test_stop, .shutdown = logger_test_shutdown,
};

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* ================================================================
     * PHASE 1: BOOTSTRAP
     * ================================================================ */

    ozayn_recovery_t recovery;
    ozayn_recovery_init(&recovery);

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

    /* Initialize logger */
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

    LOG_INFO("CORE", "=== PHASE 1: BOOTSTRAP ===");
    LOG_INFO("CORE", "OZAYN Core v%s starting", OZAYN_VERSION);

    /* ================================================================
     * PHASE 2: FOUNDATION
     * ================================================================ */

    LOG_INFO("CORE", "=== PHASE 2: FOUNDATION ===");

    ozayn_event_engine_t events;
    ozayn_event_config_t ecfg = { .queue_capacity = 256, .max_subscribers = 32 };
    if (ozayn_events_init(&events, &ecfg) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize event engine");
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }
    ozayn_events_subscribe(&events, OZAYN_EVENT_NONE, on_event, NULL);

    ozayn_command_engine_t cmd_engine;
    if (ozayn_command_engine_init(&cmd_engine) != OZAYN_OK) {
        LOG_CRITICAL("CORE", "Failed to initialize command engine");
        ozayn_events_shutdown(&events);
        ozayn_logger_shutdown(&logger);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* ================================================================
     * PHASE 3: SECURITY
     * ================================================================ */

    LOG_INFO("CORE", "=== PHASE 3: SECURITY ===");

    ozayn_security_manager_t sec_mgr;
    ozayn_security_init(&sec_mgr, cfg.values.security_enabled);
    ozayn_security_set_events(&sec_mgr, &events);
    ozayn_security_set_recovery(&sec_mgr, &recovery);
    ozayn_security_set_auth_mode(&sec_mgr, (ozayn_auth_method_t)cfg.values.security_auth_mode);
    ozayn_security_set_audit_logging(&sec_mgr, cfg.values.security_audit_log);

    uid_t current_uid = getuid();
    ozayn_security_set_allowed_uid(&sec_mgr, (uint32_t)current_uid);
    ozayn_security_register_identity(&sec_mgr, "ozayn.ai", "OZAYN AI Service",
                                     OZAYN_IDENTITY_SERVICE, OZAYN_AUTH_UID, (uint32_t)current_uid, 0);
    ozayn_security_register_identity(&sec_mgr, "ozayn.vision", "OZAYN Vision Service",
                                     OZAYN_IDENTITY_SERVICE, OZAYN_AUTH_UID, (uint32_t)current_uid, 0);

    ozayn_authorization_manager_t authz_mgr;
    ozayn_authorization_init(&authz_mgr, cfg.values.security_enabled);
    ozayn_authorization_set_security(&authz_mgr, &sec_mgr);
    ozayn_authorization_set_events(&authz_mgr, &events);
    ozayn_authorization_set_recovery(&authz_mgr, &recovery);
    ozayn_authorization_set_audit_logging(&authz_mgr, cfg.values.security_audit_log);
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.vision", "VISION_SERVICE");
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.ai", "SERVICE_BASE");
    ozayn_authorization_assign_role(&authz_mgr, "ozayn.core", "RESOURCE_ADMIN");

    ozayn_security_boundary_manager_t sec_bnd_mgr;
    ozayn_security_boundary_init(&sec_bnd_mgr, cfg.values.security_enabled);
    sec_bnd_mgr.events = &events;
    sec_bnd_mgr.recovery = &recovery;

    /* ================================================================
     * PHASE 4: RUNTIME SERVICES
     * ================================================================ */

    LOG_INFO("CORE", "=== PHASE 4: RUNTIME SERVICES ===");

    ozayn_task_manager_t task_mgr;
    ozayn_task_manager_init(&task_mgr);
    ozayn_task_manager_set_events(&task_mgr, &events);
    ozayn_task_manager_set_recovery(&task_mgr, &recovery);

    ozayn_process_manager_t proc_mgr;
    ozayn_process_manager_init(&proc_mgr);
    ozayn_process_manager_set_events(&proc_mgr, &events);
    ozayn_process_manager_set_recovery(&proc_mgr, &recovery);

    ozayn_resource_manager_t res_mgr;
    ozayn_resource_manager_init(&res_mgr, cfg.values.security_enabled);
    ozayn_resource_manager_set_events(&res_mgr, &events);
    ozayn_resource_manager_set_recovery(&res_mgr, &recovery);
    ozayn_resource_manager_set_authorization(&res_mgr, &authz_mgr);

    ozayn_scheduler_manager_t sched_mgr;
    ozayn_scheduler_init(&sched_mgr, cfg.values.security_enabled);
    ozayn_scheduler_set_task_mgr(&sched_mgr, &task_mgr);
    ozayn_scheduler_set_events(&sched_mgr, &events);
    ozayn_scheduler_set_recovery(&sched_mgr, &recovery);
    ozayn_scheduler_set_resource_mgr(&sched_mgr, &res_mgr);
    ozayn_scheduler_set_aging(&sched_mgr, 1);
    ozayn_scheduler_set_max_tasks_per_source(&sched_mgr, 16);

    ozayn_ipc_config_t ipc_cfg = {
        .enabled = cfg.values.ipc_enabled,
        .max_msg_size = cfg.values.ipc_max_msg_size,
        .max_connections = cfg.values.ipc_max_connections,
    };
    snprintf(ipc_cfg.endpoint, sizeof(ipc_cfg.endpoint), "%s",
             cfg.values.ipc_endpoint[0] ? cfg.values.ipc_endpoint : "runtime/ipc/ozayn.sock");
    ozayn_ipc_manager_t ipc_mgr;
    ozayn_ipc_manager_init(&ipc_mgr, &ipc_cfg);
    ozayn_ipc_manager_set_events(&ipc_mgr, &events);
    ozayn_ipc_manager_set_recovery(&ipc_mgr, &recovery);
    ozayn_ipc_manager_set_security(&ipc_mgr, &sec_mgr);
    ozayn_ipc_manager_set_authorization(&ipc_mgr, &authz_mgr);

    ozayn_registry_manager_t reg_mgr;
    ozayn_registry_init(&reg_mgr, cfg.values.registry_enabled);
    ozayn_registry_set_events(&reg_mgr, &events);
    ozayn_registry_set_recovery(&reg_mgr, &recovery);

    /* ================================================================
     * PHASE 5: MODULES & PLUGINS
     * ================================================================ */

    LOG_INFO("CORE", "=== PHASE 5: MODULES & PLUGINS ===");

    ozayn_module_manager_t mod_mgr;
    ozayn_module_manager_init(&mod_mgr);
    ozayn_module_manager_set_logger(&mod_mgr, &logger);
    ozayn_module_manager_set_events(&mod_mgr, &events);
    ozayn_module_manager_set_recovery(&mod_mgr, &recovery);
    ozayn_module_manager_set_config(&mod_mgr, &cfg);

    ozayn_plugin_manager_t plug_mgr;
    ozayn_plugin_manager_init(&plug_mgr);
    ozayn_plugin_manager_set_logger(&plug_mgr, &logger);
    ozayn_plugin_manager_set_events(&plug_mgr, &events);
    ozayn_plugin_manager_set_recovery(&plug_mgr, &recovery);
    ozayn_plugin_manager_set_config(&plug_mgr, &cfg);

    /* ================================================================
     * PHASE 6: MONITORING
     * ================================================================ */

    LOG_INFO("CORE", "=== PHASE 6: MONITORING ===");

    ozayn_monitoring_manager_t mon_mgr;
    ozayn_monitoring_init(&mon_mgr, cfg.values.security_enabled);
    ozayn_monitoring_set_events(&mon_mgr, &events);
    ozayn_monitoring_set_recovery(&mon_mgr, &recovery);

    ozayn_diagnostics_manager_t diag_mgr;
    ozayn_diagnostics_init(&diag_mgr, cfg.values.security_enabled);
    ozayn_diagnostics_set_events(&diag_mgr, &events);
    ozayn_diagnostics_set_recovery(&diag_mgr, &recovery);
    ozayn_diagnostics_set_monitoring(&diag_mgr, &mon_mgr);

    ozayn_state_manager_t state_mgr;
    ozayn_state_manager_init(&state_mgr, cfg.values.security_enabled);
    char state_path[512];
    snprintf(state_path, sizeof(state_path), "%s/ozayn.state",
             cfg.values.log_directory[0] ? cfg.values.log_directory : "data");
    ozayn_state_manager_set_storage_path(&state_mgr, state_path);
    state_mgr.events = &events;
    state_mgr.recovery = &recovery;

    /* ================================================================
     * LIFECYCLE COORDINATOR — register all components
     * ================================================================ */

    LOG_INFO("CORE", "=== LIFECYCLE COORDINATOR ===");

    ozayn_lc_t lifecycle;
    ozayn_lc_config_t lc_cfg = {
        .startup_timeout_ms   = 30000,
        .component_timeout_ms = 5000,
        .shutdown_timeout_ms  = 15000,
    };
    ozayn_lc_init(&lifecycle, &lc_cfg);
    ozayn_lc_set_events(&lifecycle, &events);
    ozayn_lc_set_recovery(&lifecycle, &recovery);

    /* Register foundation components */
    ozayn_lc_register_simple(&lifecycle, "EventEngine", OZAYN_LC_PHASE_FOUNDATION,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &events);
    ozayn_lc_register_simple(&lifecycle, "CommandEngine", OZAYN_LC_PHASE_FOUNDATION,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &cmd_engine);

    /* Register security components */
    ozayn_lc_register_simple(&lifecycle, "Security", OZAYN_LC_PHASE_SECURITY,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &sec_mgr);
    ozayn_lc_register_simple(&lifecycle, "Authorization", OZAYN_LC_PHASE_SECURITY,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &authz_mgr);
    ozayn_lc_register_simple(&lifecycle, "SecurityBoundary", OZAYN_LC_PHASE_SECURITY,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &sec_bnd_mgr);

    /* Register runtime services */
    ozayn_lc_register_simple(&lifecycle, "TaskManager", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &task_mgr);
    ozayn_lc_register_simple(&lifecycle, "ProcessManager", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &proc_mgr);
    ozayn_lc_register_simple(&lifecycle, "ResourceManager", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &res_mgr);
    ozayn_lc_register_simple(&lifecycle, "Scheduler", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &sched_mgr);
    ozayn_lc_register_simple(&lifecycle, "IPCManager", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_OPTIONAL, NULL, &ipc_mgr);
    ozayn_lc_register_simple(&lifecycle, "ServiceRegistry", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                             OZAYN_LC_CRITICALITY_OPTIONAL, NULL, &reg_mgr);

    /* Register modules & plugins */
    ozayn_lc_register_simple(&lifecycle, "ModuleManager", OZAYN_LC_PHASE_MODULES_PLUGINS,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &mod_mgr);
    ozayn_lc_register_simple(&lifecycle, "PluginManager", OZAYN_LC_PHASE_MODULES_PLUGINS,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &plug_mgr);

    /* Register monitoring */
    ozayn_lc_register_simple(&lifecycle, "Monitoring", OZAYN_LC_PHASE_MONITORING,
                             OZAYN_LC_CRITICALITY_REQUIRED, NULL, &mon_mgr);
    ozayn_lc_register_simple(&lifecycle, "Diagnostics", OZAYN_LC_PHASE_MONITORING,
                             OZAYN_LC_CRITICALITY_OPTIONAL, NULL, &diag_mgr);
    ozayn_lc_register_simple(&lifecycle, "StateManager", OZAYN_LC_PHASE_MONITORING,
                             OZAYN_LC_CRITICALITY_OPTIONAL, NULL, &state_mgr);

    /* Bind lifecycle to runtime */
    ozayn_runtime_t *rt = ozayn_runtime_create();
    ozayn_runtime_set_config(rt, &cfg.values);
    ozayn_runtime_set_events(rt, &events);
    ozayn_runtime_set_lifecycle_mgr(rt, &lifecycle);

    /* Bind runtime, events, recovery to command engine */
    ozayn_command_engine_set_runtime(&cmd_engine, rt);
    ozayn_command_engine_set_events(&cmd_engine, &events);
    ozayn_command_engine_set_recovery(&cmd_engine, &recovery);

    /* ================================================================
     * STARTUP — run coordinator
     * ================================================================ */

    LOG_INFO("CORE", "=== STARTING OZAYN CORE ===");

    /* Mark all registered components as initialized (since we initialized above) */
    for (int i = 0; i < lifecycle.component_count; i++) {
        lifecycle.components[i].state = OZAYN_LC_COMP_INITIALIZED;
        lifecycle.components[i].init_result = 0;
    }

    /* Start components that have start functions */
    for (int i = 0; i < lifecycle.component_count; i++) {
        ozayn_lc_component_t *c = &lifecycle.components[i];
        if (c->state == OZAYN_LC_COMP_INITIALIZED) {
            c->state = OZAYN_LC_COMP_STARTED;
            c->start_time = time(NULL);
            lifecycle.components_started++;
        }
    }

    /* Run the startup coordinator (validates phases, readiness) */
    ozayn_lc_startup(&lifecycle);

    /* Publish startup events */
    ozayn_events_publish(&events, OZAYN_EVENT_CONFIG_LOADED, OZAYN_SRC_CONFIG, NULL);
    ozayn_events_publish(&events, OZAYN_EVENT_LOGGER_READY, OZAYN_SRC_LOGGER, NULL);
    ozayn_events_publish(&events, OZAYN_EVENT_RUNTIME_STARTED, OZAYN_SRC_RUNTIME, NULL);
    ozayn_events_process(&events);

    /* Register test modules */
    ozayn_module_manager_register(&mod_mgr, &logger_test_module);
    ozayn_module_manager_init_all(&mod_mgr);
    ozayn_module_manager_start_all(&mod_mgr);

    /* Load plugins */
    const char *pdir = cfg.values.plugin_dir[0] ? cfg.values.plugin_dir : "plugins";
    ozayn_plugin_manager_discover(&plug_mgr, pdir);
    char test_path[512];
    snprintf(test_path, sizeof(test_path), "%s/test_plugin.so", pdir);
    ozayn_plugin_manager_load(&plug_mgr, test_path);
    ozayn_plugin_manager_init_all(&plug_mgr);
    ozayn_plugin_manager_start_all(&plug_mgr);

    /* Register services */
    ozayn_service_registration_t core_reg;
    memset(&core_reg, 0, sizeof(core_reg));
    snprintf(core_reg.id, sizeof(core_reg.id), "ozayn.core");
    snprintf(core_reg.name, sizeof(core_reg.name), "OZAYN Core");
    snprintf(core_reg.version, sizeof(core_reg.version), "0.1");
    core_reg.protocol_version = OZAYN_IPC_VERSION;
    snprintf(core_reg.endpoint, sizeof(core_reg.endpoint), "runtime/ipc/ozayn.sock");
    snprintf(core_reg.provider, sizeof(core_reg.provider), "core");
    snprintf(core_reg.capabilities[0], sizeof(core_reg.capabilities[0]), "system-management");
    core_reg.capability_count = 1;
    ozayn_registry_register(&reg_mgr, &core_reg, -1);

    /* Register monitoring metrics */
    ozayn_monitoring_register_metric(&mon_mgr, "task.total_created", OZAYN_METRIC_COUNTER, OZAYN_COMP_TASK_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "task.active", OZAYN_METRIC_GAUGE, OZAYN_COMP_TASK_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "resource.total", OZAYN_METRIC_GAUGE, OZAYN_COMP_RESOURCE_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "process.active", OZAYN_METRIC_GAUGE, OZAYN_COMP_PROCESS_MANAGER);
    ozayn_monitoring_register_metric(&mon_mgr, "event.queue_depth", OZAYN_METRIC_GAUGE, OZAYN_COMP_EVENT_ENGINE);

    /* Security contexts for boundary */
    uint32_t ctx_core = ozayn_security_boundary_register_context(&sec_bnd_mgr, "ozayn.core", OZAYN_SB_TRUST_CORE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_SECURITY_ADMIN);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_CORE_SHUTDOWN);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_TASK_CREATE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_CONFIG_WRITE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_core, OZAYN_CAP_PROCESS_STOP);

    uint32_t ctx_vision = ozayn_security_boundary_register_context(&sec_bnd_mgr, "plugin.vision", OZAYN_SB_TRUST_LIMITED);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_CAMERA_READ);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_IPC_SEND);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_TASK_CREATE);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_METRICS_READ);

    uint32_t ctx_unknown = ozayn_security_boundary_register_context(&sec_bnd_mgr, "plugin.unknown", OZAYN_SB_TRUST_UNTRUSTED);
    ozayn_security_boundary_grant_capability(&sec_bnd_mgr, ctx_unknown, OZAYN_CAP_METRICS_READ);

    ozayn_events_process(&events);

    /* ================================================================
     * DEMONSTRATIONS
     * ================================================================ */

    /* 1. Lifecycle status */
    LOG_INFO("DEMO", "--- Demonstration: Lifecycle status ---");
    ozayn_lc_print_status(&lifecycle);
    ozayn_lc_print_components(&lifecycle);

    /* 2. Phase/state queries */
    LOG_INFO("DEMO", "--- Demonstration: Phase & state queries ---");
    LOG_INFO("DEMO", "Current phase: %s", ozayn_lc_phase_name(ozayn_lc_get_phase(&lifecycle)));
    LOG_INFO("DEMO", "Is running: %s", ozayn_lc_is_running(&lifecycle) ? "yes" : "no");
    LOG_INFO("DEMO", "Component count: %d", ozayn_lc_component_count(&lifecycle));
    LOG_INFO("DEMO", "Components by state (INITIALIZED): %d",
             ozayn_lc_component_count_by_state(&lifecycle, OZAYN_LC_COMP_INITIALIZED));
    LOG_INFO("DEMO", "Components by state (STARTED): %d",
             ozayn_lc_component_count_by_state(&lifecycle, OZAYN_LC_COMP_STARTED));
    LOG_INFO("DEMO", "Components by criticality (REQUIRED): %d",
             ozayn_lc_component_count_by_criticality(&lifecycle, OZAYN_LC_CRITICALITY_REQUIRED));
    LOG_INFO("DEMO", "Components by criticality (OPTIONAL): %d",
             ozayn_lc_component_count_by_criticality(&lifecycle, OZAYN_LC_CRITICALITY_OPTIONAL));

    /* 3. Component lookup */
    LOG_INFO("DEMO", "--- Demonstration: Component lookup ---");
    const ozayn_lc_component_t *comp = ozayn_lc_get_component(&lifecycle, "Scheduler");
    if (comp) {
        LOG_INFO("DEMO", "Found 'Scheduler': phase=%s state=%s criticality=%s",
                 ozayn_lc_phase_name(comp->phase),
                 ozayn_lc_component_state_name(comp->state),
                 ozayn_lc_criticality_name(comp->criticality));
    }
    const ozayn_lc_component_t *not_found = ozayn_lc_get_component(&lifecycle, "Nonexistent");
    LOG_INFO("DEMO", "Lookup 'Nonexistent': %s", not_found ? "found" : "NOT FOUND");

    /* 4. Readiness check */
    LOG_INFO("DEMO", "--- Demonstration: Readiness check ---");
    int ready = ozayn_lc_readiness_check(&lifecycle);
    LOG_INFO("DEMO", "Readiness: %s", ready ? "PASS" : "FAIL");

    /* 5. Print startup log */
    LOG_INFO("DEMO", "--- Demonstration: Startup log ---");
    ozayn_lc_print_startup_log(&lifecycle);

    /* 6. Lifecycle events */
    LOG_INFO("DEMO", "--- Demonstration: Lifecycle events ---");
    LOG_INFO("DEMO", "LC_INIT_BEGAN = %d", OZAYN_LC_EVENT_INIT_BEGAN);
    LOG_INFO("DEMO", "LC_ONLINE = %d", OZAYN_LC_EVENT_ONLINE);
    LOG_INFO("DEMO", "LC_SHUTDOWN_REQUESTED = %d", OZAYN_LC_EVENT_SHUTDOWN_REQUESTED);
    LOG_INFO("DEMO", "LC_STATE_CHANGED = %d", OZAYN_LC_EVENT_STATE_CHANGED);
    ozayn_events_process(&events);

    /* 7. LC STATUS command */
    LOG_INFO("DEMO", "--- Demonstration: LC STATUS command ---");
    ozayn_command_t cmd_lc_status = ozayn_command_create(OZAYN_CMD_LC_STATUS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_lc_status);

    /* 8. Tasks */
    LOG_INFO("DEMO", "--- Demonstration: Tasks ---");
    ozayn_task_t *task1 = ozayn_task_manager_submit(&task_mgr, OZAYN_TASK_DEMO, OZAYN_TASK_SRC_CORE);
    if (task1) {
        LOG_INFO("DEMO", "Task #%u created: state=%s", task1->id,
                 ozayn_task_state_name(task1->state));
    }
    ozayn_task_t *task2 = ozayn_task_manager_submit(&task_mgr, OZAYN_TASK_DEMO, OZAYN_TASK_SRC_CORE);
    if (task2) {
        ozayn_scheduler_submit(&sched_mgr, task2->id, OZAYN_SCHED_PRIORITY_LOW, "core");
    }
    ozayn_scheduler_tick(&sched_mgr);
    LOG_INFO("DEMO", "Active tasks: %d", ozayn_task_manager_active_count(&task_mgr));

    /* 9. Scheduler */
    LOG_INFO("DEMO", "--- Demonstration: Scheduler ---");
    ozayn_task_t *sched_task = ozayn_task_manager_submit(&task_mgr, OZAYN_TASK_DEMO, OZAYN_TASK_SRC_CORE);
    if (sched_task) {
        ozayn_scheduler_submit(&sched_mgr, sched_task->id, OZAYN_SCHED_PRIORITY_CRITICAL, "core");
    }
    ozayn_sched_stats_t sched_stats = ozayn_scheduler_stats(&sched_mgr);
    LOG_INFO("DEMO", "Scheduler: submitted=%d, executed=%d, completed=%d",
             sched_stats.total_submitted, sched_stats.total_executed, sched_stats.total_completed);

    /* 10. Resources */
    LOG_INFO("DEMO", "--- Demonstration: Resources ---");
    ozayn_resource_result_t rr;
    rr = ozayn_resource_create(&res_mgr, "camera-01", "Primary Camera",
                               OZAYN_RESOURCE_TYPE_DEVICE, 1);
    LOG_INFO("DEMO", "Create camera-01: %s", ozayn_resource_result_name(rr));
    rr = ozayn_resource_allocate(&res_mgr, "camera-01", "ozayn.vision");
    LOG_INFO("DEMO", "Allocate camera-01: %s", ozayn_resource_result_name(rr));
    rr = ozayn_resource_activate(&res_mgr, "camera-01", "ozayn.vision");
    LOG_INFO("DEMO", "Activate camera-01: %s", ozayn_resource_result_name(rr));
    ozayn_resource_stats_t res_stats = ozayn_resource_manager_stats(&res_mgr);
    LOG_INFO("DEMO", "Resources: total=%d available=%d allocated=%d",
             res_stats.total, res_stats.available, res_stats.allocated);

    /* 11. Security */
    LOG_INFO("DEMO", "--- Demonstration: Security ---");
    ozayn_peer_creds_t creds = { .uid = (uint32_t)current_uid, .gid = (uint32_t)getgid(),
                                  .pid = (uint32_t)getpid(), .valid = 1 };
    ozayn_auth_result_t auth_r = ozayn_security_authenticate(&sec_mgr, "ozayn.core", &creds);
    LOG_INFO("DEMO", "Auth ozayn.core: %s", ozayn_auth_result_name(auth_r));
    ozayn_auth_result_t auth_r2 = ozayn_security_authenticate(&sec_mgr, "ozayn.vision", &creds);
    LOG_INFO("DEMO", "Auth ozayn.vision: %s", ozayn_auth_result_name(auth_r2));

    /* 12. Authorization */
    LOG_INFO("DEMO", "--- Demonstration: Authorization ---");
    ozayn_authz_result_t az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "camera", "read");
    LOG_INFO("DEMO", "ozayn.vision camera.read -> %s", ozayn_authz_decision_name(az_r.decision));
    az_r = ozayn_authorize(&authz_mgr, "ozayn.vision", "core", "shutdown");
    LOG_INFO("DEMO", "ozayn.vision core.shutdown -> %s", ozayn_authz_decision_name(az_r.decision));

    /* 13. Security boundary */
    LOG_INFO("DEMO", "--- Demonstration: Security boundary ---");
    ozayn_security_check_result_t check1 = ozayn_security_boundary_check(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_CAMERA_READ);
    LOG_INFO("DEMO", "plugin.vision camera.read -> %s", check1.allowed ? "ALLOWED" : "DENIED");
    ozayn_security_check_result_t check2 = ozayn_security_boundary_check(&sec_bnd_mgr, ctx_vision, OZAYN_CAP_SECURITY_ADMIN);
    LOG_INFO("DEMO", "plugin.vision security.admin -> %s (expected: DENIED)",
             check2.allowed ? "ALLOWED" : "DENIED");
    ozayn_security_boundary_stats_t sec_stats = ozayn_security_boundary_stats(&sec_bnd_mgr);
    LOG_INFO("DEMO", "Boundary checks: %d (allowed=%d, denied=%d)",
             sec_stats.total_checks, sec_stats.total_allowed, sec_stats.total_denied);

    /* 14. Monitoring */
    LOG_INFO("DEMO", "--- Demonstration: Monitoring ---");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_SCHEDULER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO, "operational");
    ozayn_monitoring_report_health(&mon_mgr, OZAYN_COMP_RESOURCE_MANAGER,
                                    OZAYN_HEALTH_HEALTHY, OZAYN_SEVERITY_INFO, "operational");
    ozayn_health_state_t overall = ozayn_monitoring_overall_health(&mon_mgr);
    LOG_INFO("DEMO", "Overall health: %s", ozayn_health_state_name(overall));
    ozayn_monitor_stats_t mon_stats = ozayn_monitoring_stats(&mon_mgr);
    LOG_INFO("DEMO", "Monitoring: checks=%d health_changes=%d",
             mon_stats.total_checks, mon_stats.health_changes);

    /* 15. Diagnostics */
    LOG_INFO("DEMO", "--- Demonstration: Diagnostics ---");
    ozayn_diagnostics_set_level(&diag_mgr, OZAYN_DIAG_LEVEL_DETAILED);
    LOG_INFO("DEMO", "Diagnostics level: %s",
             ozayn_diag_level_name(ozayn_diagnostics_get_level(&diag_mgr)));
    uint32_t ev1 = ozayn_diagnostics_record_evidence(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                                       OZAYN_DIAG_TARGET_IPC,
                                                       "REQ-1001", "IPC queue latency above threshold");
    LOG_INFO("DEMO", "Evidence recorded: #%u", ev1);
    ozayn_diagnostics_add_finding(&diag_mgr, OZAYN_DIAG_COMP_IPC,
                                   OZAYN_DIAG_SEV_WARNING, "REQ-1001",
                                   "IPC queue latency above threshold",
                                   "workload spike", OZAYN_DIAG_CONFIDENCE_HIGH);
    LOG_INFO("DEMO", "Findings: %d", ozayn_diagnostics_finding_count(&diag_mgr));
    ozayn_diag_stats_t diag_stats = ozayn_diagnostics_stats(&diag_mgr);
    LOG_INFO("DEMO", "Diagnostics: evidence=%d findings=%d timeline=%d",
             diag_stats.evidence_recorded, diag_stats.findings_generated, diag_stats.timeline_entries);

    /* 16. State management */
    LOG_INFO("DEMO", "--- Demonstration: State management ---");
    const char *log_level = "detailed";
    uint32_t sid = ozayn_state_create(&state_mgr, "core.log_level", "core",
                                      OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                                      OZAYN_STATE_RECOVER_ON_RESTART,
                                      log_level, (uint32_t)strlen(log_level) + 1);
    LOG_INFO("DEMO", "State create: id=%u", sid);
    int save_r = ozayn_state_save(&state_mgr);
    LOG_INFO("DEMO", "State save: %s", save_r == 0 ? "OK" : "FAILED");
    ozayn_state_stats_t state_stats = ozayn_state_manager_stats(&state_mgr);
    LOG_INFO("DEMO", "State: entries=%d persistent=%d saves=%d",
             state_stats.total_entries, state_stats.persistent_entries, state_stats.total_saves);

    /* 17. Process management */
    LOG_INFO("DEMO", "--- Demonstration: Processes ---");
    char *echo_argv[] = { "echo", "Hello from OZAYN!", NULL };
    ozayn_process_t *proc1 = ozayn_process_manager_create(&proc_mgr, "/usr/bin/echo", echo_argv);
    if (proc1) {
        LOG_INFO("DEMO", "Process #%u: PID %d, state=%s",
                 proc1->id, (int)proc1->pid, ozayn_process_state_name(proc1->state));
    }
    ozayn_process_manager_reap(&proc_mgr);
    int active_procs = ozayn_process_manager_active_count(&proc_mgr);
    LOG_INFO("DEMO", "Active processes: %d", active_procs);

    /* 18. Commands */
    LOG_INFO("DEMO", "--- Demonstration: Commands ---");
    ozayn_command_t cmd_status = ozayn_command_create(OZAYN_CMD_STATUS, OZAYN_CMD_SRC_CORE);
    ozayn_command_result_t cmd_r = ozayn_command_engine_execute(&cmd_engine, &cmd_status);
    LOG_INFO("DEMO", "STATUS: %s", ozayn_command_result_name(cmd_r));
    ozayn_command_t cmd_health = ozayn_command_create(OZAYN_CMD_HEALTH, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_health);
    ozayn_command_t cmd_sec_status = ozayn_command_create(OZAYN_CMD_SEC_STATUS, OZAYN_CMD_SRC_CLI);
    ozayn_command_engine_execute(&cmd_engine, &cmd_sec_status);

    /* 19. Plugin queries */
    LOG_INFO("DEMO", "--- Demonstration: Plugins ---");
    int active_plugs = ozayn_plugin_manager_active_count(&plug_mgr);
    LOG_INFO("DEMO", "Active plugins: %d", active_plugs);

    /* 20. Module queries */
    LOG_INFO("DEMO", "--- Demonstration: Modules ---");
    int active_mods = ozayn_module_manager_active_count(&mod_mgr);
    LOG_INFO("DEMO", "Active modules: %d", active_mods);

    /* 21. Service registry */
    LOG_INFO("DEMO", "--- Demonstration: Service registry ---");
    const ozayn_service_record_t *svc = ozayn_registry_lookup(&reg_mgr, "ozayn.core");
    if (svc) {
        LOG_INFO("DEMO", "Service 'ozayn.core' v%s state=%s",
                 svc->version, ozayn_service_state_name(svc->state));
    }

    /* 22. Error recovery */
    LOG_INFO("DEMO", "--- Demonstration: Error recovery ---");
    LOG_INFO("DEMO", "Total errors: %d", recovery.total_errors);

    /* 23. Process events */
    ozayn_events_process(&events);

    /* ================================================================
     * SHUTDOWN — through lifecycle coordinator
     * ================================================================ */

    LOG_INFO("CORE", "=== SHUTTING DOWN OZAYN CORE ===");

    /* Set stop flag for runtime loop */
    ozayn_runtime_set_stop_flag(rt, &g_stop);

    /* Request shutdown through lifecycle coordinator */
    ozayn_lc_request_shutdown(&lifecycle, OZAYN_LC_SHUTDOWN_USER_REQUEST);

    /* Publish shutdown events */
    ozayn_events_publish(&events, OZAYN_EVENT_RUNTIME_STOPPING, OZAYN_SRC_RUNTIME, NULL);
    ozayn_events_process(&events);

    /* Shutdown managers in reverse phase order (lifecycle already did its part) */
    ozayn_state_manager_shutdown(&state_mgr);
    ozayn_diagnostics_shutdown(&diag_mgr);
    ozayn_monitoring_shutdown(&mon_mgr);
    ozayn_plugin_manager_shutdown(&plug_mgr);
    ozayn_module_manager_shutdown(&mod_mgr);
    ozayn_scheduler_shutdown(&sched_mgr);
    ozayn_resource_manager_shutdown(&res_mgr);
    ozayn_authorization_shutdown(&authz_mgr);
    ozayn_security_shutdown(&sec_mgr);
    ozayn_security_boundary_shutdown(&sec_bnd_mgr);
    ozayn_ipc_manager_shutdown(&ipc_mgr);
    ozayn_registry_shutdown(&reg_mgr);
    ozayn_process_manager_shutdown(&proc_mgr);
    ozayn_task_manager_shutdown(&task_mgr);
    ozayn_command_engine_shutdown(&cmd_engine);
    ozayn_events_shutdown(&events);
    ozayn_logger_shutdown(&logger);
    ozayn_config_destroy(&cfg);

    ozayn_lc_shutdown(&lifecycle);
    ozayn_runtime_destroy(rt);

    printf("\n  OZAYN Core shutdown complete.\n\n");
    return 0;
}
