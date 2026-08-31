#ifndef OZAYN_RESOURCE_LIMITS_H
#define OZAYN_RESOURCE_LIMITS_H

#include <stdint.h>

/*
 * resource_limits.h — Resource Limits & Backpressure (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Prevents resource exhaustion by enforcing configurable caps.
 *
 * Tracks: tasks, plugins, modules, event queue depth, IPC connections,
 * worker threads, memory allocations, file handles.
 */

/* ---- Constants ---- */

#define OZAYN_RL_MAX_COUNTERS  32
#define OZAYN_RL_MAX_NAME      64

/* ---- Resource categories ---- */

typedef enum {
    OZAYN_RL_TASKS          = 0,
    OZAYN_RL_PLUGINS        = 1,
    OZAYN_RL_MODULES        = 2,
    OZAYN_RL_EVENT_QUEUE    = 3,
    OZAYN_RL_IPC_CONN       = 4,
    OZAYN_RL_WORKERS        = 5,
    OZAYN_RL_ALLOCATIONS    = 6,
    OZAYN_RL_FILE_HANDLES   = 7,
    OZAYN_RL_CUSTOM         = 8,
} ozayn_rl_category_t;

/* ---- Backpressure state ---- */

typedef enum {
    OZAYN_RL_PRESSURE_NONE     = 0,  /* below 60% of limit */
    OZAYN_RL_PRESSURE_MODERATE = 1,  /* 60-80% of limit */
    OZAYN_RL_PRESSURE_HIGH     = 2,  /* 80-95% of limit */
    OZAYN_RL_PRESSURE_CRITICAL = 3,  /* 95-100% of limit */
    OZAYN_RL_PRESSURE_EXCEEDED = 4,  /* at or above limit */
} ozayn_rl_pressure_t;

/* ---- Resource counter ---- */

typedef struct {
    char                name[OZAYN_RL_MAX_NAME];
    ozayn_rl_category_t category;
    uint32_t            limit;        /* maximum allowed */
    uint32_t            current;      /* current usage */
    uint32_t            peak;         /* historical peak */
    uint64_t            total_acquired;
    uint64_t            total_released;
    uint64_t            total_rejected;
    int                 active;
} ozayn_rl_counter_t;

/* ---- Resource limits manager ---- */

typedef struct {
    ozayn_rl_counter_t counters[OZAYN_RL_MAX_COUNTERS];
    uint32_t           counter_count;
    uint32_t           total_rejections;
} ozayn_rl_mgr_t;

/* ---- Lifecycle ---- */

int  ozayn_rl_init(ozayn_rl_mgr_t *mgr);
void ozayn_rl_shutdown(ozayn_rl_mgr_t *mgr);

/* ---- Counter management ---- */

int  ozayn_rl_register(ozayn_rl_mgr_t *mgr, const char *name,
                        ozayn_rl_category_t category, uint32_t limit);
int  ozayn_rl_unregister(ozayn_rl_mgr_t *mgr, const char *name);

/* ---- Resource operations ---- */

/* Try to acquire a resource slot. Returns 0 on success, -1 if at limit. */
int  ozayn_rl_acquire(ozayn_rl_mgr_t *mgr, const char *name, uint32_t count);

/* Release resource slots. */
void ozayn_rl_release(ozayn_rl_mgr_t *mgr, const char *name, uint32_t count);

/* ---- Query ---- */

uint32_t          ozayn_rl_current(const ozayn_rl_mgr_t *mgr, const char *name);
uint32_t          ozayn_rl_limit(const ozayn_rl_mgr_t *mgr, const char *name);
uint32_t          ozayn_rl_available(const ozayn_rl_mgr_t *mgr, const char *name);
ozayn_rl_pressure_t ozayn_rl_pressure(const ozayn_rl_mgr_t *mgr, const char *name);
int               ozayn_rl_can_acquire(const ozayn_rl_mgr_t *mgr,
                                        const char *name, uint32_t count);

/* ---- Stats ---- */

typedef struct {
    uint32_t counter_count;
    uint32_t total_rejections;
    uint32_t total_acquired;
    uint32_t total_released;
} ozayn_rl_stats_t;

ozayn_rl_stats_t ozayn_rl_stats(const ozayn_rl_mgr_t *mgr);

/* ---- Print ---- */

void ozayn_rl_print(const ozayn_rl_mgr_t *mgr);
void ozayn_rl_print_counter(const ozayn_rl_mgr_t *mgr, const char *name);

#endif /* OZAYN_RESOURCE_LIMITS_H */
