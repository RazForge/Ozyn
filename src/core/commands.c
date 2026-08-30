#include "commands.h"
#include "logger.h"
#include "recovery.h"
#include "runtime.h"
#include "events.h"
#include "registry.h"
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

/* ---- Built-in command registry ---- */

static const ozayn_command_entry_t builtin_registry[] = {
    { OZAYN_CMD_STATUS,         handle_status,         "STATUS"         },
    { OZAYN_CMD_STOP,           handle_stop,           "STOP"           },
    { OZAYN_CMD_SERVICE_LIST,   handle_service_list,   "SERVICE LIST"   },
    { OZAYN_CMD_SERVICE_STATUS, handle_service_status, "SERVICE STATUS" },
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
