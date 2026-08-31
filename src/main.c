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

    /* ================================================================
     * DEPENDENCY MANAGER — register component dependencies
     * ================================================================ */

    LOG_INFO("CORE", "=== DEPENDENCY MANAGER ===");

    ozayn_dep_manager_t dep_mgr;
    ozayn_dep_config_t dep_cfg = {
        .resolve_timeout_ms = 5000,
        .fail_on_cycle      = 1,
        .fail_on_missing    = 0,
    };
    ozayn_dep_init(&dep_mgr, &dep_cfg);
    ozayn_dep_set_events(&dep_mgr, &events);

    /* Register dependency nodes (one per core component) */
    ozayn_dep_register_simple(&dep_mgr, "EventEngine");
    ozayn_dep_register_simple(&dep_mgr, "CommandEngine");
    ozayn_dep_register_simple(&dep_mgr, "Security");
    ozayn_dep_register_simple(&dep_mgr, "Authorization");
    ozayn_dep_register_simple(&dep_mgr, "SecurityBoundary");
    ozayn_dep_register_simple(&dep_mgr, "TaskManager");
    ozayn_dep_register_simple(&dep_mgr, "ProcessManager");
    ozayn_dep_register_simple(&dep_mgr, "ResourceManager");
    ozayn_dep_register_simple(&dep_mgr, "Scheduler");
    ozayn_dep_register_simple(&dep_mgr, "IPCManager");
    ozayn_dep_register_simple(&dep_mgr, "ServiceRegistry");
    ozayn_dep_register_simple(&dep_mgr, "ModuleManager");
    ozayn_dep_register_simple(&dep_mgr, "PluginManager");
    ozayn_dep_register_simple(&dep_mgr, "Monitoring");
    ozayn_dep_register_simple(&dep_mgr, "Diagnostics");
    ozayn_dep_register_simple(&dep_mgr, "StateManager");

    /* Declare dependency edges (required) */
    /* Security depends on EventEngine */
    ozayn_dep_add_required(&dep_mgr, "Security", "EventEngine");
    /* Authorization depends on Security and EventEngine */
    ozayn_dep_add_required(&dep_mgr, "Authorization", "Security");
    ozayn_dep_add_required(&dep_mgr, "Authorization", "EventEngine");
    /* SecurityBoundary depends on Security */
    ozayn_dep_add_required(&dep_mgr, "SecurityBoundary", "Security");
    /* CommandEngine depends on EventEngine */
    ozayn_dep_add_required(&dep_mgr, "CommandEngine", "EventEngine");

    /* ResourceManager depends on Security */
    ozayn_dep_add_required(&dep_mgr, "ResourceManager", "Security");
    /* TaskManager depends on ResourceManager */
    ozayn_dep_add_required(&dep_mgr, "TaskManager", "ResourceManager");
    /* ProcessManager depends on Security */
    ozayn_dep_add_required(&dep_mgr, "ProcessManager", "Security");
    /* Scheduler depends on TaskManager and ResourceManager */
    ozayn_dep_add_required(&dep_mgr, "Scheduler", "TaskManager");
    ozayn_dep_add_required(&dep_mgr, "Scheduler", "ResourceManager");
    /* IPCManager depends on Security and EventEngine */
    ozayn_dep_add_required(&dep_mgr, "IPCManager", "Security");
    ozayn_dep_add_required(&dep_mgr, "IPCManager", "EventEngine");
    /* ServiceRegistry depends on IPCManager */
    ozayn_dep_add_required(&dep_mgr, "ServiceRegistry", "IPCManager");

    /* ModuleManager depends on Security and EventEngine */
    ozayn_dep_add_required(&dep_mgr, "ModuleManager", "Security");
    ozayn_dep_add_required(&dep_mgr, "ModuleManager", "EventEngine");
    /* PluginManager depends on Security, EventEngine, ModuleManager */
    ozayn_dep_add_required(&dep_mgr, "PluginManager", "Security");
    ozayn_dep_add_required(&dep_mgr, "PluginManager", "EventEngine");
    ozayn_dep_add_required(&dep_mgr, "PluginManager", "ModuleManager");

    /* Monitoring depends on EventEngine */
    ozayn_dep_add_required(&dep_mgr, "Monitoring", "EventEngine");
    /* Diagnostics depends on Monitoring (optional) */
    ozayn_dep_add_optional(&dep_mgr, "Diagnostics", "Monitoring");
    /* StateManager depends on EventEngine */
    ozayn_dep_add_required(&dep_mgr, "StateManager", "EventEngine");

    /* Resolve the dependency graph */
    int dep_resolved = ozayn_dep_resolve(&dep_mgr);
    ozayn_events_publish(&events, OZAYN_DEP_EVENT_RESOLVED, OZAYN_SRC_DEP, NULL);
    ozayn_events_process(&events);

    /* Bind dependency manager to runtime */
    ozayn_runtime_set_dependency_mgr(rt, &dep_mgr);

    /* ================================================================
     * SERVICE LIFECYCLE MANAGER — service state machine
     * ================================================================ */

    LOG_INFO("CORE", "=== SERVICE LIFECYCLE MANAGER ===");

    ozayn_svc_lc_manager_t svc_lc_mgr;
    ozayn_svc_lc_config_t svc_lc_cfg = {
        .max_services = 32,
        .max_groups   = 8,
    };
    ozayn_svc_lc_init(&svc_lc_mgr, &svc_lc_cfg);
    ozayn_svc_lc_set_events(&svc_lc_mgr, &events);

    /* Register core services */
    ozayn_svc_config_t svc_cfg_core = {
        .name = "EventEngine", .version = "1.0.0",
        .restart_policy = OZAYN_SVC_RESTART_ALWAYS,
        .max_restarts = 5, .restart_window_ms = 60000,
        .drain_timeout_ms = 3000, .required = 1,
    };
    ozayn_svc_lc_register(&svc_lc_mgr, &svc_cfg_core);

    svc_cfg_core.name = "SecurityEngine";
    svc_cfg_core.version = "1.0.0";
    svc_cfg_core.restart_policy = OZAYN_SVC_RESTART_ON_FAILURE;
    svc_cfg_core.max_restarts = 3;
    svc_cfg_core.required = 1;
    ozayn_svc_lc_register(&svc_lc_mgr, &svc_cfg_core);

    svc_cfg_core.name = "Scheduler";
    svc_cfg_core.version = "1.2.0";
    svc_cfg_core.restart_policy = OZAYN_SVC_RESTART_ALWAYS;
    svc_cfg_core.max_restarts = 10;
    svc_cfg_core.required = 1;
    ozayn_svc_lc_register(&svc_lc_mgr, &svc_cfg_core);

    svc_cfg_core.name = "PluginManager";
    svc_cfg_core.version = "0.9.0";
    svc_cfg_core.restart_policy = OZAYN_SVC_RESTART_ON_FAILURE;
    svc_cfg_core.max_restarts = 3;
    svc_cfg_core.required = 0;
    ozayn_svc_lc_register(&svc_lc_mgr, &svc_cfg_core);

    svc_cfg_core.name = "Monitoring";
    svc_cfg_core.version = "1.0.0";
    svc_cfg_core.restart_policy = OZAYN_SVC_RESTART_ALWAYS;
    svc_cfg_core.max_restarts = 5;
    svc_cfg_core.required = 0;
    ozayn_svc_lc_register(&svc_lc_mgr, &svc_cfg_core);

    /* Create a group and start services */
    ozayn_svc_lc_group_create(&svc_lc_mgr, "core_services");
    ozayn_svc_lc_group_add(&svc_lc_mgr, "core_services", "EventEngine");
    ozayn_svc_lc_group_add(&svc_lc_mgr, "core_services", "SecurityEngine");
    ozayn_svc_lc_group_add(&svc_lc_mgr, "core_services", "Scheduler");

    /* Bind service lifecycle manager to runtime */
    ozayn_runtime_set_svc_lifecycle_mgr(rt, &svc_lc_mgr);

    /* ================================================================
     * CONFIGURATION MANAGER — per-service config with hot-reload
     * ================================================================ */

    LOG_INFO("CORE", "=== CONFIGURATION MANAGER ===");

    ozayn_cfg_mgr_t cfg_mgr;
    ozayn_cfg_mgr_config_t cfg_mgr_cfg = {
        .max_services = 32,
        .max_keys_per_service = 32,
        .max_history = 16,
        .max_listeners = 4,
    };
    ozayn_cfg_mgr_init(&cfg_mgr, &cfg_mgr_cfg);
    ozayn_cfg_mgr_set_events(&cfg_mgr, &events);

    /* Register service configs */
    ozayn_cfg_mgr_register_service(&cfg_mgr, "EventEngine");
    ozayn_cfg_mgr_register_service(&cfg_mgr, "Scheduler");
    ozayn_cfg_mgr_register_service(&cfg_mgr, "SecurityEngine");

    /* Set defaults */
    ozayn_cfg_mgr_set_default_int(&cfg_mgr, "EventEngine", "queue_size", 1024);
    ozayn_cfg_mgr_set_default_string(&cfg_mgr, "EventEngine", "mode", "async");
    ozayn_cfg_mgr_set_default_int(&cfg_mgr, "Scheduler", "max_threads", 4);
    ozayn_cfg_mgr_set_default_bool(&cfg_mgr, "Scheduler", "preemptive", 1);

    /* Set actual values */
    ozayn_cfg_mgr_set_int(&cfg_mgr, "EventEngine", "queue_size", 2048);
    ozayn_cfg_mgr_set_string(&cfg_mgr, "EventEngine", "mode", "sync");
    ozayn_cfg_mgr_set_int(&cfg_mgr, "Scheduler", "max_threads", 8);
    ozayn_cfg_mgr_set_bool(&cfg_mgr, "Scheduler", "preemptive", 0);
    ozayn_cfg_mgr_set_string(&cfg_mgr, "SecurityEngine", "auth_mode", "uid");
    ozayn_cfg_mgr_set_float(&cfg_mgr, "SecurityEngine", "timeout", 30.0);
    ozayn_cfg_mgr_set_bool(&cfg_mgr, "SecurityEngine", "audit_log", 1);

    /* Bind config manager to runtime */
    ozayn_runtime_set_config_mgr(rt, &cfg_mgr);

    /* ================================================================
     * CORE API — formal interface contracts
     * ================================================================ */

    LOG_INFO("CORE", "=== CORE API MANAGER ===");

    ozayn_api_manager_t api_mgr;
    ozayn_api_init(&api_mgr);
    ozayn_api_set_events(&api_mgr, &events);

    /* Register service interfaces */
    ozayn_api_version_t v1_0 = {1, 0, 0, 0};
    ozayn_api_register(&api_mgr, "EventEngine", "EventEngine",
                        &v1_0, OZAYN_API_STABLE, "Event publication and subscription");
    ozayn_api_add_method(&api_mgr, "EventEngine", OZAYN_METHOD_NOTIFY);
    ozayn_api_add_method(&api_mgr, "EventEngine", OZAYN_METHOD_QUERY);

    ozayn_api_register(&api_mgr, "SecurityEngine", "SecurityEngine",
                        &v1_0, OZAYN_API_STABLE, "Authentication and authorization");
    ozayn_api_add_method(&api_mgr, "SecurityEngine", OZAYN_METHOD_CALL);
    ozayn_api_add_method(&api_mgr, "SecurityEngine", OZAYN_METHOD_QUERY);
    ozayn_api_add_method(&api_mgr, "SecurityEngine", OZAYN_METHOD_STATUS);

    ozayn_api_version_t v1_2 = {1, 2, 0, 0};
    ozayn_api_register(&api_mgr, "Scheduler", "Scheduler",
                        &v1_2, OZAYN_API_STABLE, "Task scheduling and priority management");
    ozayn_api_add_method(&api_mgr, "Scheduler", OZAYN_METHOD_CREATE);
    ozayn_api_add_method(&api_mgr, "Scheduler", OZAYN_METHOD_DESTROY);
    ozayn_api_add_method(&api_mgr, "Scheduler", OZAYN_METHOD_STATUS);

    ozayn_api_version_t v0_9 = {0, 9, 0, 0};
    ozayn_api_register(&api_mgr, "PluginManager", "PluginManager",
                        &v0_9, OZAYN_API_EXPERIMENTAL, "Dynamic plugin loading");
    ozayn_api_add_method(&api_mgr, "PluginManager", OZAYN_METHOD_START);
    ozayn_api_add_method(&api_mgr, "PluginManager", OZAYN_METHOD_STOP);

    ozayn_api_register(&api_mgr, "ConfigManager", "ConfigManager",
                        &v1_0, OZAYN_API_STABLE, "Configuration management and hot-reload");
    ozayn_api_add_method(&api_mgr, "ConfigManager", OZAYN_METHOD_GET);
    ozayn_api_add_method(&api_mgr, "ConfigManager", OZAYN_METHOD_SET);

    /* Bind API manager to runtime */
    ozayn_runtime_set_api_mgr(rt, &api_mgr);

    /* ================================================================
     * RELOAD MANAGER — runtime hot-reloading
     * ================================================================ */

    LOG_INFO("CORE", "=== RELOAD MANAGER ===");

    ozayn_reload_mgr_t reload_mgr;
    ozayn_reload_config_t reload_cfg = {
        .quiesce_timeout_ms      = 5000,
        .load_timeout_ms         = 10000,
        .health_check_timeout_ms = 3000,
        .rollback_on_fail        = 1,
        .max_concurrent          = 1,
    };
    ozayn_reload_mgr_init(&reload_mgr, &reload_cfg);
    ozayn_reload_mgr_set_events(&reload_mgr, &events);

    /* Register components with different reload capabilities */
    ozayn_reload_register(&reload_mgr, "EventEngine", "1.0.0",
                           OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_register(&reload_mgr, "SecurityEngine", "1.0.0",
                           OZAYN_RELOAD_RESTART_REQUIRED, 1);
    ozayn_reload_register(&reload_mgr, "Scheduler", "1.2.0",
                           OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_register(&reload_mgr, "PluginManager", "0.9.0",
                           OZAYN_RELOAD_SUPPORTED, 0);
    ozayn_reload_register(&reload_mgr, "Monitoring", "1.0.0",
                           OZAYN_RELOAD_SUPPORTED, 0);
    ozayn_reload_register(&reload_mgr, "CoreRuntime", "0.1.0",
                           OZAYN_RELOAD_UNSUPPORTED, 1);

    /* Bind reload manager to runtime */
    ozayn_runtime_set_reload_mgr(rt, &reload_mgr);

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

    /* 23. Dependency graph */
    LOG_INFO("DEMO", "--- Demonstration: Dependency graph ---");
    ozayn_dep_print_graph(&dep_mgr);

    /* 24. Dependency status */
    LOG_INFO("DEMO", "--- Demonstration: Dependency status ---");
    ozayn_dep_print_status(&dep_mgr);
    LOG_INFO("DEMO", "Resolution: %s", dep_resolved == 0 ? "OK" : "FAILED");
    LOG_INFO("DEMO", "Nodes: %d, Edges: %d", ozayn_dep_node_count(&dep_mgr),
             ozayn_dep_edge_count(&dep_mgr));

    /* 25. Startup & shutdown order */
    LOG_INFO("DEMO", "--- Demonstration: Dependency order ---");
    ozayn_dep_print_startup_order(&dep_mgr);

    /* 26. Forward & reverse lookup */
    LOG_INFO("DEMO", "--- Demonstration: Forward & reverse lookup ---");
    char deps_buf[OZAYN_DEP_MAX_PER_NODE][OZAYN_DEP_MAX_NAME];
    int deps_count = ozayn_dep_get_dependencies(&dep_mgr, "Scheduler", deps_buf, OZAYN_DEP_MAX_PER_NODE);
    LOG_INFO("DEMO", "Scheduler depends on (%d):", deps_count);
    for (int i = 0; i < deps_count; i++) {
        LOG_INFO("DEMO", "  -> %s", deps_buf[i]);
    }

    char rdeps_buf[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_NAME];
    int rdeps_count = ozayn_dep_get_dependents(&dep_mgr, "Security", rdeps_buf, OZAYN_DEP_MAX_NODES);
    LOG_INFO("DEMO", "Security is depended on by (%d):", rdeps_count);
    for (int i = 0; i < rdeps_count; i++) {
        LOG_INFO("DEMO", "  <- %s", rdeps_buf[i]);
    }

    /* 27. Can-start checks */
    LOG_INFO("DEMO", "--- Demonstration: Can-start checks ---");
    LOG_INFO("DEMO", "Can Scheduler start? %s", ozayn_dep_can_start(&dep_mgr, "Scheduler") ? "YES" : "NO");
    LOG_INFO("DEMO", "Can Security start? %s", ozayn_dep_can_start(&dep_mgr, "Security") ? "YES" : "NO");
    LOG_INFO("DEMO", "All nodes ready? %s", ozayn_dep_all_ready(&dep_mgr) ? "YES" : "NO");

    /* 28. Dependency state queries */
    LOG_INFO("DEMO", "--- Demonstration: Dependency state queries ---");
    LOG_INFO("DEMO", "EventEngine state: %s", ozayn_dep_state_name(ozayn_dep_get_state(&dep_mgr, "EventEngine")));
    LOG_INFO("DEMO", "Scheduler state: %s", ozayn_dep_state_name(ozayn_dep_get_state(&dep_mgr, "Scheduler")));
    LOG_INFO("DEMO", "Ready count: %d", ozayn_dep_ready_count(&dep_mgr));
    LOG_INFO("DEMO", "Blocked count: %d", ozayn_dep_blocked_count(&dep_mgr));
    LOG_INFO("DEMO", "Failed count: %d", ozayn_dep_failed_count(&dep_mgr));

    /* 29. Dependency events */
    LOG_INFO("DEMO", "--- Demonstration: Dependency events ---");
    LOG_INFO("DEMO", "DEP_REGISTERED = %d", OZAYN_DEP_EVENT_REGISTERED);
    LOG_INFO("DEMO", "DEP_EDGE_ADDED = %d", OZAYN_DEP_EVENT_EDGE_ADDED);
    LOG_INFO("DEMO", "DEP_RESOLVED = %d", OZAYN_DEP_EVENT_RESOLVED);
    LOG_INFO("DEMO", "DEP_STATE_CHANGED = %d", OZAYN_DEP_EVENT_STATE_CHANGED);

    /* 30. Service lifecycle — start group */
    LOG_INFO("DEMO", "--- Demonstration: Service lifecycle ---");
    ozayn_svc_lc_group_start(&svc_lc_mgr, "core_services");
    LOG_INFO("DEMO", "Running services: %u", ozayn_svc_lc_running_count(&svc_lc_mgr));
    LOG_INFO("DEMO", "EventEngine state: %s",
             ozayn_svc_state_name(ozayn_svc_lc_get_state(&svc_lc_mgr, "EventEngine")));
    LOG_INFO("DEMO", "Scheduler state: %s",
             ozayn_svc_state_name(ozayn_svc_lc_get_state(&svc_lc_mgr, "Scheduler")));

    /* 31. Health check */
    LOG_INFO("DEMO", "--- Demonstration: Service health ---");
    ozayn_svc_lc_check_all(&svc_lc_mgr);
    LOG_INFO("DEMO", "EventEngine health: %s",
             ozayn_svc_health_name(ozayn_svc_lc_find(&svc_lc_mgr, "EventEngine")->health));
    ozayn_svc_lc_set_health(&svc_lc_mgr, "PluginManager", OZAYN_SVC_HEALTH_DEGRADED);
    LOG_INFO("DEMO", "PluginManager health: %s",
             ozayn_svc_health_name(ozayn_svc_lc_find(&svc_lc_mgr, "PluginManager")->health));

    /* 32. Restart and failure */
    LOG_INFO("DEMO", "--- Demonstration: Restart & failure ---");
    ozayn_svc_lc_start(&svc_lc_mgr, "Monitoring");
    LOG_INFO("DEMO", "Monitoring state: %s",
             ozayn_svc_state_name(ozayn_svc_lc_get_state(&svc_lc_mgr, "Monitoring")));
    ozayn_svc_lc_restart(&svc_lc_mgr, "PluginManager");
    LOG_INFO("DEMO", "PluginManager restarts: %u",
             ozayn_svc_lc_find(&svc_lc_mgr, "PluginManager")->restart_count);

    /* 33. Print full status */
    LOG_INFO("DEMO", "--- Demonstration: Full status ---");
    ozayn_svc_lc_print_status(&svc_lc_mgr);
    ozayn_svc_lc_print_groups(&svc_lc_mgr);

    /* 34. Stats */
    LOG_INFO("DEMO", "--- Demonstration: Lifecycle stats ---");
    ozayn_svc_lc_stats_t svc_stats = ozayn_svc_lc_stats(&svc_lc_mgr);
    LOG_INFO("DEMO", "Services: %u (running=%u, stopped=%u, failed=%u)",
             svc_stats.total_services, svc_stats.running, svc_stats.stopped, svc_stats.failed);
    LOG_INFO("DEMO", "Restarts: %u, Failures: %u", svc_stats.total_restarts, svc_stats.total_failures);

    /* 35. Stop group */
    LOG_INFO("DEMO", "--- Demonstration: Group stop ---");
    ozayn_svc_lc_group_stop(&svc_lc_mgr, "core_services");
    LOG_INFO("DEMO", "Running after group stop: %u", ozayn_svc_lc_running_count(&svc_lc_mgr));

    /* 36. Configuration manager — get values */
    LOG_INFO("DEMO", "--- Demonstration: Configuration get ---");
    int64_t qsize = 0;
    ozayn_cfg_mgr_get_int(&cfg_mgr, "EventEngine", "queue_size", &qsize);
    LOG_INFO("DEMO", "EventEngine queue_size: %lld", (long long)qsize);
    char mode[64];
    ozayn_cfg_mgr_get_string(&cfg_mgr, "EventEngine", "mode", mode, sizeof(mode));
    LOG_INFO("DEMO", "EventEngine mode: %s", mode);
    int preempt = -1;
    ozayn_cfg_mgr_get_bool(&cfg_mgr, "Scheduler", "preemptive", &preempt);
    LOG_INFO("DEMO", "Scheduler preemptive: %s", preempt ? "true" : "false");
    double timeout = 0;
    ozayn_cfg_mgr_get_float(&cfg_mgr, "SecurityEngine", "timeout", &timeout);
    LOG_INFO("DEMO", "SecurityEngine timeout: %.1f", timeout);

    /* 37. Configuration defaults */
    LOG_INFO("DEMO", "--- Demonstration: Configuration defaults ---");
    int64_t max_threads = 0;
    ozayn_cfg_mgr_get_int(&cfg_mgr, "Scheduler", "max_threads", &max_threads);
    LOG_INFO("DEMO", "Scheduler max_threads (explicit): %lld", (long long)max_threads);
    int64_t default_val = 0;
    ozayn_cfg_mgr_get_int(&cfg_mgr, "EventEngine", "nonexistent_key", &default_val);
    LOG_INFO("DEMO", "EventEngine nonexistent_key (default): %lld", (long long)default_val);

    /* 38. Configuration versioning */
    LOG_INFO("DEMO", "--- Demonstration: Configuration versioning ---");
    LOG_INFO("DEMO", "Global version: %u", ozayn_cfg_mgr_global_version(&cfg_mgr));
    LOG_INFO("DEMO", "EventEngine version: %u",
             ozayn_cfg_mgr_service_version(&cfg_mgr, "EventEngine"));
    LOG_INFO("DEMO", "EventEngine queue_size version: %u",
             ozayn_cfg_mgr_key_version(&cfg_mgr, "EventEngine", "queue_size"));

    /* 39. Configuration hot-reload from string */
    LOG_INFO("DEMO", "--- Demonstration: Configuration hot-reload ---");
    const char *hot_config =
        "worker_count = 16\n"
        "enable_tracing = true\n"
        "max_latency = 2.5\n"
        "log_format = json\n";
    int loaded = ozayn_cfg_mgr_load_from_string(&cfg_mgr, "EventEngine", hot_config);
    LOG_INFO("DEMO", "Hot-reloaded %d keys into EventEngine", loaded);
    int64_t workers = 0;
    ozayn_cfg_mgr_get_int(&cfg_mgr, "EventEngine", "worker_count", &workers);
    LOG_INFO("DEMO", "EventEngine worker_count: %lld", (long long)workers);
    int tracing = -1;
    ozayn_cfg_mgr_get_bool(&cfg_mgr, "EventEngine", "enable_tracing", &tracing);
    LOG_INFO("DEMO", "EventEngine enable_tracing: %s", tracing ? "true" : "false");

    /* 40. Configuration print all */
    LOG_INFO("DEMO", "--- Demonstration: Configuration print ---");
    ozayn_cfg_mgr_print_all(&cfg_mgr);
    ozayn_cfg_mgr_stats_t cfg_stats = ozayn_cfg_mgr_stats(&cfg_mgr);
    LOG_INFO("DEMO", "Config stats: services=%u keys=%u changes=%u version=%u",
             cfg_stats.total_services, cfg_stats.total_keys,
             cfg_stats.total_changes, cfg_stats.global_version);

    /* 41. API interfaces */
    LOG_INFO("DEMO", "--- Demonstration: API interfaces ---");
    ozayn_api_print_interfaces(&api_mgr);
    LOG_INFO("DEMO", "Has SecurityEngine? %s",
             ozayn_api_has_interface(&api_mgr, "SecurityEngine", 1) ? "YES" : "NO");
    LOG_INFO("DEMO", "Has Scheduler v2? %s",
             ozayn_api_has_interface(&api_mgr, "Scheduler", 2) ? "YES" : "NO");
    LOG_INFO("DEMO", "Has NonExistent? %s",
             ozayn_api_has_interface(&api_mgr, "NonExistent", 0) ? "YES" : "NO");

    /* 42. API version checking */
    LOG_INFO("DEMO", "--- Demonstration: API version check ---");
    ozayn_api_version_t provided = {1, 2, 0, 100};
    ozayn_api_version_t required = {1, 2, 0, 200};
    ozayn_api_compat_t compat = ozayn_api_check_compat(&provided, &required);
    LOG_INFO("DEMO", "v1.2.0.100 vs v1.2.0.200 -> %s", ozayn_api_compat_name(compat));
    ozayn_api_version_t req_major = {2, 0, 0, 0};
    compat = ozayn_api_check_compat(&provided, &req_major);
    LOG_INFO("DEMO", "v1.2.0.100 vs v2.0.0.0  -> %s", ozayn_api_compat_name(compat));

    /* 43. API request/response flow */
    LOG_INFO("DEMO", "--- Demonstration: Request/Response ---");
    uint32_t req1 = ozayn_api_request_begin(&api_mgr, "Scheduler", "SecurityEngine",
                                             "authenticate", OZAYN_METHOD_CALL);
    LOG_INFO("DEMO", "Request #%u created", req1);
    uint32_t req2 = ozayn_api_request_begin(&api_mgr, "PluginManager", "Scheduler",
                                             "submit_task", OZAYN_METHOD_CREATE);
    LOG_INFO("DEMO", "Request #%u created", req2);
    ozayn_api_request_complete(&api_mgr, req1, OZAYN_API_ERR_OK, NULL);
    ozayn_api_request_complete(&api_mgr, req2, OZAYN_API_ERR_TIMEOUT, "Task queue full");

    /* 44. API pending and stats */
    LOG_INFO("DEMO", "--- Demonstration: API stats ---");
    ozayn_api_print_pending(&api_mgr);
    ozayn_api_stats_t api_stats = ozayn_api_stats(&api_mgr);
    LOG_INFO("DEMO", "API stats: interfaces=%u requests=%u responses=%u errors=%u",
             api_stats.total_interfaces, api_stats.total_requests,
             api_stats.total_responses, api_stats.total_errors);

    /* 45. Reload manager — component registration */
    LOG_INFO("DEMO", "--- Demonstration: Reload components ---");
    ozayn_reload_print_components(&reload_mgr);
    LOG_INFO("DEMO", "EventEngine reloadable? %s",
             ozayn_reload_is_reloadable(&reload_mgr, "EventEngine") ? "YES" : "NO");
    LOG_INFO("DEMO", "CoreRuntime reloadable? %s",
             ozayn_reload_is_reloadable(&reload_mgr, "CoreRuntime") ? "YES" : "NO");
    LOG_INFO("DEMO", "SecurityEngine reloadable? %s",
             ozayn_reload_is_reloadable(&reload_mgr, "SecurityEngine") ? "YES" : "NO");

    /* 46. Reload — non-reloadable rejection */
    LOG_INFO("DEMO", "--- Demonstration: Non-reloadable rejection ---");
    int r = ozayn_reload_request(&reload_mgr, "CoreRuntime", "0.2.0",
                                  "admin", "upgrade");
    LOG_INFO("DEMO", "Reload CoreRuntime: %s (expected: rejected)", r == 0 ? "OK" : "REJECTED");
    LOG_INFO("DEMO", "Reload CoreRuntime result: %s",
             ozayn_reload_result_name(OZAYN_RELOAD_RESULT_NOT_RELOADABLE));
    ozayn_events_process(&events);

    /* 47. Reload — successful hot reload of PluginManager */
    LOG_INFO("DEMO", "--- Demonstration: Hot reload PluginManager ---");
    ozayn_reload_request(&reload_mgr, "PluginManager", "1.0.0",
                          "admin", "version upgrade");
    /* Tick through all states */
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&reload_mgr)) break;
        ozayn_reload_tick(&reload_mgr);
    }
    LOG_INFO("DEMO", "Reload result: %s", ozayn_reload_is_busy(&reload_mgr) ? "IN PROGRESS" : "DONE");
    LOG_INFO("DEMO", "PluginManager version: %s",
             ozayn_reload_find(&reload_mgr, "PluginManager")->current_version);

    /* 48. Reload — active work blocks quiesce */
    LOG_INFO("DEMO", "--- Demonstration: Active work blocks quiesce ---");
    ozayn_reload_request_begin(&reload_mgr, "Monitoring");
    ozayn_reload_request_begin(&reload_mgr, "Monitoring");
    ozayn_reload_request_begin(&reload_mgr, "Monitoring");
    LOG_INFO("DEMO", "Monitoring active requests: %u",
             ozayn_reload_active_requests(&reload_mgr, "Monitoring"));
    ozayn_reload_request(&reload_mgr, "Monitoring", "1.1.0",
                          "admin", "upgrade");
    /* Tick — should stall at quiesce */
    for (int i = 0; i < 3; i++) {
        if (!ozayn_reload_is_busy(&reload_mgr)) break;
        ozayn_reload_tick(&reload_mgr);
    }
    LOG_INFO("DEMO", "Reload state (with active work): %s",
             ozayn_reload_state_name(ozayn_reload_get_state(&reload_mgr, "Monitoring")));
    /* Complete the work */
    ozayn_reload_request_end(&reload_mgr, "Monitoring");
    ozayn_reload_request_end(&reload_mgr, "Monitoring");
    ozayn_reload_request_end(&reload_mgr, "Monitoring");
    /* Now tick — should complete */
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&reload_mgr)) break;
        ozayn_reload_tick(&reload_mgr);
    }
    LOG_INFO("DEMO", "Monitoring version after reload: %s",
             ozayn_reload_find(&reload_mgr, "Monitoring")->current_version);

    /* 49. Reload — state save/load */
    LOG_INFO("DEMO", "--- Demonstration: State save/load ---");
    const char *saved_state = "{\"counter\":42,\"mode\":\"active\"}";
    ozayn_reload_save_state(&reload_mgr, "Scheduler",
                             saved_state, (uint32_t)strlen(saved_state) + 1, 1);
    char loaded_state[256];
    uint32_t loaded_ver = 0;
    ozayn_reload_load_state(&reload_mgr, "Scheduler",
                             loaded_state, sizeof(loaded_state), &loaded_ver);
    LOG_INFO("DEMO", "Loaded state: '%s' (version=%u)", loaded_state, loaded_ver);

    /* 50. Reload — audit trail */
    LOG_INFO("DEMO", "--- Demonstration: Reload audit trail ---");
    ozayn_reload_print_audit(&reload_mgr);
    LOG_INFO("DEMO", "Audit entries: %u", ozayn_reload_audit_count(&reload_mgr));

    /* 51. Reload — stats */
    LOG_INFO("DEMO", "--- Demonstration: Reload stats ---");
    ozayn_reload_stats_t rl_stats = ozayn_reload_stats(&reload_mgr);
    LOG_INFO("DEMO", "Reloadable: %u, Non-reloadable: %u, Restart-required: %u",
             rl_stats.reloadable_count, rl_stats.non_reloadable_count,
             rl_stats.restart_required_count);
    LOG_INFO("DEMO", "Requests: %u (succeeded=%u, failed=%u, rollback=%u)",
             rl_stats.total_requests, rl_stats.total_succeeded,
             rl_stats.total_failed, rl_stats.total_rollback);

    /* 52. Reload — full status */
    LOG_INFO("DEMO", "--- Demonstration: Reload status ---");
    ozayn_reload_print_status(&reload_mgr);

    /* 53. Process events */
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
    ozayn_svc_lc_shutdown(&svc_lc_mgr);
    ozayn_cfg_mgr_shutdown(&cfg_mgr);
    ozayn_api_shutdown(&api_mgr);
    ozayn_reload_mgr_shutdown(&reload_mgr);
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
    ozayn_dep_shutdown(&dep_mgr);
    ozayn_runtime_destroy(rt);

    printf("\n  OZAYN Core shutdown complete.\n\n");
    return 0;
}
