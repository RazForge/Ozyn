#include "ozayn.h"
#include "security_boundary.h"
#include "logger.h"
#include "recovery.h"
#include "events.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Name helpers
 * ================================================================ */

const char *ozayn_sb_trust_level_name(ozayn_sb_trust_level_t level) {
    switch (level) {
        case OZAYN_SB_TRUST_CORE:       return "CORE";
        case OZAYN_SB_TRUST_TRUSTED:    return "TRUSTED";
        case OZAYN_SB_TRUST_SYSTEM:     return "SYSTEM";
        case OZAYN_SB_TRUST_CONTROLLED: return "CONTROLLED";
        case OZAYN_SB_TRUST_LIMITED:    return "LIMITED";
        case OZAYN_SB_TRUST_UNTRUSTED:  return "UNTRUSTED";
    }
    return "UNKNOWN";
}

const char *ozayn_capability_name(ozayn_capability_id_t cap) {
    switch (cap) {
        case OZAYN_CAP_NONE:              return "NONE";
        case OZAYN_CAP_CAMERA_READ:       return "camera.read";
        case OZAYN_CAP_CAMERA_CONTROL:    return "camera.control";
        case OZAYN_CAP_MICROPHONE_READ:   return "microphone.read";
        case OZAYN_CAP_AUDIO_PROCESS:     return "audio.process";
        case OZAYN_CAP_IPC_SEND:          return "ipc.send";
        case OZAYN_CAP_IPC_RECEIVE:       return "ipc.receive";
        case OZAYN_CAP_IPC_BROADCAST:     return "ipc.broadcast";
        case OZAYN_CAP_EVENT_PUBLISH:     return "event.publish";
        case OZAYN_CAP_EVENT_SUBSCRIBE:   return "event.subscribe";
        case OZAYN_CAP_TASK_CREATE:       return "task.create";
        case OZAYN_CAP_TASK_CANCEL:       return "task.cancel";
        case OZAYN_CAP_PROCESS_START:     return "process.start";
        case OZAYN_CAP_PROCESS_STOP:      return "process.stop";
        case OZAYN_CAP_MODULE_LOAD:       return "module.load";
        case OZAYN_CAP_MODULE_UNLOAD:     return "module.unload";
        case OZAYN_CAP_PLUGIN_LOAD:       return "plugin.load";
        case OZAYN_CAP_PLUGIN_UNLOAD:     return "plugin.unload";
        case OZAYN_CAP_CONFIG_READ:       return "config.read";
        case OZAYN_CAP_CONFIG_WRITE:      return "config.write";
        case OZAYN_CAP_SECURITY_READ:     return "security.read";
        case OZAYN_CAP_SECURITY_ADMIN:    return "security.admin";
        case OZAYN_CAP_IDENTITY_MANAGE:   return "identity.manage";
        case OZAYN_CAP_POLICY_MODIFY:     return "policy.modify";
        case OZAYN_CAP_METRICS_READ:      return "metrics.read";
        case OZAYN_CAP_DIAGNOSTICS_READ:  return "diagnostics.read";
        case OZAYN_CAP_CORE_SHUTDOWN:     return "core.shutdown";
        case OZAYN_CAP_RESOURCE_CREATE:   return "resource.create";
        case OZAYN_CAP_RESOURCE_ALLOCATE: return "resource.allocate";
        case OZAYN_CAP_RESOURCE_RELEASE:  return "resource.release";
        default:                          return "UNKNOWN";
    }
}

const char *ozayn_component_sec_state_name(ozayn_component_security_state_t state) {
    switch (state) {
        case OZAYN_COMP_SEC_NORMAL:     return "NORMAL";
        case OZAYN_COMP_SEC_RESTRICTED: return "RESTRICTED";
        case OZAYN_COMP_SEC_SUSPENDED:  return "SUSPENDED";
        case OZAYN_COMP_SEC_ISOLATED:   return "ISOLATED";
        case OZAYN_COMP_SEC_BLOCKED:    return "BLOCKED";
    }
    return "UNKNOWN";
}

const char *ozayn_security_severity_name(ozayn_security_severity_t sev) {
    switch (sev) {
        case OZAYN_SEC_SEV_LOW:      return "LOW";
        case OZAYN_SEC_SEV_MEDIUM:   return "MEDIUM";
        case OZAYN_SEC_SEV_HIGH:     return "HIGH";
        case OZAYN_SEC_SEV_CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char *ozayn_violation_type_name(ozayn_violation_type_t type) {
    switch (type) {
        case OZAYN_VIOLATION_NONE:                 return "NONE";
        case OZAYN_VIOLATION_CAPABILITY_DENIED:    return "CAPABILITY_DENIED";
        case OZAYN_VIOLATION_PRIVILEGE_ESCALATION: return "PRIVILEGE_ESCALATION";
        case OZAYN_VIOLATION_SANDBOX_BREACH:       return "SANDBOX_BREACH";
        case OZAYN_VIOLATION_RESOURCE_ABUSE:       return "RESOURCE_ABUSE";
        case OZAYN_VIOLATION_RATE_EXCEEDED:        return "RATE_EXCEEDED";
        case OZAYN_VIOLATION_UNAUTHORIZED_ACCESS:  return "UNAUTHORIZED_ACCESS";
        case OZAYN_VIOLATION_INTEGRITY_FAIL:       return "INTEGRITY_FAIL";
        case OZAYN_VIOLATION_CONFIG_TAMPER:        return "CONFIG_TAMPER";
        case OZAYN_VIOLATION_IPC_BREACH:           return "IPC_BREACH";
        case OZAYN_VIOLATION_TASK_ABUSE:           return "TASK_ABUSE";
    }
    return "UNKNOWN";
}

const char *ozayn_security_action_name(ozayn_security_action_t action) {
    switch (action) {
        case OZAYN_SEC_ACTION_NONE:     return "NONE";
        case OZAYN_SEC_ACTION_LOG:      return "LOG";
        case OZAYN_SEC_ACTION_WARN:     return "WARN";
        case OZAYN_SEC_ACTION_BLOCK:    return "BLOCK";
        case OZAYN_SEC_ACTION_RESTRICT: return "RESTRICT";
        case OZAYN_SEC_ACTION_ISOLATE:  return "ISOLATE";
        case OZAYN_SEC_ACTION_DISABLE:  return "DISABLE";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Internal: find first free slot
 * ================================================================ */

static int find_free_context_slot(const ozayn_security_boundary_manager_t *mgr) {
    for (int i = 0; i < 32; i++) {
        if (!mgr->contexts[i].active) return i;
    }
    return -1;
}

static int find_context_by_id(const ozayn_security_boundary_manager_t *mgr,
                               uint32_t context_id) {
    for (int i = 0; i < 32; i++) {
        if (mgr->contexts[i].active && mgr->contexts[i].created_at == (time_t)context_id)
            return i;
    }
    return -1;
}

static int find_free_violation_slot(ozayn_security_boundary_manager_t *mgr) {
    for (int i = 0; i < 128; i++) {
        if (!mgr->violations[i].active) return i;
    }
    return -1;
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

int ozayn_security_boundary_init(ozayn_security_boundary_manager_t *mgr, int enabled) {
    if (!mgr) return -1;

    memset(mgr, 0, sizeof(ozayn_security_boundary_manager_t));

    mgr->policy.enabled = enabled;
    mgr->policy.fail_closed = 1;
    mgr->policy.max_violations_before_restrict = 3;
    mgr->policy.max_violations_before_isolate = 6;
    mgr->policy.max_violations_before_disable = 10;
    mgr->policy.escalation_threshold = OZAYN_SEC_SEV_HIGH;
    mgr->policy.audit_logging = 1;

    mgr->next_violation_id = 1;
    mgr->next_context_id = 1;
    mgr->initialized = 1;

    LOG_INFO("SECURITY_BOUNDARY", "Security boundary initialized (enabled=%s, fail_closed=%s)",
             enabled ? "yes" : "no",
             mgr->policy.fail_closed ? "yes" : "no");

    return 0;
}

void ozayn_security_boundary_shutdown(ozayn_security_boundary_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    LOG_INFO("SECURITY_BOUNDARY", "Security boundary shut down (contexts=%d, violations=%d)",
             mgr->context_count, mgr->violation_count);

    mgr->initialized = 0;
}

int ozayn_security_boundary_is_enabled(const ozayn_security_boundary_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->policy.enabled;
}

/* ================================================================
 * Policy
 * ================================================================ */

void ozayn_security_boundary_set_policy(ozayn_security_boundary_manager_t *mgr,
                                         const ozayn_security_policy_t *policy) {
    if (!mgr || !policy) return;
    mgr->policy = *policy;
    LOG_INFO("SECURITY_BOUNDARY", "Security policy updated (fail_closed=%s, restrict=%d, isolate=%d, disable=%d)",
             policy->fail_closed ? "yes" : "no",
             policy->max_violations_before_restrict,
             policy->max_violations_before_isolate,
             policy->max_violations_before_disable);
}

const ozayn_security_policy_t *ozayn_security_boundary_get_policy(
    const ozayn_security_boundary_manager_t *mgr)
{
    if (!mgr) return NULL;
    return &mgr->policy;
}

/* ================================================================
 * Context management
 * ================================================================ */

uint32_t ozayn_security_boundary_register_context(ozayn_security_boundary_manager_t *mgr,
                                                    const char *component_id,
                                                    ozayn_sb_trust_level_t trust) {
    if (!mgr || !mgr->initialized || !component_id) return 0;

    /* Check for duplicate */
    for (int i = 0; i < 32; i++) {
        if (mgr->contexts[i].active &&
            strcmp(mgr->contexts[i].component_id, component_id) == 0) {
            LOG_WARN("SECURITY_BOUNDARY", "Context for '%s' already registered", component_id);
            return 0;
        }
    }

    int slot = find_free_context_slot(mgr);
    if (slot < 0) {
        LOG_WARN("SECURITY_BOUNDARY", "No free context slots for '%s'", component_id);
        return 0;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[slot];
    memset(ctx, 0, sizeof(ozayn_security_context_t));
    ctx->active = 1;
    strncpy(ctx->component_id, component_id, sizeof(ctx->component_id) - 1);
    ctx->component_id[sizeof(ctx->component_id) - 1] = '\0';
    ctx->trust_level = trust;
    ctx->state = OZAYN_COMP_SEC_NORMAL;
    ctx->created_at = (time_t)mgr->next_context_id++;
    ctx->last_activity = time(NULL);

    mgr->context_count++;

    LOG_INFO("SECURITY_BOUNDARY", "Context registered: '%s' (trust=%s, state=NORMAL)",
             component_id, ozayn_sb_trust_level_name(trust));

    return (uint32_t)ctx->created_at;
}

int ozayn_security_boundary_unregister_context(ozayn_security_boundary_manager_t *mgr,
                                                 uint32_t context_id) {
    if (!mgr || !mgr->initialized) return -1;

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        LOG_WARN("SECURITY_BOUNDARY", "Context %u not found", context_id);
        return -1;
    }

    LOG_INFO("SECURITY_BOUNDARY", "Context unregistered: '%s'", mgr->contexts[idx].component_id);
    mgr->contexts[idx].active = 0;
    mgr->context_count--;
    return 0;
}

ozayn_security_context_t *ozayn_security_boundary_get_context(
    ozayn_security_boundary_manager_t *mgr, uint32_t context_id)
{
    if (!mgr || !mgr->initialized) return NULL;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return NULL;
    return &mgr->contexts[idx];
}

ozayn_security_context_t *ozayn_security_boundary_find_context(
    ozayn_security_boundary_manager_t *mgr, const char *component_id)
{
    if (!mgr || !mgr->initialized || !component_id) return NULL;
    for (int i = 0; i < 32; i++) {
        if (mgr->contexts[i].active &&
            strcmp(mgr->contexts[i].component_id, component_id) == 0) {
            return &mgr->contexts[i];
        }
    }
    return NULL;
}

int ozayn_security_boundary_context_count(const ozayn_security_boundary_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->context_count;
}

/* ================================================================
 * Capability management
 * ================================================================ */

int ozayn_security_boundary_grant_capability(ozayn_security_boundary_manager_t *mgr,
                                              uint32_t context_id,
                                              ozayn_capability_id_t cap) {
    if (!mgr || !mgr->initialized || cap <= OZAYN_CAP_NONE || cap >= OZAYN_CAP_COUNT)
        return -1;

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        LOG_WARN("SECURITY_BOUNDARY", "Context %u not found for capability grant", context_id);
        return -1;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    ctx->capabilities[cap].id = cap;
    ctx->capabilities[cap].granted = 1;
    ctx->capability_count++;

    ctx->last_activity = time(NULL);

    LOG_INFO("SECURITY_BOUNDARY", "Capability granted: '%s' -> %s",
             ctx->component_id, ozayn_capability_name(cap));
    return 0;
}

int ozayn_security_boundary_revoke_capability(ozayn_security_boundary_manager_t *mgr,
                                               uint32_t context_id,
                                               ozayn_capability_id_t cap) {
    if (!mgr || !mgr->initialized || cap <= OZAYN_CAP_NONE || cap >= OZAYN_CAP_COUNT)
        return -1;

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return -1;

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    if (!ctx->capabilities[cap].granted) return -1;

    ctx->capabilities[cap].granted = 0;
    ctx->capability_count--;

    ctx->last_activity = time(NULL);

    LOG_INFO("SECURITY_BOUNDARY", "Capability revoked: '%s' -> %s",
             ctx->component_id, ozayn_capability_name(cap));
    return 0;
}

int ozayn_security_boundary_has_capability(const ozayn_security_boundary_manager_t *mgr,
                                            uint32_t context_id,
                                            ozayn_capability_id_t cap) {
    if (!mgr || !mgr->initialized || cap <= OZAYN_CAP_NONE || cap >= OZAYN_CAP_COUNT)
        return 0;

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return 0;

    return mgr->contexts[idx].capabilities[cap].granted;
}

/* ================================================================
 * Violation reporting
 * ================================================================ */

static ozayn_security_action_t determine_action(
    const ozayn_security_boundary_manager_t *mgr,
    ozayn_security_context_t *ctx,
    ozayn_security_severity_t severity)
{
    if (severity == OZAYN_SEC_SEV_CRITICAL)
        return OZAYN_SEC_ACTION_ISOLATE;

    if (ctx->violation_count >= mgr->policy.max_violations_before_disable)
        return OZAYN_SEC_ACTION_DISABLE;
    if (ctx->violation_count >= mgr->policy.max_violations_before_isolate)
        return OZAYN_SEC_ACTION_ISOLATE;
    if (ctx->violation_count >= mgr->policy.max_violations_before_restrict)
        return OZAYN_SEC_ACTION_RESTRICT;

    if (severity >= OZAYN_SEC_SEV_HIGH) return OZAYN_SEC_ACTION_WARN;
    if (severity >= OZAYN_SEC_SEV_MEDIUM) return OZAYN_SEC_ACTION_LOG;

    return OZAYN_SEC_ACTION_LOG;
}

uint32_t ozayn_security_boundary_report_violation(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    ozayn_violation_type_t type,
    ozayn_security_severity_t severity,
    const char *description)
{
    if (!mgr || !mgr->initialized) return 0;

    int slot = find_free_violation_slot(mgr);
    if (slot < 0) {
        LOG_WARN("SECURITY_BOUNDARY", "Violation log full — dropping violation");
        return 0;
    }

    ozayn_violation_record_t *rec = &mgr->violations[slot];
    memset(rec, 0, sizeof(ozayn_violation_record_t));
    rec->active = 1;
    rec->id = mgr->next_violation_id++;
    rec->type = type;
    rec->severity = severity;
    rec->timestamp = time(NULL);
    rec->action_taken = OZAYN_SEC_ACTION_LOG;

    /* Get component name */
    int ctx_idx = find_context_by_id(mgr, context_id);
    if (ctx_idx >= 0) {
        strncpy(rec->component_id, mgr->contexts[ctx_idx].component_id,
                sizeof(rec->component_id) - 1);
        mgr->contexts[ctx_idx].violation_count++;
        mgr->contexts[ctx_idx].last_activity = rec->timestamp;

        /* Determine action */
        rec->action_taken = determine_action(mgr, &mgr->contexts[ctx_idx], severity);

        /* Apply state transition */
        if (rec->action_taken == OZAYN_SEC_ACTION_RESTRICT &&
            mgr->contexts[ctx_idx].state == OZAYN_COMP_SEC_NORMAL) {
            mgr->contexts[ctx_idx].state = OZAYN_COMP_SEC_RESTRICTED;
        } else if (rec->action_taken == OZAYN_SEC_ACTION_ISOLATE) {
            mgr->contexts[ctx_idx].state = OZAYN_COMP_SEC_ISOLATED;
        } else if (rec->action_taken == OZAYN_SEC_ACTION_DISABLE) {
            mgr->contexts[ctx_idx].state = OZAYN_COMP_SEC_BLOCKED;
        }
    }

    if (description) {
        strncpy(rec->description, description, sizeof(rec->description) - 1);
        rec->description[sizeof(rec->description) - 1] = '\0';
    }

    mgr->violation_count++;
    mgr->total_denied++;

    LOG_WARN("SECURITY_BOUNDARY", "Violation #%u: [%s] %s (%s) -> %s",
             rec->id, rec->component_id,
             ozayn_violation_type_name(type),
             ozayn_security_severity_name(severity),
             ozayn_security_action_name(rec->action_taken));

    return rec->id;
}

int ozayn_security_boundary_violation_count(
    const ozayn_security_boundary_manager_t *mgr)
{
    if (!mgr) return 0;
    return mgr->violation_count;
}

const ozayn_violation_record_t *ozayn_security_boundary_get_violation(
    const ozayn_security_boundary_manager_t *mgr, int index)
{
    if (!mgr) return NULL;
    int count = 0;
    for (int i = 0; i < 128; i++) {
        if (mgr->violations[i].active) {
            if (count == index) return &mgr->violations[i];
            count++;
        }
    }
    return NULL;
}

/* ================================================================
 * Enforcement — capability check
 * ================================================================ */

ozayn_security_check_result_t ozayn_security_boundary_check(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    ozayn_capability_id_t requested_cap)
{
    ozayn_security_check_result_t result;
    memset(&result, 0, sizeof(result));
    result.allowed = 0;
    result.violation = OZAYN_VIOLATION_NONE;
    result.severity = OZAYN_SEC_SEV_LOW;
    result.action = OZAYN_SEC_ACTION_NONE;
    result.reason = "";

    if (!mgr || !mgr->initialized) {
        result.reason = "security boundary not initialized";
        return result;
    }

    if (!mgr->policy.enabled) {
        result.allowed = 1;
        result.reason = "security boundary disabled";
        mgr->total_checks++;
        mgr->total_allowed++;
        return result;
    }

    mgr->total_checks++;

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        result.violation = OZAYN_VIOLATION_UNAUTHORIZED_ACCESS;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "unknown context";
        mgr->total_denied++;
        return result;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    ctx->last_activity = time(NULL);

    /* Check if component is blocked/isolated */
    if (ctx->state == OZAYN_COMP_SEC_BLOCKED) {
        result.violation = OZAYN_VIOLATION_UNAUTHORIZED_ACCESS;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "component is BLOCKED";
        mgr->total_denied++;
        return result;
    }

    if (ctx->state == OZAYN_COMP_SEC_ISOLATED) {
        result.violation = OZAYN_VIOLATION_SANDBOX_BREACH;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "component is ISOLATED";
        mgr->total_denied++;
        return result;
    }

    /* Check capability */
    if (requested_cap > OZAYN_CAP_NONE && requested_cap < OZAYN_CAP_COUNT &&
        ctx->capabilities[requested_cap].granted) {
        result.allowed = 1;
        result.reason = "capability granted";
        mgr->total_allowed++;
        return result;
    }

    /* Denied — determine severity based on what was requested */
    result.violation = OZAYN_VIOLATION_CAPABILITY_DENIED;
    result.action = OZAYN_SEC_ACTION_BLOCK;

    /* Sensitive capabilities get higher severity */
    if (requested_cap == OZAYN_CAP_SECURITY_ADMIN ||
        requested_cap == OZAYN_CAP_CORE_SHUTDOWN ||
        requested_cap == OZAYN_CAP_IDENTITY_MANAGE) {
        result.severity = OZAYN_SEC_SEV_CRITICAL;
    } else if (requested_cap == OZAYN_CAP_POLICY_MODIFY ||
               requested_cap == OZAYN_CAP_CONFIG_WRITE ||
               requested_cap == OZAYN_CAP_PROCESS_STOP) {
        result.severity = OZAYN_SEC_SEV_HIGH;
    } else {
        result.severity = OZAYN_SEC_SEV_MEDIUM;
    }

    result.reason = "capability not granted";

    /* Report violation */
    ozayn_security_boundary_report_violation(mgr, context_id,
                                              OZAYN_VIOLATION_CAPABILITY_DENIED,
                                              result.severity,
                                              ozayn_capability_name(requested_cap));

    return result;
}

/* ================================================================
 * Enforcement — IPC check
 * ================================================================ */

ozayn_security_check_result_t ozayn_security_boundary_check_ipc(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    const char *target_component)
{
    ozayn_security_check_result_t result;
    memset(&result, 0, sizeof(result));
    result.allowed = 0;
    result.reason = "";

    if (!mgr || !mgr->initialized) {
        result.reason = "security boundary not initialized";
        return result;
    }

    if (!mgr->policy.enabled) {
        result.allowed = 1;
        result.reason = "security boundary disabled";
        return result;
    }

    /* UNTRUSTED components cannot send IPC to CORE components */
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        result.violation = OZAYN_VIOLATION_UNAUTHORIZED_ACCESS;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "unknown context";
        return result;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[idx];

    if (ctx->state == OZAYN_COMP_SEC_ISOLATED || ctx->state == OZAYN_COMP_SEC_BLOCKED) {
        result.violation = OZAYN_VIOLATION_IPC_BREACH;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "component is not allowed IPC";
        return result;
    }

    /* UNTRUSTED cannot IPC to anything */
    if (ctx->trust_level == OZAYN_SB_TRUST_UNTRUSTED) {
        result.violation = OZAYN_VIOLATION_IPC_BREACH;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "untrusted component IPC denied";
        ozayn_security_boundary_report_violation(mgr, context_id,
                                                  OZAYN_VIOLATION_IPC_BREACH,
                                                  result.severity,
                                                  "untrusted IPC attempt");
        return result;
    }

    /* Check IPC capability */
    if (!ctx->capabilities[OZAYN_CAP_IPC_SEND].granted) {
        result.violation = OZAYN_VIOLATION_CAPABILITY_DENIED;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "ipc.send capability not granted";
        ozayn_security_boundary_report_violation(mgr, context_id,
                                                  OZAYN_VIOLATION_CAPABILITY_DENIED,
                                                  result.severity,
                                                  "IPC without capability");
        return result;
    }

    /* TARGET_PROTECT: untrusted/limited cannot IPC to security/authorization */
    if (target_component) {
        if ((strcmp(target_component, "SECURITY") == 0 ||
             strcmp(target_component, "AUTHORIZATION") == 0) &&
            ctx->trust_level >= OZAYN_SB_TRUST_LIMITED) {
            result.violation = OZAYN_VIOLATION_PRIVILEGE_ESCALATION;
            result.severity = OZAYN_SEC_SEV_CRITICAL;
            result.action = OZAYN_SEC_ACTION_BLOCK;
            result.reason = "limited component cannot IPC to security";
            ozayn_security_boundary_report_violation(mgr, context_id,
                                                      OZAYN_VIOLATION_PRIVILEGE_ESCALATION,
                                                      result.severity,
                                                      "IPC to protected component");
            return result;
        }
    }

    result.allowed = 1;
    result.reason = "IPC allowed";
    return result;
}

/* ================================================================
 * Enforcement — resource check
 * ================================================================ */

ozayn_security_check_result_t ozayn_security_boundary_check_resource(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id,
    const char *resource_id)
{
    ozayn_security_check_result_t result;
    memset(&result, 0, sizeof(result));
    result.allowed = 0;
    result.reason = "";

    (void)resource_id;

    if (!mgr || !mgr->initialized) {
        result.reason = "security boundary not initialized";
        return result;
    }

    if (!mgr->policy.enabled) {
        result.allowed = 1;
        return result;
    }

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        result.violation = OZAYN_VIOLATION_UNAUTHORIZED_ACCESS;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "unknown context";
        return result;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[idx];

    if (ctx->state == OZAYN_COMP_SEC_ISOLATED || ctx->state == OZAYN_COMP_SEC_BLOCKED) {
        result.violation = OZAYN_VIOLATION_RESOURCE_ABUSE;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "component restricted from resources";
        return result;
    }

    /* Check resource allocation capability */
    if (!ctx->capabilities[OZAYN_CAP_RESOURCE_ALLOCATE].granted) {
        result.violation = OZAYN_VIOLATION_CAPABILITY_DENIED;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "resource.allocate capability not granted";
        ozayn_security_boundary_report_violation(mgr, context_id,
                                                  OZAYN_VIOLATION_CAPABILITY_DENIED,
                                                  result.severity,
                                                  "resource access without capability");
        return result;
    }

    result.allowed = 1;
    result.reason = "resource access allowed";
    return result;
}

/* ================================================================
 * Enforcement — task check
 * ================================================================ */

ozayn_security_check_result_t ozayn_security_boundary_check_task(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t context_id)
{
    ozayn_security_check_result_t result;
    memset(&result, 0, sizeof(result));
    result.allowed = 0;
    result.reason = "";

    if (!mgr || !mgr->initialized) {
        result.reason = "security boundary not initialized";
        return result;
    }

    if (!mgr->policy.enabled) {
        result.allowed = 1;
        return result;
    }

    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) {
        result.violation = OZAYN_VIOLATION_UNAUTHORIZED_ACCESS;
        result.severity = OZAYN_SEC_SEV_HIGH;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "unknown context";
        return result;
    }

    ozayn_security_context_t *ctx = &mgr->contexts[idx];

    if (ctx->state == OZAYN_COMP_SEC_BLOCKED) {
        result.violation = OZAYN_VIOLATION_TASK_ABUSE;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "component BLOCKED — cannot create tasks";
        return result;
    }

    if (!ctx->capabilities[OZAYN_CAP_TASK_CREATE].granted) {
        result.violation = OZAYN_VIOLATION_CAPABILITY_DENIED;
        result.severity = OZAYN_SEC_SEV_MEDIUM;
        result.action = OZAYN_SEC_ACTION_BLOCK;
        result.reason = "task.create capability not granted";
        ozayn_security_boundary_report_violation(mgr, context_id,
                                                  OZAYN_VIOLATION_CAPABILITY_DENIED,
                                                  result.severity,
                                                  "task creation without capability");
        return result;
    }

    /* Check task quota */
    if (ctx->limits.max_tasks > 0) {
        /* In real implementation, would check active task count against limit */
        /* For now, just allow */
    }

    result.allowed = 1;
    result.reason = "task creation allowed";
    return result;
}

/* ================================================================
 * State transitions
 * ================================================================ */

int ozayn_security_boundary_restrict(ozayn_security_boundary_manager_t *mgr,
                                      uint32_t context_id) {
    if (!mgr || !mgr->initialized) return -1;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return -1;

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    if (ctx->state != OZAYN_COMP_SEC_NORMAL) return -1;

    ctx->state = OZAYN_COMP_SEC_RESTRICTED;
    LOG_WARN("SECURITY_BOUNDARY", "Component '%s' -> RESTRICTED", ctx->component_id);
    return 0;
}

int ozayn_security_boundary_isolate(ozayn_security_boundary_manager_t *mgr,
                                     uint32_t context_id) {
    if (!mgr || !mgr->initialized) return -1;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return -1;

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    ctx->state = OZAYN_COMP_SEC_ISOLATED;
    LOG_WARN("SECURITY_BOUNDARY", "Component '%s' -> ISOLATED", ctx->component_id);
    return 0;
}

int ozayn_security_boundary_restore(ozayn_security_boundary_manager_t *mgr,
                                     uint32_t context_id) {
    if (!mgr || !mgr->initialized) return -1;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return -1;

    ozayn_security_context_t *ctx = &mgr->contexts[idx];
    ctx->state = OZAYN_COMP_SEC_NORMAL;
    LOG_INFO("SECURITY_BOUNDARY", "Component '%s' -> NORMAL (restored)", ctx->component_id);
    return 0;
}

/* ================================================================
 * Resource limits
 * ================================================================ */

void ozayn_security_boundary_set_limits(ozayn_security_boundary_manager_t *mgr,
                                         uint32_t context_id,
                                         const ozayn_resource_limits_t *limits) {
    if (!mgr || !mgr->initialized || !limits) return;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return;

    mgr->contexts[idx].limits = *limits;
    LOG_INFO("SECURITY_BOUNDARY", "Limits set for '%s' (tasks=%d, resources=%d, memory=%lld)",
             mgr->contexts[idx].component_id,
             limits->max_tasks, limits->max_resources,
             (long long)limits->max_memory_bytes);
}

const ozayn_resource_limits_t *ozayn_security_boundary_get_limits(
    const ozayn_security_boundary_manager_t *mgr, uint32_t context_id)
{
    if (!mgr || !mgr->initialized) return NULL;
    int idx = find_context_by_id(mgr, context_id);
    if (idx < 0) return NULL;
    return &mgr->contexts[idx].limits;
}

/* ================================================================
 * Security context inheritance
 * ================================================================ */

uint32_t ozayn_security_boundary_inherit_context(
    ozayn_security_boundary_manager_t *mgr,
    uint32_t parent_context_id)
{
    if (!mgr || !mgr->initialized) return 0;

    int pidx = find_context_by_id(mgr, parent_context_id);
    if (pidx < 0) return 0;

    ozayn_security_context_t *parent = &mgr->contexts[pidx];

    /* Inherit trust level (cannot escalate) */
    ozayn_sb_trust_level_t inherited_trust = parent->trust_level;

    /* Create a child context with inherited trust */
    char child_id[64];
    int written = snprintf(child_id, sizeof(child_id), "%s.task.",
                           parent->component_id);
    if (written > 0 && (size_t)written < sizeof(child_id)) {
        snprintf(child_id + written, sizeof(child_id) - (size_t)written,
                 "%u", mgr->next_context_id);
    }

    uint32_t child_ctx_id = ozayn_security_boundary_register_context(
        mgr, child_id, inherited_trust);

    if (child_ctx_id == 0) return 0;

    /* Grant child same capabilities as parent (inherit, not escalate) */
    int cidx = find_context_by_id(mgr, child_ctx_id);
    if (cidx >= 0) {
        ozayn_security_context_t *child = &mgr->contexts[cidx];
        for (int c = 1; c < OZAYN_CAP_COUNT; c++) {
            child->capabilities[c] = parent->capabilities[c];
            if (parent->capabilities[c].granted) child->capability_count++;
        }
    }

    LOG_INFO("SECURITY_BOUNDARY", "Context inherited: '%s' from '%s' (trust=%s)",
             child_id, parent->component_id,
             ozayn_sb_trust_level_name(inherited_trust));

    return child_ctx_id;
}

/* ================================================================
 * Statistics
 * ================================================================ */

ozayn_security_boundary_stats_t ozayn_security_boundary_stats(
    const ozayn_security_boundary_manager_t *mgr)
{
    ozayn_security_boundary_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    if (!mgr) return stats;

    stats.total_checks = mgr->total_checks;
    stats.total_allowed = mgr->total_allowed;
    stats.total_denied = mgr->total_denied;
    stats.total_violations = mgr->violation_count;
    stats.components_registered = mgr->context_count;

    for (int i = 0; i < 32; i++) {
        if (mgr->contexts[i].active) {
            if (mgr->contexts[i].state == OZAYN_COMP_SEC_ISOLATED)
                stats.isolated_components++;
            if (mgr->contexts[i].state == OZAYN_COMP_SEC_RESTRICTED)
                stats.restricted_components++;
        }
    }

    return stats;
}

/* ================================================================
 * Print helpers
 * ================================================================ */

void ozayn_security_boundary_print_contexts(
    const ozayn_security_boundary_manager_t *mgr)
{
    if (!mgr) return;

    LOG_INFO("SECURITY_BOUNDARY", "--- Security Contexts (%d) ---", mgr->context_count);

    for (int i = 0; i < 32; i++) {
        const ozayn_security_context_t *ctx = &mgr->contexts[i];
        if (ctx->active) {
            LOG_INFO("SECURITY_BOUNDARY", "  '%s' trust=%s state=%s caps=%d violations=%d",
                     ctx->component_id,
                     ozayn_sb_trust_level_name(ctx->trust_level),
                     ozayn_component_sec_state_name(ctx->state),
                     ctx->capability_count,
                     ctx->violation_count);
        }
    }
}

void ozayn_security_boundary_print_violations(
    const ozayn_security_boundary_manager_t *mgr)
{
    if (!mgr) return;

    LOG_INFO("SECURITY_BOUNDARY", "--- Violations (%d) ---", mgr->violation_count);

    for (int i = 0; i < 128; i++) {
        const ozayn_violation_record_t *v = &mgr->violations[i];
        if (v->active) {
            LOG_INFO("SECURITY_BOUNDARY", "  #%u [%s] %s %s -> %s",
                     v->id, v->component_id,
                     ozayn_violation_type_name(v->type),
                     ozayn_security_severity_name(v->severity),
                     ozayn_security_action_name(v->action_taken));
        }
    }
}

void ozayn_security_boundary_print_stats(
    const ozayn_security_boundary_manager_t *mgr)
{
    ozayn_security_boundary_stats_t s = ozayn_security_boundary_stats(mgr);

    LOG_INFO("SECURITY_BOUNDARY", "--- Security Statistics ---");
    LOG_INFO("SECURITY_BOUNDARY", "  Checks: %d (allowed=%d, denied=%d)",
             s.total_checks, s.total_allowed, s.total_denied);
    LOG_INFO("SECURITY_BOUNDARY", "  Violations: %d", s.total_violations);
    LOG_INFO("SECURITY_BOUNDARY", "  Contexts: %d (restricted=%d, isolated=%d)",
             s.components_registered, s.restricted_components, s.isolated_components);
}
