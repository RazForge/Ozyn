#ifndef OZAYN_CIRCUIT_BREAKER_H
#define OZAYN_CIRCUIT_BREAKER_H

#include <stdint.h>
#include <time.h>

/*
 * circuit_breaker.h — Circuit Breaker Pattern (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Prevents cascading failures by stopping calls to failing dependencies.
 *
 * States:
 *   CLOSED   — Normal operation, calls pass through
 *   OPEN     — Dependency is failing, calls are blocked
 *   HALF_OPEN — Testing recovery, limited calls allowed
 */

/* ---- States ---- */

typedef enum {
    OZAYN_CB_CLOSED     = 0,
    OZAYN_CB_OPEN       = 1,
    OZAYN_CB_HALF_OPEN  = 2,
} ozayn_cb_state_t;

/* ---- Configuration ---- */

typedef struct {
    uint32_t failure_threshold;    /* failures before opening */
    uint32_t success_threshold;    /* successes to close from half-open */
    uint32_t timeout_ms;           /* time in OPEN before half-open */
    uint32_t max_calls_half_open;  /* calls allowed in half-open */
} ozayn_cb_config_t;

/* ---- Circuit breaker ---- */

typedef struct {
    char                    name[128];
    ozayn_cb_config_t       config;
    ozayn_cb_state_t        state;
    uint32_t                failure_count;
    uint32_t                success_count;
    uint32_t                consecutive_successes;
    uint32_t                total_calls;
    uint32_t                total_failures;
    uint32_t                total_rejected;
    uint32_t                half_open_calls;
    struct timespec         opened_at;
    struct timespec         last_failure_at;
    int                     initialized;
} ozayn_circuit_breaker_t;

/* ---- Lifecycle ---- */

int  ozayn_cb_init(ozayn_circuit_breaker_t *cb, const char *name,
                    const ozayn_cb_config_t *config);
void ozayn_cb_shutdown(ozayn_circuit_breaker_t *cb);

/* ---- State machine ---- */

/* Call this before attempting the operation */
int  ozayn_cb_allow_call(ozayn_circuit_breaker_t *cb);

/* Call after a successful operation */
void ozayn_cb_record_success(ozayn_circuit_breaker_t *cb);

/* Call after a failed operation */
void ozayn_cb_record_failure(ozayn_circuit_breaker_t *cb);

/* Manually reset to closed */
void ozayn_cb_reset(ozayn_circuit_breaker_t *cb);

/* ---- Query ---- */

ozayn_cb_state_t ozayn_cb_get_state(const ozayn_circuit_breaker_t *cb);
const char      *ozayn_cb_state_name(ozayn_cb_state_t state);
int              ozayn_cb_is_open(const ozayn_circuit_breaker_t *cb);

/* ---- Stats ---- */

typedef struct {
    uint32_t total_calls;
    uint32_t total_failures;
    uint32_t total_rejected;
    uint32_t failure_count;
    uint32_t success_count;
    ozayn_cb_state_t state;
} ozayn_cb_stats_t;

ozayn_cb_stats_t ozayn_cb_stats(const ozayn_circuit_breaker_t *cb);

/* ---- Print ---- */

void ozayn_cb_print(const ozayn_circuit_breaker_t *cb);

#endif /* OZAYN_CIRCUIT_BREAKER_H */
