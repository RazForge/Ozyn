#include "ozayn.h"
#include "diagnostics.h"
#include "logger.h"
#include "recovery.h"
#include "events.h"
#include "monitoring.h"
#include <stdio.h>
#include <string.h>

/*
 * Diagnostics & Debugging Engine implementation.
 *
 * Collects evidence, correlates events, reconstructs timelines,
 * generates findings, captures snapshots, tracks failure patterns.
 *
 * Bounded memory: static arrays, no dynamic allocation.
 * Deterministic: rules-based, no AI inference.
 */

#define REPEATED_FAILURE_THRESHOLD  3
#define FAILURE_WINDOW_SECONDS    300

/* ---- Names ---- */

const char *ozayn_diag_level_name(ozayn_diag_level_t level) {
    switch (level) {
        case OZAYN_DIAG_LEVEL_NORMAL:   return "NORMAL";
        case OZAYN_DIAG_LEVEL_DETAILED: return "DETAILED";
        case OZAYN_DIAG_LEVEL_DEBUG:    return "DEBUG";
        case OZAYN_DIAG_LEVEL_TRACE:    return "TRACE";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_session_state_name(ozayn_diag_session_state_t state) {
    switch (state) {
        case OZAYN_DIAG_SESSION_CREATED:    return "CREATED";
        case OZAYN_DIAG_SESSION_COLLECTING: return "COLLECTING";
        case OZAYN_DIAG_SESSION_ANALYZING:  return "ANALYZING";
        case OZAYN_DIAG_SESSION_FINDINGS:   return "FINDINGS";
        case OZAYN_DIAG_SESSION_COMPLETED:  return "COMPLETED";
        case OZAYN_DIAG_SESSION_FAILED:     return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_target_name(ozayn_diag_target_t target) {
    switch (target) {
        case OZAYN_DIAG_TARGET_CORE:      return "CORE";
        case OZAYN_DIAG_TARGET_RUNTIME:   return "RUNTIME";
        case OZAYN_DIAG_TARGET_TASK:      return "TASK";
        case OZAYN_DIAG_TARGET_PROCESS:   return "PROCESS";
        case OZAYN_DIAG_TARGET_MODULE:    return "MODULE";
        case OZAYN_DIAG_TARGET_PLUGIN:    return "PLUGIN";
        case OZAYN_DIAG_TARGET_IPC:       return "IPC";
        case OZAYN_DIAG_TARGET_SCHEDULER: return "SCHEDULER";
        case OZAYN_DIAG_TARGET_RESOURCE:  return "RESOURCE";
        case OZAYN_DIAG_TARGET_EVENT:     return "EVENT";
        case OZAYN_DIAG_TARGET_COMPONENT: return "COMPONENT";
        case OZAYN_DIAG_TARGET_SYSTEM:    return "SYSTEM";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_confidence_name(ozayn_diag_confidence_t conf) {
    switch (conf) {
        case OZAYN_DIAG_CONFIDENCE_NONE:    return "NONE";
        case OZAYN_DIAG_CONFIDENCE_LOW:     return "LOW";
        case OZAYN_DIAG_CONFIDENCE_MEDIUM:  return "MEDIUM";
        case OZAYN_DIAG_CONFIDENCE_HIGH:    return "HIGH";
        case OZAYN_DIAG_CONFIDENCE_CERTAIN: return "CERTAIN";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_finding_sev_name(ozayn_diag_finding_sev_t sev) {
    switch (sev) {
        case OZAYN_DIAG_SEV_INFO:     return "INFO";
        case OZAYN_DIAG_SEV_WARNING:  return "WARNING";
        case OZAYN_DIAG_SEV_ERROR:    return "ERROR";
        case OZAYN_DIAG_SEV_CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_component_name(ozayn_diag_component_t comp) {
    switch (comp) {
        case OZAYN_DIAG_COMP_CORE:            return "CORE";
        case OZAYN_DIAG_COMP_RUNTIME:         return "RUNTIME";
        case OZAYN_DIAG_COMP_EVENT_ENGINE:    return "EVENT_ENGINE";
        case OZAYN_DIAG_COMP_COMMAND_ENGINE:  return "COMMAND_ENGINE";
        case OZAYN_DIAG_COMP_TASK_MANAGER:    return "TASK_MANAGER";
        case OZAYN_DIAG_COMP_PROCESS_MANAGER: return "PROCESS_MANAGER";
        case OZAYN_DIAG_COMP_MODULE_MANAGER:  return "MODULE_MANAGER";
        case OZAYN_DIAG_COMP_PLUGIN_MANAGER:  return "PLUGIN_MANAGER";
        case OZAYN_DIAG_COMP_IPC:             return "IPC";
        case OZAYN_DIAG_COMP_CONFIGURATION:   return "CONFIGURATION";
        case OZAYN_DIAG_COMP_LOGGER:          return "LOGGER";
        case OZAYN_DIAG_COMP_ERROR_RECOVERY:  return "ERROR_RECOVERY";
        case OZAYN_DIAG_COMP_SECURITY:        return "SECURITY";
        case OZAYN_DIAG_COMP_AUTHORIZATION:   return "AUTHORIZATION";
        case OZAYN_DIAG_COMP_RESOURCE_MANAGER: return "RESOURCE_MANAGER";
        case OZAYN_DIAG_COMP_SCHEDULER:       return "SCHEDULER";
    }
    return "UNKNOWN";
}

const char *ozayn_diag_health_name(ozayn_diag_health_t health) {
    switch (health) {
        case OZAYN_DIAG_HEALTH_UNKNOWN:  return "UNKNOWN";
        case OZAYN_DIAG_HEALTH_HEALTHY:  return "HEALTHY";
        case OZAYN_DIAG_HEALTH_DEGRADED: return "DEGRADED";
        case OZAYN_DIAG_HEALTH_UNHEALTHY: return "UNHEALTHY";
        case OZAYN_DIAG_HEALTH_FAILED:   return "FAILED";
    }
    return "UNKNOWN";
}

/* ---- Lifecycle ---- */

ozayn_result_t ozayn_diagnostics_init(ozayn_diagnostics_manager_t *mgr, int enabled) {
    if (!mgr) return OZAYN_ERR_NULL;
    memset(mgr, 0, sizeof(*mgr));
    mgr->enabled = enabled;
    mgr->initialized = 1;
    mgr->level = OZAYN_DIAG_LEVEL_NORMAL;
    mgr->next_evidence_id = 1;
    mgr->next_finding_id = 1;
    mgr->next_timeline_seq = 1;
    mgr->next_session_id = 1;
    mgr->next_snapshot_id = 1;
    LOG_INFO("DIAGNOSTICS", "Diagnostics engine initialized (enabled=%s, level=%s)",
             enabled ? "yes" : "no", ozayn_diag_level_name(mgr->level));
    return OZAYN_OK;
}

void ozayn_diagnostics_shutdown(ozayn_diagnostics_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;
    LOG_INFO("DIAGNOSTICS", "Diagnostics shut down (evidence=%d, findings=%d, sessions=%d)",
             mgr->stats.evidence_recorded, mgr->stats.findings_generated,
             mgr->stats.sessions_completed);
    mgr->initialized = 0;
}

/* ---- Bindings ---- */

void ozayn_diagnostics_set_events(ozayn_diagnostics_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_diagnostics_set_recovery(ozayn_diagnostics_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

void ozayn_diagnostics_set_monitoring(ozayn_diagnostics_manager_t *mgr, void *monitoring) {
    if (mgr) mgr->monitoring = monitoring;
}

/* ---- Level control ---- */

void ozayn_diagnostics_set_level(ozayn_diagnostics_manager_t *mgr, ozayn_diag_level_t level) {
    if (!mgr) return;
    mgr->level = level;
    LOG_INFO("DIAGNOSTICS", "Diagnostic level set to %s", ozayn_diag_level_name(level));
}

ozayn_diag_level_t ozayn_diagnostics_get_level(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return OZAYN_DIAG_LEVEL_NORMAL;
    return mgr->level;
}

/* ---- Evidence ---- */

uint32_t ozayn_diagnostics_record_evidence(ozayn_diagnostics_manager_t *mgr,
                                            ozayn_diag_component_t component,
                                            ozayn_diag_target_t target,
                                            const char *correlation_id,
                                            const char *description) {
    if (!mgr || !mgr->initialized) return 0;
    if (mgr->evidence_count >= OZAYN_DIAG_MAX_EVIDENCE) {
        LOG_WARN("DIAGNOSTICS", "Evidence buffer full — dropping");
        return 0;
    }

    int slot = -1;
    for (int i = 0; i < OZAYN_DIAG_MAX_EVIDENCE; i++) {
        if (!mgr->evidence[i].active) { slot = i; break; }
    }
    if (slot < 0) return 0;

    ozayn_evidence_t *ev = &mgr->evidence[slot];
    ev->active = 1;
    ev->id = mgr->next_evidence_id++;
    ev->component = component;
    ev->target = target;
    ev->timestamp = time(NULL);
    ev->is_redacted = 0;

    if (correlation_id)
        snprintf(ev->correlation_id, sizeof(ev->correlation_id), "%s", correlation_id);
    else
        ev->correlation_id[0] = '\0';

    if (description)
        snprintf(ev->description, sizeof(ev->description), "%s", description);
    else
        ev->description[0] = '\0';

    mgr->evidence_count++;
    mgr->stats.evidence_recorded++;

    if (mgr->level >= OZAYN_DIAG_LEVEL_DETAILED) {
        LOG_INFO("DIAGNOSTICS", "Evidence #%u recorded: [%s] %s (corr=%s)",
                 ev->id, ozayn_diag_component_name(component), description,
                 correlation_id ? correlation_id : "none");
    }

    return ev->id;
}

const ozayn_evidence_t *ozayn_diagnostics_get_evidence(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id) {
    if (!mgr) return NULL;
    for (int i = 0; i < OZAYN_DIAG_MAX_EVIDENCE; i++) {
        if (mgr->evidence[i].active && mgr->evidence[i].id == id)
            return &mgr->evidence[i];
    }
    return NULL;
}

int ozayn_diagnostics_evidence_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->evidence_count;
}

/* ---- Findings ---- */

uint32_t ozayn_diagnostics_add_finding(ozayn_diagnostics_manager_t *mgr,
                                        ozayn_diag_component_t component,
                                        ozayn_diag_finding_sev_t severity,
                                        const char *correlation_id,
                                        const char *observation,
                                        const char *possible_cause,
                                        ozayn_diag_confidence_t confidence) {
    if (!mgr || !mgr->initialized) return 0;
    if (mgr->finding_count >= OZAYN_DIAG_MAX_FINDINGS) {
        LOG_WARN("DIAGNOSTICS", "Findings buffer full — dropping");
        return 0;
    }

    int slot = -1;
    for (int i = 0; i < OZAYN_DIAG_MAX_FINDINGS; i++) {
        if (!mgr->findings[i].active) { slot = i; break; }
    }
    if (slot < 0) return 0;

    ozayn_diagnostic_finding_t *f = &mgr->findings[slot];
    f->active = 1;
    f->id = mgr->next_finding_id++;
    f->component = component;
    f->severity = severity;
    f->confidence = confidence;
    f->timestamp = time(NULL);

    if (correlation_id)
        snprintf(f->correlation_id, sizeof(f->correlation_id), "%s", correlation_id);
    else
        f->correlation_id[0] = '\0';

    if (observation)
        snprintf(f->observation, sizeof(f->observation), "%s", observation);
    else
        f->observation[0] = '\0';

    if (possible_cause)
        snprintf(f->possible_cause, sizeof(f->possible_cause), "%s", possible_cause);
    else
        f->possible_cause[0] = '\0';

    mgr->finding_count++;
    mgr->stats.findings_generated++;

    LOG_INFO("DIAGNOSTICS", "Finding #%u: [%s] %s (cause=%s, conf=%s)",
             f->id, ozayn_diag_component_name(component), observation,
             possible_cause, ozayn_diag_confidence_name(confidence));

    return f->id;
}

const ozayn_diagnostic_finding_t *ozayn_diagnostics_get_finding(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id) {
    if (!mgr) return NULL;
    for (int i = 0; i < OZAYN_DIAG_MAX_FINDINGS; i++) {
        if (mgr->findings[i].active && mgr->findings[i].id == id)
            return &mgr->findings[i];
    }
    return NULL;
}

int ozayn_diagnostics_finding_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->finding_count;
}

/* ---- Timeline ---- */

uint32_t ozayn_diagnostics_timeline_add(ozayn_diagnostics_manager_t *mgr,
                                         ozayn_diag_component_t component,
                                         const char *correlation_id,
                                         const char *description) {
    if (!mgr || !mgr->initialized) return 0;

    int slot = mgr->timeline_count < OZAYN_DIAG_MAX_TIMELINE
        ? mgr->timeline_count
        : (int)(mgr->next_timeline_seq % OZAYN_DIAG_MAX_TIMELINE);

    ozayn_timeline_entry_t *te = &mgr->timeline[slot];
    te->active = 1;
    te->seq = mgr->next_timeline_seq++;
    te->component = component;
    te->timestamp = time(NULL);

    if (correlation_id)
        snprintf(te->correlation_id, sizeof(te->correlation_id), "%s", correlation_id);
    else
        te->correlation_id[0] = '\0';

    if (description)
        snprintf(te->description, sizeof(te->description), "%s", description);
    else
        te->description[0] = '\0';

    if (mgr->timeline_count < OZAYN_DIAG_MAX_TIMELINE)
        mgr->timeline_count++;

    mgr->stats.timeline_entries++;

    if (mgr->level >= OZAYN_DIAG_LEVEL_DEBUG) {
        LOG_INFO("DIAGNOSTICS", "Timeline #%u: [%s] %s",
                 te->seq, ozayn_diag_component_name(component), description);
    }

    return te->seq;
}

const ozayn_timeline_entry_t *ozayn_diagnostics_timeline_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t seq) {
    if (!mgr) return NULL;
    for (int i = 0; i < mgr->timeline_count; i++) {
        if (mgr->timeline[i].active && mgr->timeline[i].seq == seq)
            return &mgr->timeline[i];
    }
    return NULL;
}

int ozayn_diagnostics_timeline_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->timeline_count;
}

void ozayn_diagnostics_timeline_print(const ozayn_diagnostics_manager_t *mgr,
                                       const char *correlation_id) {
    if (!mgr) return;

    LOG_INFO("DIAGNOSTICS", "--- Timeline (entries=%d) ---", mgr->timeline_count);

    for (int i = 0; i < mgr->timeline_count; i++) {
        const ozayn_timeline_entry_t *te = &mgr->timeline[i];
        if (!te->active) continue;
        if (correlation_id && te->correlation_id[0] &&
            strcmp(te->correlation_id, correlation_id) != 0)
            continue;

        struct tm tm_info;
        localtime_r(&te->timestamp, &tm_info);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_info);

        LOG_INFO("DIAGNOSTICS", "  %s [%s] %s",
                 timebuf,
                 ozayn_diag_component_name(te->component),
                 te->description);
    }
}

/* ---- Sessions ---- */

uint32_t ozayn_diagnostics_session_start(ozayn_diagnostics_manager_t *mgr,
                                          ozayn_diag_target_t target,
                                          const char *correlation_id) {
    if (!mgr || !mgr->initialized) return 0;
    if (mgr->session_count >= OZAYN_DIAG_MAX_SESSIONS) {
        LOG_WARN("DIAGNOSTICS", "Session buffer full — cannot start new session");
        return 0;
    }

    int slot = -1;
    for (int i = 0; i < OZAYN_DIAG_MAX_SESSIONS; i++) {
        if (!mgr->sessions[i].active) { slot = i; break; }
    }
    if (slot < 0) return 0;

    ozayn_diag_session_t *s = &mgr->sessions[slot];
    memset(s, 0, sizeof(*s));
    s->active = 1;
    s->id = mgr->next_session_id++;
    s->target = target;
    s->state = OZAYN_DIAG_SESSION_CREATED;
    s->started_at = time(NULL);

    if (correlation_id)
        snprintf(s->correlation_id, sizeof(s->correlation_id), "%s", correlation_id);
    else
        s->correlation_id[0] = '\0';

    mgr->session_count++;
    mgr->stats.sessions_created++;

    LOG_INFO("DIAGNOSTICS", "Session #%u started (target=%s, corr=%s)",
             s->id, ozayn_diag_target_name(target),
             correlation_id ? correlation_id : "none");

    return s->id;
}

ozayn_result_t ozayn_diagnostics_session_set_state(
    ozayn_diagnostics_manager_t *mgr,
    uint32_t session_id,
    ozayn_diag_session_state_t state) {
    if (!mgr) return OZAYN_ERR_NULL;

    for (int i = 0; i < OZAYN_DIAG_MAX_SESSIONS; i++) {
        ozayn_diag_session_t *s = &mgr->sessions[i];
        if (s->active && s->id == session_id) {
            s->state = state;
            if (state == OZAYN_DIAG_SESSION_COMPLETED ||
                state == OZAYN_DIAG_SESSION_FAILED) {
                s->completed_at = time(NULL);
                if (state == OZAYN_DIAG_SESSION_COMPLETED)
                    mgr->stats.sessions_completed++;
            }
            LOG_INFO("DIAGNOSTICS", "Session #%u state -> %s",
                     session_id, ozayn_diag_session_state_name(state));
            return OZAYN_OK;
        }
    }
    return OZAYN_ERR;
}

const ozayn_diag_session_t *ozayn_diagnostics_session_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t session_id) {
    if (!mgr) return NULL;
    for (int i = 0; i < OZAYN_DIAG_MAX_SESSIONS; i++) {
        if (mgr->sessions[i].active && mgr->sessions[i].id == session_id)
            return &mgr->sessions[i];
    }
    return NULL;
}

int ozayn_diagnostics_session_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->session_count;
}

int ozayn_diagnostics_active_session_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_DIAG_MAX_SESSIONS; i++) {
        if (mgr->sessions[i].active &&
            mgr->sessions[i].state != OZAYN_DIAG_SESSION_COMPLETED &&
            mgr->sessions[i].state != OZAYN_DIAG_SESSION_FAILED)
            count++;
    }
    return count;
}

/* ---- Snapshots ---- */

uint32_t ozayn_diagnostics_snapshot_capture(ozayn_diagnostics_manager_t *mgr, void *mon) {
    if (!mgr || !mgr->initialized) return 0;

    int slot = -1;
    if (mgr->snapshot_count >= OZAYN_DIAG_MAX_SNAPSHOTS) {
        int oldest = 0;
        for (int i = 1; i < OZAYN_DIAG_MAX_SNAPSHOTS; i++) {
            if (mgr->snapshots[i].timestamp < mgr->snapshots[oldest].timestamp)
                oldest = i;
        }
        slot = oldest;
    } else {
        for (int i = 0; i < OZAYN_DIAG_MAX_SNAPSHOTS; i++) {
            if (!mgr->snapshots[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) return 0;

    ozayn_diag_snapshot_t *snap = &mgr->snapshots[slot];
    memset(snap, 0, sizeof(*snap));
    snap->active = 1;
    snap->id = mgr->next_snapshot_id++;
    snap->timestamp = time(NULL);

    if (mon) {
        /* Cast monitoring manager to read health state */
        const ozayn_monitoring_manager_t *monitoring =
            (const ozayn_monitoring_manager_t *)mon;
        snap->overall_health = (ozayn_diag_health_t)ozayn_monitoring_overall_health(monitoring);
        snap->open_incidents = ozayn_monitoring_open_incident_count(monitoring);
        for (int c = 0; c <= OZAYN_DIAG_COMP_SCHEDULER; c++) {
            snap->component_health[c] = (ozayn_diag_health_t)
                ozayn_monitoring_get_health(monitoring, (ozayn_component_id_t)c);
        }
    }

    if (mgr->snapshot_count < OZAYN_DIAG_MAX_SNAPSHOTS)
        mgr->snapshot_count++;

    mgr->stats.snapshots_captured++;

    LOG_INFO("DIAGNOSTICS", "Snapshot #%u captured (overall=%s)",
             snap->id,
             ozayn_diag_health_name(snap->overall_health));

    return snap->id;
}

const ozayn_diag_snapshot_t *ozayn_diagnostics_snapshot_get(
    const ozayn_diagnostics_manager_t *mgr, uint32_t id) {
    if (!mgr) return NULL;
    for (int i = 0; i < OZAYN_DIAG_MAX_SNAPSHOTS; i++) {
        if (mgr->snapshots[i].active && mgr->snapshots[i].id == id)
            return &mgr->snapshots[i];
    }
    return NULL;
}

int ozayn_diagnostics_snapshot_count(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->snapshot_count;
}

/* ---- Failure tracking ---- */

ozayn_result_t ozayn_diagnostics_record_failure(ozayn_diagnostics_manager_t *mgr,
                                                  ozayn_diag_component_t component) {
    if (!mgr || !mgr->initialized) return OZAYN_ERR_NULL;

    time_t now = time(NULL);

    for (int i = 0; i < OZAYN_DIAG_MAX_FAILURES; i++) {
        if (mgr->failures[i].active && mgr->failures[i].component == component) {
            mgr->failures[i].total_failures++;
            if (now - mgr->failures[i].window_start > FAILURE_WINDOW_SECONDS) {
                mgr->failures[i].recent_failures = 1;
                mgr->failures[i].window_start = now;
            } else {
                mgr->failures[i].recent_failures++;
            }
            mgr->failures[i].last_failure_at = now;
            return OZAYN_OK;
        }
    }

    int slot = -1;
    for (int i = 0; i < OZAYN_DIAG_MAX_FAILURES; i++) {
        if (!mgr->failures[i].active) { slot = i; break; }
    }
    if (slot < 0) return OZAYN_ERR;

    mgr->failures[slot].active = 1;
    mgr->failures[slot].component = component;
    mgr->failures[slot].total_failures = 1;
    mgr->failures[slot].recent_failures = 1;
    mgr->failures[slot].last_failure_at = now;
    mgr->failures[slot].window_start = now;
    mgr->failure_count++;

    return OZAYN_OK;
}

int ozayn_diagnostics_failure_count(const ozayn_diagnostics_manager_t *mgr,
                                     ozayn_diag_component_t component) {
    if (!mgr) return 0;
    for (int i = 0; i < OZAYN_DIAG_MAX_FAILURES; i++) {
        if (mgr->failures[i].active && mgr->failures[i].component == component)
            return mgr->failures[i].total_failures;
    }
    return 0;
}

int ozayn_diagnostics_is_repeated_failure(const ozayn_diagnostics_manager_t *mgr,
                                           ozayn_diag_component_t component,
                                           int threshold) {
    if (!mgr) return 0;
    for (int i = 0; i < OZAYN_DIAG_MAX_FAILURES; i++) {
        if (mgr->failures[i].active && mgr->failures[i].component == component)
            return mgr->failures[i].recent_failures >= threshold;
    }
    return 0;
}

void ozayn_diagnostics_print_failure_summary(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return;

    LOG_INFO("DIAGNOSTICS", "--- Failure Summary ---");
    int found = 0;
    for (int i = 0; i < OZAYN_DIAG_MAX_FAILURES; i++) {
        if (mgr->failures[i].active) {
            found = 1;
            const char *repeated = mgr->failures[i].recent_failures >= REPEATED_FAILURE_THRESHOLD
                ? " [REPEATED]" : "";
            LOG_INFO("DIAGNOSTICS", "  %-20s total=%d recent=%d%s",
                     ozayn_diag_component_name(mgr->failures[i].component),
                     mgr->failures[i].total_failures,
                     mgr->failures[i].recent_failures,
                     repeated);
        }
    }
    if (!found)
        LOG_INFO("DIAGNOSTICS", "  No failures recorded");
}

/* ---- Query ---- */

ozayn_diag_stats_t ozayn_diagnostics_stats(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) {
        ozayn_diag_stats_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return mgr->stats;
}

int ozayn_diagnostics_is_enabled(const ozayn_diagnostics_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->enabled;
}

/* ---- Auto-diagnose (event-driven) ---- */

void ozayn_diagnostics_on_task_failure(ozayn_diagnostics_manager_t *mgr,
                                        uint32_t task_id,
                                        ozayn_diag_component_t component,
                                        const char *reason) {
    if (!mgr || !mgr->initialized) return;

    char desc[OZAYN_DIAG_MAX_DESC_LEN];
    snprintf(desc, sizeof(desc), "Task #%u failed: %s", task_id, reason);
    ozayn_diagnostics_record_evidence(mgr, component, OZAYN_DIAG_TARGET_TASK,
                                       NULL, desc);
    ozayn_diagnostics_timeline_add(mgr, component, NULL, desc);
    ozayn_diagnostics_record_failure(mgr, component);

    if (ozayn_diagnostics_is_repeated_failure(mgr, component, REPEATED_FAILURE_THRESHOLD)) {
        LOG_WARN("DIAGNOSTICS", "REPEATED FAILURE DETECTED: %s",
                 ozayn_diag_component_name(component));
        mgr->stats.repeated_failures_detected++;
    }

    ozayn_diagnostics_add_finding(mgr, component, OZAYN_DIAG_SEV_ERROR,
                                   NULL, desc, reason,
                                   OZAYN_DIAG_CONFIDENCE_CERTAIN);
}

void ozayn_diagnostics_on_resource_failure(ozayn_diagnostics_manager_t *mgr,
                                            const char *resource_name,
                                            ozayn_diag_component_t component,
                                            const char *reason) {
    if (!mgr || !mgr->initialized) return;

    char desc[OZAYN_DIAG_MAX_DESC_LEN];
    snprintf(desc, sizeof(desc), "Resource '%s' failed: %s", resource_name, reason);
    ozayn_diagnostics_record_evidence(mgr, component, OZAYN_DIAG_TARGET_RESOURCE,
                                       NULL, desc);
    ozayn_diagnostics_timeline_add(mgr, component, NULL, desc);
    ozayn_diagnostics_record_failure(mgr, component);

    ozayn_diagnostics_add_finding(mgr, component, OZAYN_DIAG_SEV_ERROR,
                                   NULL, desc, reason,
                                   OZAYN_DIAG_CONFIDENCE_HIGH);
}

void ozayn_diagnostics_on_health_change(ozayn_diagnostics_manager_t *mgr,
                                         ozayn_diag_component_t component,
                                         ozayn_diag_health_t old_state,
                                         ozayn_diag_health_t new_state) {
    if (!mgr || !mgr->initialized) return;
    if (mgr->level < OZAYN_DIAG_LEVEL_DETAILED) return;

    char desc[OZAYN_DIAG_MAX_DESC_LEN];
    snprintf(desc, sizeof(desc), "Health: %s -> %s",
             ozayn_diag_health_name(old_state),
             ozayn_diag_health_name(new_state));
    ozayn_diagnostics_record_evidence(mgr, component, OZAYN_DIAG_TARGET_COMPONENT,
                                       NULL, desc);
    ozayn_diagnostics_timeline_add(mgr, component, NULL, desc);
}
