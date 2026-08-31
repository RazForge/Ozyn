#include "resource_limits.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* ---------- Lifecycle ---------- */

int ozayn_rl_init(ozayn_rl_mgr_t *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(ozayn_rl_mgr_t));
    LOG_INFO("RESOURCE_LIMITS", "Resource limits manager initialized");
    return 0;
}

void ozayn_rl_shutdown(ozayn_rl_mgr_t *mgr) {
    if (!mgr) return;
    LOG_INFO("RESOURCE_LIMITS", "Resource limits manager shut down (rejections=%u)",
             mgr->total_rejections);
}

/* ---------- Counter management ---------- */

int ozayn_rl_register(ozayn_rl_mgr_t *mgr, const char *name,
                       ozayn_rl_category_t category, uint32_t limit) {
    if (!mgr || !name) return -1;
    if (mgr->counter_count >= OZAYN_RL_MAX_COUNTERS) {
        LOG_ERROR("RESOURCE_LIMITS", "Counter limit reached");
        return -1;
    }

    /* Check duplicate */
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            LOG_ERROR("RESOURCE_LIMITS", "Duplicate counter: %s", name);
            return -1;
        }
    }

    ozayn_rl_counter_t *c = &mgr->counters[mgr->counter_count];
    memset(c, 0, sizeof(ozayn_rl_counter_t));
    ozayn_defense_strlcpy(c->name, name, sizeof(c->name));
    c->category = category;
    c->limit = limit;
    c->current = 0;
    c->peak = 0;
    c->total_acquired = 0;
    c->total_released = 0;
    c->total_rejected = 0;
    c->active = 1;

    mgr->counter_count++;
    LOG_DEBUG("RESOURCE_LIMITS", "Registered: %s (limit=%u)", name, limit);
    return 0;
}

int ozayn_rl_unregister(ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            mgr->counters[i].active = 0;
            LOG_DEBUG("RESOURCE_LIMITS", "Unregistered: %s", name);
            return 0;
        }
    }
    return -1;
}

/* ---------- Resource operations ---------- */

static ozayn_rl_counter_t *find_counter(ozayn_rl_mgr_t *mgr, const char *name) {
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0)
            return &mgr->counters[i];
    }
    return NULL;
}

int ozayn_rl_acquire(ozayn_rl_mgr_t *mgr, const char *name, uint32_t count) {
    if (!mgr || !name) return -1;

    ozayn_rl_counter_t *c = find_counter(mgr, name);
    if (!c) {
        LOG_ERROR("RESOURCE_LIMITS", "Unknown counter: %s", name);
        return -1;
    }

    if (c->current + count > c->limit) {
        c->total_rejected += count;
        mgr->total_rejections += count;
        LOG_WARN("RESOURCE_LIMITS", "Resource limit exceeded: %s (%u+%u > %u)",
                 name, c->current, count, c->limit);
        return -1;
    }

    c->current += count;
    c->total_acquired += count;
    if (c->current > c->peak) c->peak = c->current;
    return 0;
}

void ozayn_rl_release(ozayn_rl_mgr_t *mgr, const char *name, uint32_t count) {
    if (!mgr || !name) return;

    ozayn_rl_counter_t *c = find_counter(mgr, name);
    if (!c) return;

    if (count > c->current) {
        LOG_WARN("RESOURCE_LIMITS", "Over-release: %s (releasing %u, have %u)",
                 name, count, c->current);
        c->current = 0;
    } else {
        c->current -= count;
    }
    c->total_released += count;
}

/* ---------- Query ---------- */

uint32_t ozayn_rl_current(const ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return 0;
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0)
            return mgr->counters[i].current;
    }
    return 0;
}

uint32_t ozayn_rl_limit(const ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return 0;
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0)
            return mgr->counters[i].limit;
    }
    return 0;
}

uint32_t ozayn_rl_available(const ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return 0;
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            ozayn_rl_counter_t *c = (ozayn_rl_counter_t *)&mgr->counters[i];
            return c->limit > c->current ? c->limit - c->current : 0;
        }
    }
    return 0;
}

ozayn_rl_pressure_t ozayn_rl_pressure(const ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return OZAYN_RL_PRESSURE_NONE;

    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            const ozayn_rl_counter_t *c = &mgr->counters[i];
            if (c->limit == 0) return OZAYN_RL_PRESSURE_NONE;
            double pct = (double)c->current / (double)c->limit * 100.0;
            if (pct >= 100.0) return OZAYN_RL_PRESSURE_EXCEEDED;
            if (pct >= 95.0)  return OZAYN_RL_PRESSURE_CRITICAL;
            if (pct >= 80.0)  return OZAYN_RL_PRESSURE_HIGH;
            if (pct >= 60.0)  return OZAYN_RL_PRESSURE_MODERATE;
            return OZAYN_RL_PRESSURE_NONE;
        }
    }
    return OZAYN_RL_PRESSURE_NONE;
}

int ozayn_rl_can_acquire(const ozayn_rl_mgr_t *mgr,
                          const char *name, uint32_t count) {
    if (!mgr || !name) return 0;
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            return mgr->counters[i].current + count <= mgr->counters[i].limit;
        }
    }
    return 0;
}

/* ---------- Stats ---------- */

ozayn_rl_stats_t ozayn_rl_stats(const ozayn_rl_mgr_t *mgr) {
    ozayn_rl_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!mgr) return s;

    s.counter_count = mgr->counter_count;
    s.total_rejections = mgr->total_rejections;
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active) {
            s.total_acquired += (uint32_t)mgr->counters[i].total_acquired;
            s.total_released += (uint32_t)mgr->counters[i].total_released;
        }
    }
    return s;
}

/* ---------- Print ---------- */

void ozayn_rl_print(const ozayn_rl_mgr_t *mgr) {
    if (!mgr) return;

    LOG_INFO("RESOURCE_LIMITS", "=== Resource Limits (%u counters) ===", mgr->counter_count);
    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        const ozayn_rl_counter_t *c = &mgr->counters[i];
        if (!c->active) continue;

        double pct = c->limit > 0 ? (double)c->current / (double)c->limit * 100.0 : 0.0;
        LOG_INFO("RESOURCE_LIMITS", "  [%s] %u/%u (%.1f%%) peak=%u rejected=%lu",
                 c->name, c->current, c->limit, pct, c->peak,
                 (unsigned long)c->total_rejected);
    }
    LOG_INFO("RESOURCE_LIMITS", "Total rejections: %u", mgr->total_rejections);
}

void ozayn_rl_print_counter(const ozayn_rl_mgr_t *mgr, const char *name) {
    if (!mgr || !name) return;

    for (uint32_t i = 0; i < mgr->counter_count; i++) {
        if (mgr->counters[i].active && strcmp(mgr->counters[i].name, name) == 0) {
            const ozayn_rl_counter_t *c = &mgr->counters[i];
            double pct = c->limit > 0 ? (double)c->current / (double)c->limit * 100.0 : 0.0;
            LOG_INFO("RESOURCE_LIMITS", "[%s] current=%u limit=%u (%.1f%%) peak=%u",
                     c->name, c->current, c->limit, pct, c->peak);
            return;
        }
    }
    LOG_WARN("RESOURCE_LIMITS", "Counter not found: %s", name);
}
