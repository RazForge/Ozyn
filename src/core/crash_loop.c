#include "crash_loop.h"
#include "defense.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* ---------- Clock ---------- */

static uint64_t clock_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ---------- Lifecycle ---------- */

int ozayn_cl_init(ozayn_crash_loop_t *mgr, const ozayn_cl_policy_t *policy) {
    if (!mgr || !policy) return -1;

    memset(mgr, 0, sizeof(ozayn_crash_loop_t));
    mgr->policy = *policy;
    mgr->initialized = 1;

    LOG_INFO("CRASH_LOOP", "Initialized (max_failures=%u, window=%lu ms, cooldown=%lu ms)",
             policy->max_failures_in_window,
             (unsigned long)policy->window_duration_ms,
             (unsigned long)policy->cooldown_ms);
    return 0;
}

void ozayn_cl_shutdown(ozayn_crash_loop_t *mgr) {
    if (!mgr) return;
    mgr->initialized = 0;
    LOG_INFO("CRASH_LOOP", "Shut down (crash_loops=%u, quarantines=%u)",
             mgr->total_crash_loops, mgr->total_quarantines);
}

/* ---------- Component registration ---------- */

int ozayn_cl_register(ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->component_count >= OZAYN_CL_MAX_COMPONENTS) {
        LOG_ERROR("CRASH_LOOP", "Component limit reached");
        return -1;
    }

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return -1;
    }

    ozayn_cl_component_t *c = &mgr->components[mgr->component_count];
    memset(c, 0, sizeof(ozayn_cl_component_t));
    ozayn_defense_strlcpy(c->name, name, sizeof(c->name));
    c->max_restarts = mgr->policy.max_restarts;
    c->cooldown_ms = mgr->policy.cooldown_ms;
    c->window_start_us = clock_monotonic_us();
    c->active = 1;

    mgr->component_count++;
    LOG_DEBUG("CRASH_LOOP", "Registered: %s", name);
    return 0;
}

int ozayn_cl_unregister(ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0) {
            mgr->components[i].active = 0;
            return 0;
        }
    }
    return -1;
}

/* ---------- Find ---------- */

static ozayn_cl_component_t *cl_find(ozayn_crash_loop_t *mgr, const char *name) {
    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return &mgr->components[i];
    }
    return NULL;
}

/* ---------- Failure reporting ---------- */

int ozayn_cl_record_failure(ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    ozayn_cl_component_t *c = cl_find(mgr, name);
    if (!c) return -1;

    uint64_t now = clock_monotonic_us();

    /* Check if we're in cooldown */
    if (c->quarantined && now < c->cooldown_until_us) {
        LOG_WARN("CRASH_LOOP", "%s: quarantined, rejecting restart", name);
        return -1;
    }

    /* Check if window has expired → reset */
    if (now - c->window_start_us > mgr->policy.window_duration_ms * 1000ULL) {
        c->window_start_us = now;
        c->window_failures = 0;
    }

    /* Record the failure */
    uint32_t slot = c->failure_count % OZAYN_CL_MAX_HISTORY;
    clock_gettime(CLOCK_MONOTONIC, &c->failure_times[slot]);
    c->failure_count++;
    c->window_failures++;

    /* Check for crash loop */
    if (c->window_failures >= mgr->policy.max_failures_in_window) {
        c->quarantined = 1;
        c->cooldown_until_us = now + c->cooldown_ms * 1000ULL;
        mgr->total_crash_loops++;
        mgr->total_quarantines++;
        LOG_WARN("CRASH_LOOP", "%s: CRASH LOOP DETECTED (%u failures in window) → QUARANTINED",
                 name, c->window_failures);
        return -1;
    }

    /* Check restart limit */
    if (c->restart_count >= c->max_restarts) {
        c->quarantined = 1;
        c->cooldown_until_us = now + c->cooldown_ms * 1000ULL;
        mgr->total_quarantines++;
        LOG_WARN("CRASH_LOOP", "%s: restart limit reached (%u) → QUARANTINED",
                 name, c->restart_count);
        return -1;
    }

    c->restart_count++;
    return 0;
}

void ozayn_cl_record_success(ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return;

    ozayn_cl_component_t *c = cl_find(mgr, name);
    if (!c) return;

    c->window_failures = 0;
    c->window_start_us = clock_monotonic_us();

    if (c->quarantined) {
        c->quarantined = 0;
        mgr->total_recoveries++;
        LOG_INFO("CRASH_LOOP", "%s: recovered, released from quarantine", name);
    }
}

/* ---------- Quarantine ---------- */

void ozayn_cl_quarantine(ozayn_crash_loop_t *mgr, const char *name) {
    ozayn_cl_component_t *c = cl_find(mgr, name);
    if (!c) return;

    c->quarantined = 1;
    c->cooldown_until_us = clock_monotonic_us() + c->cooldown_ms * 1000ULL;
    mgr->total_quarantines++;
    LOG_WARN("CRASH_LOOP", "%s: QUARANTINED", name);
}

void ozayn_cl_release(ozayn_crash_loop_t *mgr, const char *name) {
    ozayn_cl_component_t *c = cl_find(mgr, name);
    if (!c) return;

    c->quarantined = 0;
    c->window_failures = 0;
    c->window_start_us = clock_monotonic_us();
    LOG_INFO("CRASH_LOOP", "%s: released from quarantine", name);
}

/* ---------- Query ---------- */

int ozayn_cl_is_quarantined(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return mgr->components[i].quarantined;
    }
    return 0;
}

int ozayn_cl_in_crash_loop(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return mgr->components[i].window_failures >= mgr->policy.max_failures_in_window;
    }
    return 0;
}

int ozayn_cl_can_restart(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0) {
            const ozayn_cl_component_t *c = &mgr->components[i];
            if (c->quarantined) return 0;
            if (c->restart_count >= c->max_restarts) return 0;
            return 1;
        }
    }
    return 0;
}

uint32_t ozayn_cl_failure_count(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return mgr->components[i].failure_count;
    }
    return 0;
}

uint32_t ozayn_cl_restart_count(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return 0;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return mgr->components[i].restart_count;
    }
    return 0;
}

const ozayn_cl_component_t *ozayn_cl_find(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;

    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && strcmp(mgr->components[i].name, name) == 0)
            return &mgr->components[i];
    }
    return NULL;
}

/* ---------- Stats ---------- */

ozayn_cl_stats_t ozayn_cl_stats(const ozayn_crash_loop_t *mgr) {
    ozayn_cl_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    s.component_count = mgr->component_count;
    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active && mgr->components[i].quarantined)
            s.quarantined_count++;
    }
    s.total_crash_loops = mgr->total_crash_loops;
    s.total_quarantines = mgr->total_quarantines;
    s.total_recoveries = mgr->total_recoveries;
    return s;
}

/* ---------- Print ---------- */

static void print_component(const ozayn_cl_component_t *c) {
    LOG_INFO("CRASH_LOOP", "  [%s] failures=%u restarts=%u quarantined=%s",
             c->name, c->failure_count, c->restart_count,
             c->quarantined ? "YES" : "NO");
}

void ozayn_cl_print(const ozayn_crash_loop_t *mgr) {
    if (!mgr) return;

    LOG_INFO("CRASH_LOOP", "=== Crash Loop Status (%u components) ===", mgr->component_count);
    for (uint32_t i = 0; i < mgr->component_count; i++) {
        if (mgr->components[i].active)
            print_component(&mgr->components[i]);
    }
    LOG_INFO("CRASH_LOOP", "Crash loops: %u, Quarantines: %u, Recoveries: %u",
             mgr->total_crash_loops, mgr->total_quarantines, mgr->total_recoveries);
}

void ozayn_cl_print_component(const ozayn_crash_loop_t *mgr, const char *name) {
    if (!mgr || !name) return;

    const ozayn_cl_component_t *c = ozayn_cl_find(mgr, name);
    if (!c) {
        LOG_WARN("CRASH_LOOP", "Component not found: %s", name);
        return;
    }
    print_component(c);
}
