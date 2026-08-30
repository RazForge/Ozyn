#ifndef OZAYN_SECURITY_BOUNDARY_H
#define OZAYN_SECURITY_BOUNDARY_H

#include <stdint.h>
#include <time.h>

/*
 * security_boundary.h — Security & Isolation Boundary (Stage 20).
 *
 * Self-contained header — no circular includes.
 * Defines trust levels, capabilities, component security contexts,
 * violation tracking, and enforcement policies.
 *
 * Stage 15 (Authorization) answers: "Can component X perform operation Y?"
 * Stage 20 (Security Boundary) answers: "Even if allowed, what is component X
 * confined to — resources, interfaces, files, capabilities?"
 */

/* ---- Trust levels (component isolation trust, distinct from identity trust) ---- */
typedef enum {
    OZAYN_SB_TRUST_CORE       = 0,  /* Runtime, scheduler, task mgr, resource mgr */
    OZAYN_SB_TRUST_TRUSTED    = 1,  /* Essential internal modules */
    OZAYN_SB_TRUST_SYSTEM     = 2,  /* System services with defined roles */
    OZAYN_SB_TRUST_CONTROLLED = 3,  /* Managed modules/plugins */
    OZAYN_SB_TRUST_LIMITED    = 4,  /* Third-party, reduced capabilities */
    OZAYN_SB_TRUST_UNTRUSTED  = 5,  /* Unknown / sandbox-only */
} ozayn_sb_trust_level_t;

/* ---- Capability IDs ---- */
typedef enum {
    OZAYN_CAP_NONE                = 0,

    /* Resource access */
    OZAYN_CAP_CAMERA_READ         = 1,
    OZAYN_CAP_CAMERA_CONTROL      = 2,
    OZAYN_CAP_MICROPHONE_READ     = 3,
    OZAYN_CAP_AUDIO_PROCESS       = 4,

    /* Communication */
    OZAYN_CAP_IPC_SEND            = 10,
    OZAYN_CAP_IPC_RECEIVE         = 11,
    OZAYN_CAP_IPC_BROADCAST       = 12,
    OZAYN_CAP_EVENT_PUBLISH       = 13,
    OZAYN_CAP_EVENT_SUBSCRIBE     = 14,

    /* Tasks */
    OZAYN_CAP_TASK_CREATE         = 20,
    OZAYN_CAP_TASK_CANCEL         = 21,

    /* Process */
    OZAYN_CAP_PROCESS_START       = 30,
    OZAYN_CAP_PROCESS_STOP        = 31,

    /* Module/Plugin management */
    OZAYN_CAP_MODULE_LOAD         = 40,
    OZAYN_CAP_MODULE_UNLOAD       = 41,
    OZAYN_CAP_PLUGIN_LOAD         = 42,
    OZAYN_CAP_PLUGIN_UNLOAD       = 43,

    /* Configuration */
    OZAYN_CAP_CONFIG_READ         = 50,
    OZAYN_CAP_CONFIG_WRITE        = 51,

    /* Security (sensitive) */
    OZAYN_CAP_SECURITY_READ       = 60,
    OZAYN_CAP_SECURITY_ADMIN      = 61,
    OZAYN_CAP_IDENTITY_MANAGE     = 62,
    OZAYN_CAP_POLICY_MODIFY       = 63,

    /* Diagnostics */
    OZAYN_CAP_METRICS_READ        = 70,
    OZAYN_CAP_DIAGNOSTICS_READ    = 71,

    /* System */
    OZAYN_CAP_CORE_SHUTDOWN       = 80,
    OZAYN_CAP_RESOURCE_CREATE     = 81,
    OZAYN_CAP_RESOURCE_ALLOCATE   = 82,
    OZAYN_CAP_RESOURCE_RELEASE    = 83,

    OZAYN_CAP_COUNT
} ozayn_capability_id_t;

/* ---- Component security states ---- */
typedef enum {
    OZAYN_COMP_SEC_NORMAL     = 0,
    OZAYN_COMP_SEC_RESTRICTED = 1,
    OZAYN_COMP_SEC_SUSPENDED  = 2,
    OZAYN_COMP_SEC_ISOLATED   = 3,
    OZAYN_COMP_SEC_BLOCKED    = 4,
} ozayn_component_security_state_t;

/* ---- Violation severity ---- */
typedef enum {
    OZAYN_SEC_SEV_LOW      = 0,
    OZAYN_SEC_SEV_MEDIUM   = 1,
    OZAYN_SEC_SEV_HIGH     = 2,
    OZAYN_SEC_SEV_CRITICAL = 3,
} ozayn_security_severity_t;

/* ---- Violation types ---- */
typedef enum {
    OZAYN_VIOLATION_NONE                = 0,
    OZAYN_VIOLATION_CAPABILITY_DENIED   = 1,
    OZAYN_VIOLATION_PRIVILEGE_ESCALATION = 2,
    OZAYN_VIOLATION_SANDBOX_BREACH      = 3,
    OZAYN_VIOLATION_RESOURCE_ABUSE      = 4,
    OZAYN_VIOLATION_RATE_EXCEEDED       = 5,
    OZAYN_VIOLATION_UNAUTHORIZED_ACCESS = 6,
    OZAYN_VIOLATION_INTEGRITY_FAIL      = 7,
    OZAYN_VIOLATION_CONFIG_TAMPER       = 8,
    OZAYN_VIOLATION_IPC_BREACH          = 9,
    OZAYN_VIOLATION_TASK_ABUSE          = 10,
} ozayn_violation_type_t;

/* ---- Security actions (response to violations) ---- */
typedef enum {
    OZAYN_SEC_ACTION_NONE      = 0,
    OZAYN_SEC_ACTION_LOG       = 1,
    OZAYN_SEC_ACTION_WARN      = 2,
    OZAYN_SEC_ACTION_BLOCK     = 3,
    OZAYN_SEC_ACTION_RESTRICT  = 4,
    OZAYN_SEC_ACTION_ISOLATE   = 5,
    OZAYN_SEC_ACTION_DISABLE   = 6,
} ozayn_security_action_t;

/* ---- Capability entry ---- */
typedef struct {
    ozayn_capability_id_t id;
    int                   granted;
} ozayn_capability_entry_t;

/* ---- Resource limits ---- */
typedef struct {
    int max_tasks;
    int max_resources;
    int max_ipc_channels;
    int64_t max_memory_bytes;
    int max_cpu_percent;
    int max_request_rate;
} ozayn_resource_limits_t;

/* ---- Violation record ---- */
typedef struct {
    int                          active;
    uint32_t                     id;
    ozayn_violation_type_t       type;
    ozayn_security_severity_t    severity;
    char                         component_id[64];
    char                         description[128];
    ozayn_security_action_t      action_taken;
    time_t                       timestamp;
} ozayn_violation_record_t;

/* ---- Component security context ---- */
typedef struct {
    int                                  active;
    char                                 component_id[64];
    ozayn_sb_trust_level_t                  trust_level;
    ozayn_component_security_state_t     state;
    ozayn_capability_entry_t             capabilities[OZAYN_CAP_COUNT];
    int                                  capability_count;
    ozayn_resource_limits_t              limits;
    int                                  violation_count;
    int                                  requests_allowed;
    int                                  requests_denied;
    time_t                               created_at;
    time_t                               last_activity;
} ozayn_security_context_t;

/* ---- Security policy ---- */
typedef struct {
    int                                  enabled;
    int                                  fail_closed;
    int                                  max_violations_before_restrict;
    int                                  max_violations_before_isolate;
    int                                  max_violations_before_disable;
    ozayn_security_severity_t            escalation_threshold;
    int                                  audit_logging;
} ozayn_security_policy_t;

/* ---- Security boundary manager ---- */
typedef struct {
    ozayn_security_context_t    contexts[32];
    int                        context_count;
    ozayn_violation_record_t   violations[128];
    int                        violation_count;
    uint32_t                   next_violation_id;
    uint32_t                   next_context_id;
    ozayn_security_policy_t    policy;
    void                      *events;
    void                      *recovery;
    int                        total_checks;
    int                        total_allowed;
    int                        total_denied;
    int                        initialized;
} ozayn_security_boundary_manager_t;

/* ---- Name helpers ---- */
const char *ozayn_sb_trust_level_name(ozayn_sb_trust_level_t level);
const char *ozayn_capability_name(ozayn_capability_id_t cap);
const char *ozayn_component_sec_state_name(ozayn_component_security_state_t state);
const char *ozayn_security_severity_name(ozayn_security_severity_t sev);
const char *ozayn_violation_type_name(ozayn_violation_type_t type);
const char *ozayn_security_action_name(ozayn_security_action_t action);

/* ---- Lifecycle ---- */
int  ozayn_security_boundary_init(ozayn_security_boundary_manager_t *mgr, int enabled);
void ozayn_security_boundary_shutdown(ozayn_security_boundary_manager_t *mgr);
int  ozayn_security_boundary_is_enabled(const ozayn_security_boundary_manager_t *mgr);

/* ---- Policy ---- */
void ozayn_security_boundary_set_policy(ozayn_security_boundary_manager_t *mgr,
                                         const ozayn_security_policy_t *policy);
const ozayn_security_policy_t *ozayn_security_boundary_get_policy(
    const ozayn_security_boundary_manager_t *mgr);

/* ---- Context management ---- */
uint32_t ozayn_security_boundary_register_context(ozayn_security_boundary_manager_t *mgr,
                                                    const char *component_id,
                                                    ozayn_sb_trust_level_t trust);
int      ozayn_security_boundary_unregister_context(ozayn_security_boundary_manager_t *mgr,
                                                      uint32_t context_id);
ozayn_security_context_t *ozayn_security_boundary_get_context(
    ozayn_security_boundary_manager_t *mgr, uint32_t context_id);
ozayn_security_context_t *ozayn_security_boundary_find_context(
    ozayn_security_boundary_manager_t *mgr, const char *component_id);
int ozayn_security_boundary_context_count(const ozayn_security_boundary_manager_t *mgr);

/* ---- Capability management ---- */
int ozayn_security_boundary_grant_capability(ozayn_security_boundary_manager_t *mgr,
                                              uint32_t context_id,
                                              ozayn_capability_id_t cap);
int ozayn_security_boundary_revoke_capability(ozayn_security_boundary_manager_t *mgr,
                                               uint32_t context_id,
                                               ozayn_capability_id_t cap);
int ozayn_security_boundary_has_capability(const ozayn_security_boundary_manager_t *mgr,
                                            uint32_t context_id,
                                            ozayn_capability_id_t cap);

/* ---- Enforcement ---- */
typedef struct {
    int                          allowed;
    ozayn_violation_type_t       violation;
    ozayn_security_severity_t    severity;
    ozayn_security_action_t      action;
    const char                  *reason;
} ozayn_security_check_result_t;

ozayn_security_check_result_t ozayn_security_boundary_check(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    ozayn_capability_id_t requested_cap);

ozayn_security_check_result_t ozayn_security_boundary_check_ipc(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    const char *target_component);

ozayn_security_check_result_t ozayn_security_boundary_check_resource(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    const char *resource_id);

ozayn_security_check_result_t ozayn_security_boundary_check_task(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id);

/* ---- Violations ---- */
uint32_t ozayn_security_boundary_report_violation(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    ozayn_violation_type_t type,
    ozayn_security_severity_t severity,
    const char *description);

int ozayn_security_boundary_violation_count(
    const ozayn_security_boundary_manager_t *mgr);
const ozayn_violation_record_t *ozayn_security_boundary_get_violation(
    const ozayn_security_boundary_manager_t *mgr, int index);

/* ---- State transitions ---- */
int ozayn_security_boundary_restrict(ozayn_security_boundary_manager_t *mgr,
                                      uint32_t context_id);
int ozayn_security_boundary_isolate(ozayn_security_boundary_manager_t *mgr,
                                     uint32_t context_id);
int ozayn_security_boundary_restore(ozayn_security_boundary_manager_t *mgr,
                                     uint32_t context_id);

/* ---- Resource limits ---- */
void ozayn_security_boundary_set_limits(ozayn_security_boundary_manager_t *mgr,
                                         uint32_t context_id,
                                         const ozayn_resource_limits_t *limits);
const ozayn_resource_limits_t *ozayn_security_boundary_get_limits(
    const ozayn_security_boundary_manager_t *mgr, uint32_t context_id);

/* ---- Security context inheritance (for tasks) ---- */
uint32_t ozayn_security_boundary_inherit_context(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t parent_context_id);

/* ---- Statistics ---- */
typedef struct {
    int total_checks;
    int total_allowed;
    int total_denied;
    int total_violations;
    int components_registered;
    int isolated_components;
    int restricted_components;
} ozayn_security_boundary_stats_t;

ozayn_security_boundary_stats_t ozayn_security_boundary_stats(
    const ozayn_security_boundary_manager_t *mgr);

/* ---- Print helpers ---- */
void ozayn_security_boundary_print_contexts(
    const ozayn_security_boundary_manager_t *mgr);
void ozayn_security_boundary_print_violations(
    const ozayn_security_boundary_manager_t *mgr);
void ozayn_security_boundary_print_stats(
    const ozayn_security_boundary_manager_t *mgr);

#endif
