#include "circuit_breaker.h"
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

const char *ozayn_cb_state_name(ozayn_cb_state_t state) {
    switch (state) {
        case OZAYN_CB_CLOSED:    return "CLOSED";
        case OZAYN_CB_OPEN:      return "OPEN";
        case OZAYN_CB_HALF_OPEN: return "HALF_OPEN";
    }
    return "UNKNOWN";
}

/* ---------- Lifecycle ---------- */

int ozayn_cb_init(ozayn_circuit_breaker_t *cb, const char *name,
                   const ozayn_cb_config_t *config) {
    if (!cb || !config) return -1;

    memset(cb, 0, sizeof(ozayn_circuit_breaker_t));
    if (name) ozayn_defense_strlcpy(cb->name, name, sizeof(cb->name));
    else ozayn_defense_strlcpy(cb->name, "unnamed", sizeof(cb->name));

    cb->config = *config;
    cb->state = OZAYN_CB_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->consecutive_successes = 0;
    cb->total_calls = 0;
    cb->total_failures = 0;
    cb->total_rejected = 0;
    cb->half_open_calls = 0;
    cb->initialized = 1;

    LOG_INFO("CIRCUIT_BREAKER", "Initialized: %s (fail_thresh=%u, timeout=%u ms)",
             cb->name, config->failure_threshold, config->timeout_ms);
    return 0;
}

void ozayn_cb_shutdown(ozayn_circuit_breaker_t *cb) {
    if (!cb) return;
    cb->initialized = 0;
    LOG_INFO("CIRCUIT_BREAKER", "Shutdown: %s", cb->name);
}

/* ---------- State machine ---------- */

int ozayn_cb_allow_call(ozayn_circuit_breaker_t *cb) {
    if (!cb || !cb->initialized) return 0;

    switch (cb->state) {
        case OZAYN_CB_CLOSED:
            return 1; /* always allow in closed state */

        case OZAYN_CB_OPEN:
            /* Check if timeout has elapsed → transition to half-open */
            if (clock_elapsed_ms(&cb->opened_at) >= cb->config.timeout_ms) {
                cb->state = OZAYN_CB_HALF_OPEN;
                cb->half_open_calls = 0;
                cb->consecutive_successes = 0;
                LOG_INFO("CIRCUIT_BREAKER", "%s: OPEN → HALF_OPEN (timeout elapsed)",
                         cb->name);
                return 1;
            }
            cb->total_rejected++;
            return 0; /* reject in open state */

        case OZAYN_CB_HALF_OPEN:
            if (cb->half_open_calls < cb->config.max_calls_half_open) {
                cb->half_open_calls++;
                return 1;
            }
            cb->total_rejected++;
            return 0;
    }
    return 0;
}

void ozayn_cb_record_success(ozayn_circuit_breaker_t *cb) {
    if (!cb || !cb->initialized) return;

    cb->total_calls++;
    cb->success_count++;
    cb->consecutive_successes++;

    switch (cb->state) {
        case OZAYN_CB_CLOSED:
            /* Reset failure count on success */
            cb->failure_count = 0;
            break;

        case OZAYN_CB_HALF_OPEN:
            if (cb->consecutive_successes >= cb->config.success_threshold) {
                cb->state = OZAYN_CB_CLOSED;
                cb->failure_count = 0;
                cb->consecutive_successes = 0;
                LOG_INFO("CIRCUIT_BREAKER", "%s: HALF_OPEN → CLOSED (recovered)",
                         cb->name);
            }
            break;

        case OZAYN_CB_OPEN:
            break;
    }
}

void ozayn_cb_record_failure(ozayn_circuit_breaker_t *cb) {
    if (!cb || !cb->initialized) return;

    cb->total_calls++;
    cb->total_failures++;
    cb->failure_count++;
    cb->consecutive_successes = 0;
    clock_gettime(CLOCK_MONOTONIC, &cb->last_failure_at);

    switch (cb->state) {
        case OZAYN_CB_CLOSED:
            if (cb->failure_count >= cb->config.failure_threshold) {
                cb->state = OZAYN_CB_OPEN;
                clock_gettime(CLOCK_MONOTONIC, &cb->opened_at);
                LOG_WARN("CIRCUIT_BREAKER", "%s: CLOSED → OPEN (failures=%u)",
                         cb->name, cb->failure_count);
            }
            break;

        case OZAYN_CB_HALF_OPEN:
            /* Any failure in half-open → back to open */
            cb->state = OZAYN_CB_OPEN;
            clock_gettime(CLOCK_MONOTONIC, &cb->opened_at);
            LOG_WARN("CIRCUIT_BREAKER", "%s: HALF_OPEN → OPEN (test failed)", cb->name);
            break;

        case OZAYN_CB_OPEN:
            break;
    }
}

void ozayn_cb_reset(ozayn_circuit_breaker_t *cb) {
    if (!cb) return;
    cb->state = OZAYN_CB_CLOSED;
    cb->failure_count = 0;
    cb->consecutive_successes = 0;
    cb->half_open_calls = 0;
    LOG_INFO("CIRCUIT_BREAKER", "%s: reset to CLOSED", cb->name);
}

/* ---------- Query ---------- */

ozayn_cb_state_t ozayn_cb_get_state(const ozayn_circuit_breaker_t *cb) {
    return cb ? cb->state : OZAYN_CB_CLOSED;
}

int ozayn_cb_is_open(const ozayn_circuit_breaker_t *cb) {
    return cb && cb->state == OZAYN_CB_OPEN;
}

/* ---------- Stats ---------- */

ozayn_cb_stats_t ozayn_cb_stats(const ozayn_circuit_breaker_t *cb) {
    ozayn_cb_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!cb) return s;
    s.total_calls = cb->total_calls;
    s.total_failures = cb->total_failures;
    s.total_rejected = cb->total_rejected;
    s.failure_count = cb->failure_count;
    s.success_count = cb->success_count;
    s.state = cb->state;
    return s;
}

/* ---------- Print ---------- */

void ozayn_cb_print(const ozayn_circuit_breaker_t *cb) {
    if (!cb) return;

    LOG_INFO("CIRCUIT_BREAKER", "=== %s ===", cb->name);
    LOG_INFO("CIRCUIT_BREAKER", "State: %s", ozayn_cb_state_name(cb->state));
    LOG_INFO("CIRCUIT_BREAKER", "Calls: %u (failures=%u, rejected=%u)",
             cb->total_calls, cb->total_failures, cb->total_rejected);
    LOG_INFO("CIRCUIT_BREAKER", "Failure count: %u/%u",
             cb->failure_count, cb->config.failure_threshold);
    if (cb->state == OZAYN_CB_OPEN) {
        LOG_INFO("CIRCUIT_BREAKER", "Retry in: %lu ms",
                 cb->config.timeout_ms - clock_elapsed_ms(&cb->opened_at));
    }
}
