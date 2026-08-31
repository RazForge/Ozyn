#include "health_tracker.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* ---------- Clock ---------- */

static uint64_t clock_elapsed_ms(const struct timespec *from) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t a = (uint64_t)from->tv_sec * 1000ULL + (uint64_t)from->tv_nsec / 1000000ULL;
    uint64_t b = (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
    return b >= a ? b - a : 0;
}

/* ---------- Names ---------- */

const char *ozayn_ht_state_name(ozayn_ht_state_t state) {
    switch (state) {
        case OZAYN_HT_UNKNOWN:      return "UNKNOWN";
        case OZAYN_HT_STARTING:     return "STARTING";
        case OZAYN_HT_HEALTHY:      return "HEALTHY";
        case OZAYN_HT_DEGRADED:     return "DEGRADED";
        case OZAYN_HT_UNRESPONSIVE: return "UNRESPONSIVE";
        case OZAYN_HT_FAILED:       return "FAILED";
        case OZAYN_HT_RECOVERING:   return "RECOVERING";
        case OZAYN_HT_STOPPING:     return "STOPPING";
        case OZAYN_HT_STOPPED:      return "STOPPED";
        case OZAYN_HT_QUARANTINED:  return "QUARANTINED";
    }
    return "UNKNOWN";
}

const char *ozayn_ht_criticality_name(ozayn_ht_criticality_t crit) {
    switch (crit) {
        case OZAYN_HT_CRITICAL:  return "CRITICAL";
        case OZAYN_HT_IMPORTANT: return "IMPORTANT";
        case OZAYN_HT_OPTIONAL:  return "OPTIONAL";
    }
    return "UNKNOWN";
}

/* ---------- Lifecycle ---------- */

int ozayn_ht_init(ozayn_health_tracker_t *ht, const ozayn_ht_watchdog_config_t *wd_cfg) {
    if (!ht || !wd_cfg) return -1;

    memset(ht, 0, sizeof(ozayn_health_tracker_t));
    ht->watchdog = *wd_cfg;
    clock_gettime(CLOCK_MONOTONIC, &ht->last_watchdog_check);
    ht->initialized = 1;

    LOG_INFO("HEALTH_TRACKER", "Initialized (check_interval=%u ms, auto_degrade=%d)",
             wd_cfg->check_interval_ms, wd_cfg->auto_degrade);
    return 0;
}

void ozayn_ht_shutdown(ozayn_health_tracker_t *ht) {
    if (!ht) return;
    ht->initialized = 0;
    LOG_INFO("HEALTH_TRACKER", "Shut down (%u degraded, %u failed, %u quarantined)",
             ht->total_degraded, ht->total_failed, ht->total_quarantined);
}

/* ---------- Component registration ---------- */

int ozayn_ht_register(ozayn_health_tracker_t *ht, const char *name,
                       ozayn_ht_criticality_t criticality,
                       uint32_t heartbeat_interval_ms,
                       uint32_t heartbeat_timeout_ms) {
    if (!ht || !name) return -1;
    if (ht->component_count >= OZAYN_HT_MAX_COMPONENTS) {
        LOG_ERROR("HEALTH_TRACKER", "Component limit reached");
        return -1;
    }

    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && strcmp(ht->components[i].name, name) == 0) {
            LOG_ERROR("HEALTH_TRACKER", "Duplicate component: %s", name);
            return -1;
        }
    }

    ozayn_ht_component_t *c = &ht->components[ht->component_count];
    memset(c, 0, sizeof(ozayn_ht_component_t));
    ozayn_defense_strlcpy(c->name, name, sizeof(c->name));
    c->state = OZAYN_HT_STARTING;
    c->criticality = criticality;
    c->heartbeat_interval_ms = heartbeat_interval_ms;
    c->heartbeat_timeout_ms = heartbeat_timeout_ms;
    clock_gettime(CLOCK_MONOTONIC, &c->last_heartbeat);
    clock_gettime(CLOCK_MONOTONIC, &c->last_state_change);
    c->miss_count = 0;
    c->total_heartbeats = 0;
    c->total_misses = 0;
    c->watchdog_enabled = 1;
    c->active = 1;

    ht->component_count++;
    LOG_DEBUG("HEALTH_TRACKER", "Registered: %s (crit=%s, hb_interval=%u ms)",
              name, ozayn_ht_criticality_name(criticality), heartbeat_interval_ms);
    return 0;
}

int ozayn_ht_unregister(ozayn_health_tracker_t *ht, const char *name) {
    if (!ht || !name) return -1;

    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && strcmp(ht->components[i].name, name) == 0) {
            ht->components[i].active = 0;
            return 0;
        }
    }
    return -1;
}

/* ---------- Heartbeat ---------- */

void ozayn_ht_heartbeat(ozayn_health_tracker_t *ht, const char *name) {
    if (!ht || !name) return;

    for (uint32_t i = 0; i < ht->component_count; i++) {
        ozayn_ht_component_t *c = &ht->components[i];
        if (c->active && strcmp(c->name, name) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &c->last_heartbeat);
            c->miss_count = 0;
            c->total_heartbeats++;

            if (c->state == OZAYN_HT_UNRESPONSIVE || c->state == OZAYN_HT_DEGRADED) {
                c->state = OZAYN_HT_HEALTHY;
                clock_gettime(CLOCK_MONOTONIC, &c->last_state_change);
                LOG_INFO("HEALTH_TRACKER", "%s: → HEALTHY (heartbeat received)", name);
            }
            return;
        }
    }
}

/* ---------- State management ---------- */

static void transition_state(ozayn_health_tracker_t *ht, ozayn_ht_component_t *c,
                              ozayn_ht_state_t new_state) {
    if (c->state == new_state) return;

    ozayn_ht_state_t old = c->state;
    c->state = new_state;
    clock_gettime(CLOCK_MONOTONIC, &c->last_state_change);

    LOG_INFO("HEALTH_TRACKER", "%s: %s → %s",
             c->name, ozayn_ht_state_name(old), ozayn_ht_state_name(new_state));

    if (new_state == OZAYN_HT_DEGRADED) ht->total_degraded++;
    else if (new_state == OZAYN_HT_FAILED) ht->total_failed++;
    else if (new_state == OZAYN_HT_QUARANTINED) ht->total_quarantined++;
    else if (new_state == OZAYN_HT_HEALTHY && (old == OZAYN_HT_DEGRADED || old == OZAYN_HT_FAILED))
        ht->total_recovered++;
}

void ozayn_ht_set_state(ozayn_health_tracker_t *ht, const char *name,
                         ozayn_ht_state_t state) {
    if (!ht || !name) return;

    for (uint32_t i = 0; i < ht->component_count; i++) {
        ozayn_ht_component_t *c = &ht->components[i];
        if (c->active && strcmp(c->name, name) == 0) {
            transition_state(ht, c, state);
            return;
        }
    }
}

void ozayn_ht_quarantine(ozayn_health_tracker_t *ht, const char *name) {
    ozayn_ht_set_state(ht, name, OZAYN_HT_QUARANTINED);
}

void ozayn_ht_unquarantine(ozayn_health_tracker_t *ht, const char *name) {
    ozayn_ht_set_state(ht, name, OZAYN_HT_RECOVERING);
}

/* ---------- Watchdog tick ---------- */

void ozayn_ht_watchdog_tick(ozayn_health_tracker_t *ht) {
    if (!ht) return;

    if (clock_elapsed_ms(&ht->last_watchdog_check) < ht->watchdog.check_interval_ms)
        return;

    clock_gettime(CLOCK_MONOTONIC, &ht->last_watchdog_check);

    for (uint32_t i = 0; i < ht->component_count; i++) {
        ozayn_ht_component_t *c = &ht->components[i];
        if (!c->active || !c->watchdog_enabled) continue;
        if (c->state == OZAYN_HT_STOPPED || c->state == OZAYN_HT_QUARANTINED) continue;

        uint64_t since_heartbeat = clock_elapsed_ms(&c->last_heartbeat);

        if (since_heartbeat > c->heartbeat_timeout_ms) {
            c->miss_count++;
            c->total_misses++;

            if (ht->watchdog.auto_fail &&
                c->miss_count >= ht->watchdog.max_misses) {
                transition_state(ht, c, OZAYN_HT_FAILED);
                LOG_WARN("HEALTH_TRACKER", "%s: FAILED (missed %u heartbeats)",
                         c->name, c->miss_count);
            } else if (ht->watchdog.auto_degrade &&
                       c->state == OZAYN_HT_HEALTHY) {
                transition_state(ht, c, OZAYN_HT_DEGRADED);
                LOG_WARN("HEALTH_TRACKER", "%s: DEGRADED (missed heartbeat)",
                         c->name);
            }
        }
    }
}

/* ---------- Query ---------- */

ozayn_ht_state_t ozayn_ht_get_state(const ozayn_health_tracker_t *ht, const char *name) {
    if (!ht || !name) return OZAYN_HT_UNKNOWN;

    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && strcmp(ht->components[i].name, name) == 0)
            return ht->components[i].state;
    }
    return OZAYN_HT_UNKNOWN;
}

const ozayn_ht_component_t *ozayn_ht_find(const ozayn_health_tracker_t *ht,
                                           const char *name) {
    if (!ht || !name) return NULL;

    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && strcmp(ht->components[i].name, name) == 0)
            return &ht->components[i];
    }
    return NULL;
}

int ozayn_ht_is_healthy(const ozayn_health_tracker_t *ht, const char *name) {
    return ozayn_ht_get_state(ht, name) == OZAYN_HT_HEALTHY;
}

int ozayn_ht_any_failed(const ozayn_health_tracker_t *ht) {
    return ozayn_ht_count_by_state(ht, OZAYN_HT_FAILED) > 0;
}

int ozayn_ht_any_degraded(const ozayn_health_tracker_t *ht) {
    return ozayn_ht_count_by_state(ht, OZAYN_HT_DEGRADED) > 0;
}

int ozayn_ht_count_by_state(const ozayn_health_tracker_t *ht, ozayn_ht_state_t state) {
    if (!ht) return 0;
    int count = 0;
    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && ht->components[i].state == state)
            count++;
    }
    return count;
}

int ozayn_ht_count_by_criticality(const ozayn_health_tracker_t *ht,
                                   ozayn_ht_criticality_t crit) {
    if (!ht) return 0;
    int count = 0;
    for (uint32_t i = 0; i < ht->component_count; i++) {
        if (ht->components[i].active && ht->components[i].criticality == crit)
            count++;
    }
    return count;
}

/* ---------- Stats ---------- */

ozayn_ht_stats_t ozayn_ht_stats(const ozayn_health_tracker_t *ht) {
    ozayn_ht_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!ht) return s;

    s.component_count = ht->component_count;
    s.healthy_count = (uint32_t)ozayn_ht_count_by_state(ht, OZAYN_HT_HEALTHY);
    s.degraded_count = (uint32_t)ozayn_ht_count_by_state(ht, OZAYN_HT_DEGRADED);
    s.failed_count = (uint32_t)ozayn_ht_count_by_state(ht, OZAYN_HT_FAILED);
    s.quarantined_count = (uint32_t)ozayn_ht_count_by_state(ht, OZAYN_HT_QUARANTINED);
    s.total_degraded = ht->total_degraded;
    s.total_failed = ht->total_failed;
    s.total_recovered = ht->total_recovered;
    s.total_quarantined = ht->total_quarantined;
    return s;
}

/* ---------- Print ---------- */

void ozayn_ht_print(const ozayn_health_tracker_t *ht) {
    if (!ht) return;

    LOG_INFO("HEALTH_TRACKER", "=== Health Status (%u components) ===", ht->component_count);
    for (uint32_t i = 0; i < ht->component_count; i++) {
        const ozayn_ht_component_t *c = &ht->components[i];
        if (!c->active) continue;

        LOG_INFO("HEALTH_TRACKER", "  [%s] state=%s crit=%s hb_misses=%u",
                 c->name,
                 ozayn_ht_state_name(c->state),
                 ozayn_ht_criticality_name(c->criticality),
                 c->miss_count);
    }
}

void ozayn_ht_print_component(const ozayn_health_tracker_t *ht, const char *name) {
    const ozayn_ht_component_t *c = ozayn_ht_find(ht, name);
    if (!c) {
        LOG_WARN("HEALTH_TRACKER", "Component not found: %s", name);
        return;
    }

    LOG_INFO("HEALTH_TRACKER", "=== %s ===", c->name);
    LOG_INFO("HEALTH_TRACKER", "State: %s", ozayn_ht_state_name(c->state));
    LOG_INFO("HEALTH_TRACKER", "Criticality: %s", ozayn_ht_criticality_name(c->criticality));
    LOG_INFO("HEALTH_TRACKER", "Heartbeats: %u (misses=%u)", c->total_heartbeats, c->total_misses);
}
