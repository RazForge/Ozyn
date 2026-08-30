#ifndef OZAYN_COMMANDS_H
#define OZAYN_COMMANDS_H

#include "ozayn.h"
#include <stdint.h>

/*
 * ozayn_commands.h — Command engine.
 *
 * Receives intentional requests, validates them, executes or routes them,
 * and produces a result. The inverse of the Event Engine.
 *
 * Event Engine: "What happened?"   (observe)
 * Command Engine: "What should OZAYN do?" (act)
 */

/* ---- Command types ---- */
typedef enum {
    OZAYN_CMD_NONE              = 0,
    OZAYN_CMD_STATUS            = 1,
    OZAYN_CMD_STOP              = 2,
    OZAYN_CMD_SERVICE_LIST      = 3,
    OZAYN_CMD_SERVICE_STATUS    = 4,
    OZAYN_CMD_AUTH_STATUS       = 5,
    OZAYN_CMD_IDENTITY_LIST     = 6,
    OZAYN_CMD_PERMISSION_CHECK  = 7,
    OZAYN_CMD_ROLE_LIST         = 8,
    OZAYN_CMD_HEALTH            = 9,
    OZAYN_CMD_METRICS           = 10,
} ozayn_command_type_t;

/* ---- Command source ---- */
typedef enum {
    OZAYN_CMD_SRC_CORE   = 0,
    OZAYN_CMD_SRC_CLI    = 1,
} ozayn_command_source_t;

/* ---- Command status (lifecycle) ---- */
typedef enum {
    OZAYN_CMD_RECEIVED   = 0,
    OZAYN_CMD_VALIDATED  = 1,
    OZAYN_CMD_EXECUTING  = 2,
    OZAYN_CMD_COMPLETED  = 3,
    OZAYN_CMD_FAILED     = 4,
} ozayn_command_status_t;

/* ---- Command result ---- */
typedef enum {
    OZAYN_CMD_RESULT_SUCCESS    = 0,
    OZAYN_CMD_RESULT_FAILURE    = 1,
    OZAYN_CMD_RESULT_REJECTED   = 2,
    OZAYN_CMD_RESULT_INVALID    = 3,
    OZAYN_CMD_RESULT_NOT_FOUND  = 4,
} ozayn_command_result_t;

/* ---- Command structure ---- */
typedef struct {
    ozayn_command_type_t    type;
    uint32_t                id;
    ozayn_command_source_t  source;
    const void             *payload;
    size_t                  payload_size;
    ozayn_command_status_t  status;
    ozayn_command_result_t  result;
} ozayn_command_t;

/* ---- Handler function type ---- */
/* Context is the command engine (gives access to runtime, events, recovery) */
typedef ozayn_command_result_t (*ozayn_command_handler_t)(
    const ozayn_command_t *cmd, void *context);

/* ---- Registry entry ---- */
typedef struct {
    ozayn_command_type_t    type;
    ozayn_command_handler_t handler;
    const char             *name;
} ozayn_command_entry_t;

/* ---- Command engine ---- */
typedef struct {
    const ozayn_command_entry_t *registry;
    int                         registry_size;
    void                       *runtime;   /* cast to ozayn_runtime_t* in .c */
    void                       *events;    /* cast to ozayn_event_engine_t* in .c */
    void                       *recovery;  /* cast to ozayn_recovery_t* in .c */
    uint32_t                    next_id;
    int                         initialized;
} ozayn_command_engine_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_command_engine_init(ozayn_command_engine_t *engine);
void           ozayn_command_engine_shutdown(ozayn_command_engine_t *engine);

/* ---- Binding ---- */
void ozayn_command_engine_set_runtime(ozayn_command_engine_t *engine, void *runtime);
void ozayn_command_engine_set_events(ozayn_command_engine_t *engine, void *events);
void ozayn_command_engine_set_recovery(ozayn_command_engine_t *engine, void *recovery);

/* ---- Create command ---- */
ozayn_command_t ozayn_command_create(ozayn_command_type_t type,
                                     ozayn_command_source_t source);

/* ---- Execute ---- */
ozayn_command_result_t ozayn_command_engine_execute(
    ozayn_command_engine_t *engine,
    ozayn_command_t *cmd);

/* ---- Query ---- */
const char *ozayn_command_type_name(ozayn_command_type_t type);
const char *ozayn_command_source_name(ozayn_command_source_t src);
const char *ozayn_command_status_name(ozayn_command_status_t status);
const char *ozayn_command_result_name(ozayn_command_result_t result);

#endif
