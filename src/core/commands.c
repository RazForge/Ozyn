#include "commands.h"
#include "logger.h"
#include "recovery.h"
#include "runtime.h"
#include "events.h"
#include "registry.h"
#include "security.h"
#include "authorization.h"
#include "monitoring.h"
#include "diagnostics.h"
#include "security_boundary.h"
#include "state_manager.h"
#include <stdio.h>
#include <string.h>

/*
 * Command engine implementation.
 *
 * Static registry of known command types mapped to handler functions.
 * Synchronous execution: validate → execute → result.
 *
 * Handlers receive the command engine as context, giving access to
 * runtime, event engine, and error recovery through void* pointers.
 */

/* Forward declarations of handler functions */
static ozayn_command_result_t handle_status(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_stop(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_service_list(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_service_status(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_auth_status(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_identity_list(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_permission_check(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_role_list(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_health(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_metrics(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_diagnose(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_snapshot(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_incidents(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_trace(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_sec_status(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_sec_contexts(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_sec_check(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_sec_violations(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_state_status(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_state_save(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_state_load(const ozayn_command_t *cmd, void *ctx);
static ozayn_command_result_t handle_state_info(const ozayn_command_t *cmd, void *ctx);

/* ---- Built-in command registry ---- */

static const ozayn_command_entry_t builtin_registry[] = {
    { OZAYN_CMD_STATUS,         handle_status,         "STATUS"         },
    { OZAYN_CMD_STOP,           handle_stop,           "STOP"           },
    { OZAYN_CMD_SERVICE_LIST,   handle_service_list,   "SERVICE LIST"   },
    { OZAYN_CMD_SERVICE_STATUS, handle_service_status, "SERVICE STATUS" },
    { OZAYN_CMD_AUTH_STATUS,       handle_auth_status,       "AUTH STATUS"       },
    { OZAYN_CMD_IDENTITY_LIST,     handle_identity_list,     "IDENTITY LIST"     },
    { OZAYN_CMD_PERMISSION_CHECK,  handle_permission_check,  "PERMISSION CHECK"  },
    { OZAYN_CMD_ROLE_LIST,         handle_role_list,         "ROLE LIST"         },
    { OZAYN_CMD_HEALTH,            handle_health,            "HEALTH"            },
    { OZAYN_CMD_METRICS,           handle_metrics,           "METRICS"           },
    { OZAYN_CMD_DIAGNOSE,          handle_diagnose,          "DIAGNOSE"          },
    { OZAYN_CMD_SNAPSHOT,          handle_snapshot,          "SNAPSHOT"          },
    { OZAYN_CMD_INCIDENTS,         handle_incidents,         "INCIDENTS"         },
    { OZAYN_CMD_TRACE,             handle_trace,             "TRACE"             },
    { OZAYN_CMD_SEC_STATUS,        handle_sec_status,        "SEC STATUS"        },
    { OZAYN_CMD_SEC_CONTEXTS,      handle_sec_contexts,      "SEC CONTEXTS"      },
    { OZAYN_CMD_SEC_CHECK,         handle_sec_check,         "SEC CHECK"         },
    { OZAYN_CMD_SEC_VIOLATIONS,    handle_sec_violations,    "SEC VIOLATIONS"    },
    { OZAYN_CMD_STATE_STATUS,      handle_state_status,      "STATE STATUS"      },
    { OZAYN_CMD_STATE_SAVE,        handle_state_save,        "STATE SAVE"        },
    { OZAYN_CMD_STATE_LOAD,        handle_state_load,        "STATE LOAD"        },
    { OZAYN_CMD_STATE_INFO,        handle_state_info,        "STATE INFO"        },
};

static const int builtin_registry_size =
    (int)(sizeof(builtin_registry) / sizeof(builtin_registry[0]));

/* ---- Names ---- */

const char *ozayn_command_type_name(ozayn_command_type_t type) {
    switch (type) {
        case OZAYN_CMD_NONE:           return "NONE";
        case OZAYN_CMD_STATUS:         return "STATUS";
        case OZAYN_CMD_STOP:           return "STOP";
        case OZAYN_CMD_SERVICE_LIST:   return "SERVICE_LIST";
        case OZAYN_CMD_SERVICE_STATUS: return "SERVICE_STATUS";
        case OZAYN_CMD_AUTH_STATUS:       return "AUTH_STATUS";
        case OZAYN_CMD_IDENTITY_LIST:     return "IDENTITY_LIST";
        case OZAYN_CMD_PERMISSION_CHECK:  return "PERMISSION_CHECK";
        case OZAYN_CMD_ROLE_LIST:         return "ROLE_LIST";
        case OZAYN_CMD_HEALTH:            return "HEALTH";
        case OZAYN_CMD_METRICS:           return "METRICS";
        case OZAYN_CMD_DIAGNOSE:          return "DIAGNOSE";
        case OZAYN_CMD_SNAPSHOT:          return "SNAPSHOT";
        case OZAYN_CMD_INCIDENTS:         return "INCIDENTS";
        case OZAYN_CMD_TRACE:             return "TRACE";
        case OZAYN_CMD_SEC_STATUS:        return "SEC_STATUS";
        case OZAYN_CMD_SEC_CONTEXTS:      return "SEC_CONTEXTS";
        case OZAYN_CMD_SEC_CHECK:         return "SEC_CHECK";
        case OZAYN_CMD_SEC_VIOLATIONS:    return "SEC_VIOLATIONS";
        case OZAYN_CMD_STATE_STATUS:      return "STATE_STATUS";
        case OZAYN_CMD_STATE_SAVE:        return "STATE_SAVE";
        case OZAYN_CMD_STATE_LOAD:        return "STATE_LOAD";
        case OZAYN_CMD_STATE_INFO:        return "STATE_INFO";
    }
    return "UNKNOWN";
}

const char *ozayn_command_source_name(ozayn_command_source_t src) {
    switch (src) {
        case OZAYN_CMD_SRC_CORE: return "CORE";
        case OZAYN_CMD_SRC_CLI:  return "CLI";
    }
    return "UNKNOWN";
}

const char *ozayn_command_status_name(ozayn_command_status_t status) {
    switch (status) {
        case OZAYN_CMD_RECEIVED:  return "RECEIVED";
        case OZAYN_CMD_VALIDATED: return "VALIDATED";
        case OZAYN_CMD_EXECUTING: return "EXECUTING";
        case OZAYN_CMD_COMPLETED: return "COMPLETED";
        case OZAYN_CMD_FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_command_result_name(ozayn_command_result_t result) {
    switch (result) {
        case OZAYN_CMD_RESULT_SUCCESS:   return "SUCCESS";
        case OZAYN_CMD_RESULT_FAILURE:   return "FAILURE";
        case OZAYN_CMD_RESULT_REJECTED:  return "REJECTED";
        case OZAYN_CMD_RESULT_INVALID:   return "INVALID";
        case OZAYN_CMD_RESULT_NOT_FOUND: return "NOT_FOUND";
    }
    return "UNKNOWN";
}

/* ---- Lifecycle ---- */

ozayn_result_t ozayn_command_engine_init(ozayn_command_engine_t *engine) {
    if (!engine) return OZAYN_ERR_NULL;

    memset(engine, 0, sizeof(ozayn_command_engine_t));

    engine->registry      = builtin_registry;
    engine->registry_size = builtin_registry_size;
    engine->next_id       = 1;
    engine->initialized   = 1;

    LOG_INFO("COMMANDS", "Command engine initialized (%d commands registered)",
             engine->registry_size);

    return OZAYN_OK;
}

void ozayn_command_engine_shutdown(ozayn_command_engine_t *engine) {
    if (!engine || !engine->initialized) return;

    engine->initialized = 0;
    engine->registry = NULL;
    engine->registry_size = 0;

    LOG_INFO("COMMANDS", "Command engine shut down");
}

/* ---- Binding ---- */

void ozayn_command_engine_set_runtime(ozayn_command_engine_t *engine, void *runtime) {
    if (engine) engine->runtime = runtime;
}

void ozayn_command_engine_set_events(ozayn_command_engine_t *engine, void *events) {
    if (engine) engine->events = events;
}

void ozayn_command_engine_set_recovery(ozayn_command_engine_t *engine, void *recovery) {
    if (engine) engine->recovery = recovery;
}

/* ---- Create command ---- */

ozayn_command_t ozayn_command_create(ozayn_command_type_t type,
                                     ozayn_command_source_t source) {
    ozayn_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type   = type;
    cmd.source = source;
    cmd.status = OZAYN_CMD_RECEIVED;
    cmd.result = OZAYN_CMD_RESULT_SUCCESS;
    return cmd;
}

/* ---- Validate ---- */

static ozayn_command_result_t validate_command(
    const ozayn_command_engine_t *engine,
    const ozayn_command_t *cmd)
{
    if (!cmd) return OZAYN_CMD_RESULT_INVALID;

    if (cmd->type == OZAYN_CMD_NONE) {
        LOG_WARN("COMMANDS", "Command type is NONE — rejected");
        return OZAYN_CMD_RESULT_INVALID;
    }

    /* Verify type exists in registry */
    int found = 0;
    for (int i = 0; i < engine->registry_size; i++) {
        if (engine->registry[i].type == cmd->type) {
            found = 1;
            break;
        }
    }

    if (!found) {
        LOG_WARN("COMMANDS", "Unknown command type %d — rejected",
                 (int)cmd->type);
        return OZAYN_CMD_RESULT_NOT_FOUND;
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- Lookup handler ---- */

static ozayn_command_handler_t lookup_handler(
    const ozayn_command_engine_t *engine,
    ozayn_command_type_t type)
{
    for (int i = 0; i < engine->registry_size; i++) {
        if (engine->registry[i].type == type) {
            return engine->registry[i].handler;
        }
    }
    return NULL;
}

/* ---- Execute ---- */

ozayn_command_result_t ozayn_command_engine_execute(
    ozayn_command_engine_t *engine,
    ozayn_command_t *cmd)
{
    if (!engine || !engine->initialized) {
        LOG_ERROR("COMMANDS", "Command engine not initialized");
        return OZAYN_CMD_RESULT_FAILURE;
    }
    if (!cmd) return OZAYN_CMD_RESULT_INVALID;

    /* Assign ID */
    cmd->id = engine->next_id++;

    LOG_DEBUG("COMMANDS", "Command #%u received: %s from %s",
              cmd->id,
              ozayn_command_type_name(cmd->type),
              ozayn_command_source_name(cmd->source));

    /* Validate */
    ozayn_command_result_t vr = validate_command(engine, cmd);
    if (vr != OZAYN_CMD_RESULT_SUCCESS) {
        cmd->status = OZAYN_CMD_FAILED;
        cmd->result = vr;
        LOG_WARN("COMMANDS", "Command #%u validation failed: %s",
                 cmd->id, ozayn_command_result_name(vr));
        return vr;
    }

    cmd->status = OZAYN_CMD_VALIDATED;

    /* Lookup handler */
    ozayn_command_handler_t handler = lookup_handler(engine, cmd->type);
    if (!handler) {
        cmd->status = OZAYN_CMD_FAILED;
        cmd->result = OZAYN_CMD_RESULT_NOT_FOUND;
        LOG_ERROR("COMMANDS", "Command #%u handler not found", cmd->id);
        return OZAYN_CMD_RESULT_NOT_FOUND;
    }

    /* Execute — pass engine as context so handlers can access runtime,
     * events, and recovery */
    cmd->status = OZAYN_CMD_EXECUTING;
    LOG_DEBUG("COMMANDS", "Command #%u executing", cmd->id);

    ozayn_command_result_t result = handler(cmd, engine);

    cmd->result = result;

    if (result == OZAYN_CMD_RESULT_SUCCESS) {
        cmd->status = OZAYN_CMD_COMPLETED;
        LOG_INFO("COMMANDS", "Command #%u completed: %s",
                 cmd->id, ozayn_command_type_name(cmd->type));
    } else {
        cmd->status = OZAYN_CMD_FAILED;
        LOG_WARN("COMMANDS", "Command #%u failed: %s",
                 cmd->id, ozayn_command_result_name(result));

        /* Report to error recovery */
        if (engine->recovery) {
            ozayn_recovery_raise((ozayn_recovery_t *)engine->recovery,
                                 OZAYN_ERRCAT_INTERNAL,
                                 OZAYN_LOG_WARNING,
                                 OZAYN_SCOPE_OPERATION,
                                 "COMMANDS",
                                 "Command execution failed");
        }
    }

    return result;
}

/* ================================================================
 * Built-in handlers
 * ================================================================ */

/* ---- STATUS handler ---- */

static ozayn_command_result_t handle_status(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt) {
        LOG_INFO("STATUS", "Runtime: not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    const char *state_name = ozayn_state_name(rt->state);
    int running = ozayn_runtime_is_running(rt);

    LOG_INFO("STATUS", "Runtime state: %s (running=%s)",
             state_name, running ? "yes" : "no");
    LOG_INFO("STATUS", "Core status: %s",
             rt->core.status ? rt->core.status : "UNKNOWN");
    LOG_INFO("STATUS", "Core version: %s",
             rt->core.version ? rt->core.version : "UNKNOWN");

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- STOP handler ---- */

static ozayn_command_result_t handle_stop(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt) {
        LOG_ERROR("STOP", "Runtime not available");
        return OZAYN_CMD_RESULT_FAILURE;
    }

    if (!ozayn_runtime_is_running(rt)) {
        LOG_WARN("STOP", "Runtime is not running — cannot stop");
        return OZAYN_CMD_RESULT_REJECTED;
    }

    LOG_INFO("STOP", "Stop requested — requesting runtime shutdown");

    /* Publish event through event engine */
    if (engine->events) {
        ozayn_events_publish((ozayn_event_engine_t *)engine->events,
                             OZAYN_EVENT_RUNTIME_STOPPING,
                             OZAYN_SRC_USER,
                             NULL);
    }

    /* Request stop through runtime API */
    ozayn_runtime_request_stop(rt);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SERVICE LIST handler ---- */

static ozayn_command_result_t handle_service_list(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->registry_mgr) {
        LOG_INFO("SERVICE_LIST", "Service registry not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_registry_manager_t *reg = (ozayn_registry_manager_t *)rt->registry_mgr;
    const ozayn_service_record_t *list[OZAYN_REGISTRY_MAX_SERVICES];
    int count = ozayn_registry_list(reg, list, OZAYN_REGISTRY_MAX_SERVICES);

    LOG_INFO("SERVICE_LIST", "--- Registered Services (%d) ---", count);
    for (int i = 0; i < count; i++) {
        LOG_INFO("SERVICE_LIST", "  [%d] '%s' v%s state=%s endpoint=%s",
                 i + 1, list[i]->id, list[i]->version,
                 ozayn_service_state_name(list[i]->state),
                 list[i]->endpoint);
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SERVICE STATUS handler ---- */

static ozayn_command_result_t handle_service_status(const ozayn_command_t *cmd, void *ctx) {
    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->registry_mgr) {
        LOG_INFO("SERVICE_STATUS", "Service registry not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    /* Payload is service ID string */
    const char *service_id = (const char *)cmd->payload;
    if (!service_id || service_id[0] == '\0') {
        LOG_WARN("SERVICE_STATUS", "No service ID provided");
        return OZAYN_CMD_RESULT_INVALID;
    }

    ozayn_registry_manager_t *reg = (ozayn_registry_manager_t *)rt->registry_mgr;
    const ozayn_service_record_t *rec = ozayn_registry_lookup(reg, service_id);

    if (!rec) {
        LOG_INFO("SERVICE_STATUS", "Service '%s' not found", service_id);
        return OZAYN_CMD_RESULT_NOT_FOUND;
    }

    LOG_INFO("SERVICE_STATUS", "--- Service: %s ---", rec->id);
    LOG_INFO("SERVICE_STATUS", "  Name:       %s", rec->name);
    LOG_INFO("SERVICE_STATUS", "  Version:    %s", rec->version);
    LOG_INFO("SERVICE_STATUS", "  Protocol:   %d", rec->protocol_version);
    LOG_INFO("SERVICE_STATUS", "  State:      %s", ozayn_service_state_name(rec->state));
    LOG_INFO("SERVICE_STATUS", "  Endpoint:   %s", rec->endpoint);
    LOG_INFO("SERVICE_STATUS", "  Provider:   %s", rec->provider);
    LOG_INFO("SERVICE_STATUS", "  Capabilities (%d):", rec->capability_count);
    for (int i = 0; i < rec->capability_count; i++) {
        LOG_INFO("SERVICE_STATUS", "    - %s", rec->capabilities[i]);
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- AUTH STATUS handler ---- */

static ozayn_command_result_t handle_auth_status(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_mgr) {
        LOG_INFO("AUTH_STATUS", "Security manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_security_manager_t *sec = (ozayn_security_manager_t *)rt->security_mgr;

    LOG_INFO("AUTH_STATUS", "--- Security Status ---");
    LOG_INFO("AUTH_STATUS", "  Enabled:     %s", sec->enabled ? "yes" : "no");
    LOG_INFO("AUTH_STATUS", "  Auth mode:   %s", ozayn_auth_method_name(sec->auth_mode));
    LOG_INFO("AUTH_STATUS", "  Audit log:   %s", sec->audit_logging ? "on" : "off");
    LOG_INFO("AUTH_STATUS", "  Identities:  %d", ozayn_security_identity_count(sec));
    LOG_INFO("AUTH_STATUS", "  Allowed UIDs: %d", sec->allowed_uid_count);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- IDENTITY LIST handler ---- */

static ozayn_command_result_t handle_identity_list(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_mgr) {
        LOG_INFO("IDENTITY_LIST", "Security manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_security_manager_t *sec = (ozayn_security_manager_t *)rt->security_mgr;

    LOG_INFO("IDENTITY_LIST", "--- Registered Identities (%d) ---",
             ozayn_security_identity_count(sec));

    for (int i = 0; i < OZAYN_SECURITY_MAX_IDENTITIES; i++) {
        const ozayn_identity_record_t *rec = &sec->identities[i];
        if (rec->active) {
            LOG_INFO("IDENTITY_LIST", "  [%d] '%s' type=%s trust=%s auth=%s uid=%u",
                     i + 1, rec->id,
                     ozayn_identity_type_name(rec->type),
                     ozayn_trust_state_name(rec->trust_state),
                     ozayn_auth_method_name(rec->auth_method),
                     rec->auth_uid);
        }
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- PERMISSION CHECK handler ---- */

static ozayn_command_result_t handle_permission_check(const ozayn_command_t *cmd, void *ctx) {
    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->authorization_mgr) {
        LOG_INFO("PERMISSION_CHECK", "Authorization manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    const char *args = (const char *)cmd->payload;
    if (!args || args[0] == '\0') {
        LOG_WARN("PERMISSION_CHECK", "Usage: CHECK <identity_id> <action> <resource>");
        return OZAYN_CMD_RESULT_INVALID;
    }

    char identity_id[64] = {0};
    char action[32] = {0};
    char resource[32] = {0};

    if (sscanf(args, "%63s %31s %31s", identity_id, action, resource) != 3) {
        LOG_WARN("PERMISSION_CHECK", "Invalid arguments: expected <identity> <action> <resource>");
        return OZAYN_CMD_RESULT_INVALID;
    }

    ozayn_authorization_manager_t *authz = (ozayn_authorization_manager_t *)rt->authorization_mgr;
    ozayn_authz_result_t result = ozayn_authorize(authz, identity_id, action, resource);

    LOG_INFO("PERMISSION_CHECK", "CHECK %s %s.%s -> %s (reason=%s)",
             identity_id, action, resource,
             ozayn_authz_decision_name(result.decision),
             ozayn_deny_reason_name(result.reason));

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- ROLE LIST handler ---- */

static ozayn_command_result_t handle_role_list(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->authorization_mgr) {
        LOG_INFO("ROLE_LIST", "Authorization manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_authorization_manager_t *authz = (ozayn_authorization_manager_t *)rt->authorization_mgr;

    LOG_INFO("ROLE_LIST", "--- Registered Roles (%d) ---", ozayn_authorization_role_count(authz));

    for (int i = 0; i < OZAYN_AUTHZ_MAX_ROLES; i++) {
        const ozayn_role_t *role = &authz->roles[i];
        if (role->active) {
            LOG_INFO("ROLE_LIST", "  [%d] '%s' (permissions=%d)",
                     i + 1, role->id, role->permission_count);
            for (int j = 0; j < role->permission_count; j++) {
                LOG_INFO("ROLE_LIST", "      - %s", role->permissions[j]);
            }
        }
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- HEALTH handler ---- */

static ozayn_command_result_t handle_health(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->monitoring_mgr) {
        LOG_INFO("HEALTH", "Monitoring engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_monitoring_manager_t *mon = (ozayn_monitoring_manager_t *)rt->monitoring_mgr;
    ozayn_health_state_t overall = ozayn_monitoring_overall_health(mon);

    LOG_INFO("HEALTH", "--- System Health ---");
    LOG_INFO("HEALTH", "Overall: %s", ozayn_health_state_name(overall));

    for (int i = 0; i <= OZAYN_COMP_SCHEDULER; i++) {
        ozayn_health_state_t hs = ozayn_monitoring_get_health(mon, (ozayn_component_id_t)i);
        LOG_INFO("HEALTH", "  %-20s %s", ozayn_component_name((ozayn_component_id_t)i),
                 ozayn_health_state_name(hs));
    }

    int open_incidents = ozayn_monitoring_open_incident_count(mon);
    LOG_INFO("HEALTH", "Open incidents: %d", open_incidents);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- METRICS handler ---- */

static ozayn_command_result_t handle_metrics(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->monitoring_mgr) {
        LOG_INFO("METRICS", "Monitoring engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_monitoring_manager_t *mon = (ozayn_monitoring_manager_t *)rt->monitoring_mgr;

    LOG_INFO("METRICS", "--- Registered Metrics (%d) ---", ozayn_monitoring_metric_count(mon));

    for (int i = 0; i < OZAYN_MONITOR_MAX_METRICS; i++) {
        if (mon->metrics[i].active) {
            LOG_INFO("METRICS", "  %-24s %s = %lld (%s)",
                     mon->metrics[i].name,
                     ozayn_component_name(mon->metrics[i].component),
                     (long long)mon->metrics[i].value,
                     ozayn_metric_type_name(mon->metrics[i].type));
        }
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- DIAGNOSE handler ---- */

static ozayn_command_result_t handle_diagnose(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->diagnostics_mgr) {
        LOG_INFO("DIAGNOSE", "Diagnostics engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_diagnostics_manager_t *diag = (ozayn_diagnostics_manager_t *)rt->diagnostics_mgr;

    LOG_INFO("DIAGNOSE", "--- Diagnostics State ---");
    LOG_INFO("DIAGNOSE", "Enabled: %s, Level: %s",
             ozayn_diagnostics_is_enabled(diag) ? "yes" : "no",
             ozayn_diag_level_name(ozayn_diagnostics_get_level(diag)));
    LOG_INFO("DIAGNOSE", "Evidence: %d, Findings: %d, Timeline: %d",
             ozayn_diagnostics_evidence_count(diag),
             ozayn_diagnostics_finding_count(diag),
             ozayn_diagnostics_timeline_count(diag));
    LOG_INFO("DIAGNOSE", "Sessions: %d (active=%d), Snapshots: %d",
             ozayn_diagnostics_session_count(diag),
             ozayn_diagnostics_active_session_count(diag),
             ozayn_diagnostics_snapshot_count(diag));

    /* Failure summary */
    ozayn_diagnostics_print_failure_summary(diag);

    /* Findings */
    LOG_INFO("DIAGNOSE", "--- Findings ---");
    for (int i = 0; i < OZAYN_DIAG_MAX_FINDINGS; i++) {
        if (diag->findings[i].active) {
            LOG_INFO("DIAGNOSE", "  #%u [%s] %s (cause=%s, conf=%s)",
                     diag->findings[i].id,
                     ozayn_diag_component_name(diag->findings[i].component),
                     diag->findings[i].observation,
                     diag->findings[i].possible_cause,
                     ozayn_diag_confidence_name(diag->findings[i].confidence));
        }
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SNAPSHOT handler ---- */

static ozayn_command_result_t handle_snapshot(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->diagnostics_mgr) {
        LOG_INFO("SNAPSHOT", "Diagnostics engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_diagnostics_manager_t *diag = (ozayn_diagnostics_manager_t *)rt->diagnostics_mgr;

    uint32_t snap_id = ozayn_diagnostics_snapshot_capture(diag, rt->monitoring_mgr);
    const ozayn_diag_snapshot_t *snap = ozayn_diagnostics_snapshot_get(diag, snap_id);

    if (snap) {
        LOG_INFO("SNAPSHOT", "--- Snapshot #%u ---", snap->id);
        struct tm tm_info;
        localtime_r(&snap->timestamp, &tm_info);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_info);
        LOG_INFO("SNAPSHOT", "Time: %s", timebuf);
        LOG_INFO("SNAPSHOT", "Overall health: %s",
                 ozayn_diag_health_name(snap->overall_health));
        LOG_INFO("SNAPSHOT", "Open incidents: %d", snap->open_incidents);

        for (int c = 0; c <= OZAYN_DIAG_COMP_SCHEDULER; c++) {
            ozayn_diag_health_t hs = snap->component_health[c];
            if (hs != OZAYN_DIAG_HEALTH_UNKNOWN) {
                LOG_INFO("SNAPSHOT", "  %-20s %s",
                         ozayn_diag_component_name((ozayn_diag_component_t)c),
                         ozayn_diag_health_name(hs));
            }
        }
    }

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- INCIDENTS handler ---- */

static ozayn_command_result_t handle_incidents(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->monitoring_mgr) {
        LOG_INFO("INCIDENTS", "Monitoring engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_monitoring_manager_t *mon = (ozayn_monitoring_manager_t *)rt->monitoring_mgr;

    LOG_INFO("INCIDENTS", "--- Incidents ---");
    int open = 0;
    for (int i = 0; i < OZAYN_MONITOR_MAX_INCIDENTS; i++) {
        const ozayn_incident_t *inc = &mon->incidents[i];
        if (inc->active) {
            LOG_INFO("INCIDENTS", "  #%u [%s] %s — %s",
                     inc->id,
                     ozayn_component_name(inc->component),
                     ozayn_severity_name(inc->severity),
                     inc->reason);
            if (inc->state != OZAYN_INCIDENT_RESOLVED) open++;
        }
    }
    LOG_INFO("INCIDENTS", "Open: %d", open);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- TRACE handler ---- */

static ozayn_command_result_t handle_trace(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->diagnostics_mgr) {
        LOG_INFO("TRACE", "Diagnostics engine not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_diagnostics_manager_t *diag = (ozayn_diagnostics_manager_t *)rt->diagnostics_mgr;

    ozayn_diagnostics_timeline_print(diag, NULL);

    LOG_INFO("TRACE", "--- Sessions ---");
    for (int i = 0; i < OZAYN_DIAG_MAX_SESSIONS; i++) {
        if (diag->sessions[i].active) {
            LOG_INFO("TRACE", "  Session #%u [%s] %s (evidence=%d, findings=%d)",
                     diag->sessions[i].id,
                     ozayn_diag_target_name(diag->sessions[i].target),
                     ozayn_diag_session_state_name(diag->sessions[i].state),
                     diag->sessions[i].evidence_count,
                     diag->sessions[i].finding_count);
        }
    }

    ozayn_diag_stats_t stats = ozayn_diagnostics_stats(diag);
    LOG_INFO("TRACE", "--- Statistics ---");
    LOG_INFO("TRACE", "Evidence: %d, Findings: %d, Timeline: %d",
             stats.evidence_recorded, stats.findings_generated, stats.timeline_entries);
    LOG_INFO("TRACE", "Sessions: %d created, %d completed",
             stats.sessions_created, stats.sessions_completed);
    LOG_INFO("TRACE", "Snapshots: %d, Redactions: %d, Repeated failures: %d",
             stats.snapshots_captured, stats.redactions_applied,
             stats.repeated_failures_detected);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SEC STATUS handler ---- */

static ozayn_command_result_t handle_sec_status(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_boundary_mgr) {
        LOG_INFO("SEC_STATUS", "Security boundary not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_security_boundary_manager_t *sb =
        (ozayn_security_boundary_manager_t *)rt->security_boundary_mgr;

    LOG_INFO("SEC_STATUS", "--- Security Boundary Status ---");
    LOG_INFO("SEC_STATUS", "  Enabled:    %s", sb->policy.enabled ? "yes" : "no");
    LOG_INFO("SEC_STATUS", "  Fail closed: %s", sb->policy.fail_closed ? "yes" : "no");
    LOG_INFO("SEC_STATUS", "  Contexts:   %d", sb->context_count);
    LOG_INFO("SEC_STATUS", "  Violations: %d", sb->violation_count);

    ozayn_security_boundary_stats_t stats = ozayn_security_boundary_stats(sb);
    LOG_INFO("SEC_STATUS", "  Checks:     %d (allowed=%d, denied=%d)",
             stats.total_checks, stats.total_allowed, stats.total_denied);
    LOG_INFO("SEC_STATUS", "  Restricted: %d, Isolated: %d",
             stats.restricted_components, stats.isolated_components);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SEC CONTEXTS handler ---- */

static ozayn_command_result_t handle_sec_contexts(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_boundary_mgr) {
        LOG_INFO("SEC_CONTEXTS", "Security boundary not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_security_boundary_manager_t *sb =
        (ozayn_security_boundary_manager_t *)rt->security_boundary_mgr;

    ozayn_security_boundary_print_contexts(sb);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SEC CHECK handler ---- */

static ozayn_command_result_t handle_sec_check(const ozayn_command_t *cmd, void *ctx) {
    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_boundary_mgr) {
        LOG_INFO("SEC_CHECK", "Security boundary not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    const char *args = (const char *)cmd->payload;
    if (!args || args[0] == '\0') {
        LOG_WARN("SEC_CHECK", "Usage: CHECK <context_id> <capability>");
        return OZAYN_CMD_RESULT_INVALID;
    }

    uint32_t ctx_id = 0;
    char cap_name[32] = {0};

    if (sscanf(args, "%u %31s", &ctx_id, cap_name) != 2) {
        LOG_WARN("SEC_CHECK", "Invalid arguments: expected <context_id> <capability>");
        return OZAYN_CMD_RESULT_INVALID;
    }

    /* Map capability name to ID */
    ozayn_capability_id_t cap = OZAYN_CAP_NONE;
    for (int i = 1; i < OZAYN_CAP_COUNT; i++) {
        if (strcmp(ozayn_capability_name((ozayn_capability_id_t)i), cap_name) == 0) {
            cap = (ozayn_capability_id_t)i;
            break;
        }
    }

    if (cap == OZAYN_CAP_NONE) {
        LOG_WARN("SEC_CHECK", "Unknown capability: %s", cap_name);
        return OZAYN_CMD_RESULT_INVALID;
    }

    ozayn_security_boundary_manager_t *sb =
        (ozayn_security_boundary_manager_t *)rt->security_boundary_mgr;

    ozayn_security_check_result_t result = ozayn_security_boundary_check(sb, ctx_id, cap);

    LOG_INFO("SEC_CHECK", "CHECK ctx=%u %s -> %s (reason=%s)",
             ctx_id, ozayn_capability_name(cap),
             result.allowed ? "ALLOWED" : "DENIED",
             result.reason);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- SEC VIOLATIONS handler ---- */

static ozayn_command_result_t handle_sec_violations(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->security_boundary_mgr) {
        LOG_INFO("SEC_VIOLATIONS", "Security boundary not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_security_boundary_manager_t *sb =
        (ozayn_security_boundary_manager_t *)rt->security_boundary_mgr;

    ozayn_security_boundary_print_violations(sb);

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- STATE STATUS handler ---- */

static ozayn_command_result_t handle_state_status(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->state_mgr) {
        LOG_INFO("STATE_STATUS", "State manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_state_manager_t *mgr = (ozayn_state_manager_t *)rt->state_mgr;

    LOG_INFO("STATE_STATUS", "--- State Manager Status ---");
    LOG_INFO("STATE_STATUS", "  Enabled: %s", mgr->enabled ? "yes" : "no");
    LOG_INFO("STATE_STATUS", "  Storage: %s", mgr->storage_path);
    LOG_INFO("STATE_STATUS", "  Entries: %d", ozayn_state_count(mgr));
    LOG_INFO("STATE_STATUS", "  Dirty: %d", ozayn_state_dirty_count(mgr));
    LOG_INFO("STATE_STATUS", "  Saves: %d, Loads: %d", mgr->total_saves, mgr->total_loads);

    ozayn_state_validation_t val = ozayn_state_validate(mgr);
    LOG_INFO("STATE_STATUS", "  File: %s", ozayn_state_validation_name(val));

    return OZAYN_CMD_RESULT_SUCCESS;
}

/* ---- STATE SAVE handler ---- */

static ozayn_command_result_t handle_state_save(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->state_mgr) {
        LOG_INFO("STATE_SAVE", "State manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_state_manager_t *mgr = (ozayn_state_manager_t *)rt->state_mgr;

    int r = ozayn_state_save(mgr);
    LOG_INFO("STATE_SAVE", "Save: %s (entries=%d)", r == 0 ? "OK" : "FAILED", ozayn_state_count(mgr));

    return r == 0 ? OZAYN_CMD_RESULT_SUCCESS : OZAYN_CMD_RESULT_FAILURE;
}

/* ---- STATE LOAD handler ---- */

static ozayn_command_result_t handle_state_load(const ozayn_command_t *cmd, void *ctx) {
    (void)cmd;

    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->state_mgr) {
        LOG_INFO("STATE_LOAD", "State manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_state_manager_t *mgr = (ozayn_state_manager_t *)rt->state_mgr;

    int r = ozayn_state_load(mgr);
    LOG_INFO("STATE_LOAD", "Load: %s (entries=%d)", r == 0 ? "OK" : "FAILED", ozayn_state_count(mgr));

    return r == 0 ? OZAYN_CMD_RESULT_SUCCESS : OZAYN_CMD_RESULT_FAILURE;
}

/* ---- STATE INFO handler ---- */

static ozayn_command_result_t handle_state_info(const ozayn_command_t *cmd, void *ctx) {
    ozayn_command_engine_t *engine = (ozayn_command_engine_t *)ctx;
    ozayn_runtime_t *rt = (ozayn_runtime_t *)engine->runtime;

    if (!rt || !rt->state_mgr) {
        LOG_INFO("STATE_INFO", "State manager not available");
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    ozayn_state_manager_t *mgr = (ozayn_state_manager_t *)rt->state_mgr;

    const char *key = (const char *)cmd->payload;
    if (!key || key[0] == '\0') {
        /* Show all entries */
        ozayn_state_manager_print_entries(mgr);
        return OZAYN_CMD_RESULT_SUCCESS;
    }

    const ozayn_state_entry_t *e = ozayn_state_get(mgr, key);
    if (!e) {
        LOG_INFO("STATE_INFO", "State key '%s' not found", key);
        return OZAYN_CMD_RESULT_NOT_FOUND;
    }

    LOG_INFO("STATE_INFO", "--- State: %s ---", e->key);
    LOG_INFO("STATE_INFO", "  ID:       %u", e->id);
    LOG_INFO("STATE_INFO", "  Owner:    %s", e->owner);
    LOG_INFO("STATE_INFO", "  Namespace: %s", ozayn_state_namespace_name(e->ns));
    LOG_INFO("STATE_INFO", "  Category: %s", ozayn_state_category_name(e->category));
    LOG_INFO("STATE_INFO", "  Recovery: %s", ozayn_state_recovery_name(e->recovery));
    LOG_INFO("STATE_INFO", "  Version:  %u", e->version);
    LOG_INFO("STATE_INFO", "  Size:     %u bytes", e->data_size);
    LOG_INFO("STATE_INFO", "  Flags:    %s%s",
             (e->flags & OZAYN_STATE_FLAG_DIRTY) ? "DIRTY " : "",
             (e->flags & OZAYN_STATE_FLAG_SEALED) ? "SEALED " : "");

    return OZAYN_CMD_RESULT_SUCCESS;
}
