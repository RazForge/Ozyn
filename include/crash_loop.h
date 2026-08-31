#ifndef OZAYN_CRASH_LOOP_H
#define OZAYN_CRASH_LOOP_H

#include <stdint.h>
#include <time.h>

/*
 * crash_loop.h — Crash-Loop Detection & Quarantine (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Detects repeated component failures, enforces cooldowns,
 * and quarantines broken components.
 */

/* ---- Constants ---- */

#define OZAYN_CL_MAX_COMPONENTS  64
#define OZAYN_CL_MAX_NAME        64
#define OZAYN_CL_MAX_HISTORY     16

/* ---- Component failure record ---- */

typedef struct {
    char        name[OZAYN_CL_MAX_NAME];
    struct      timespec failure_times[OZAYN_CL_MAX_HISTORY];
    uint32_t    failure_count;
    uint32_t    window_failures;    /* failures within current window */
    uint64_t    window_start_us;    /* start of current failure window */
    uint32_t    restart_count;
    uint32_t    max_restarts;       /* configurable limit */
    uint64_t    cooldown_ms;        /* cooldown after crash loop detected */
    uint64_t    cooldown_until_us;  /* when cooldown expires */
    int         quarantined;
    int         active;
} ozayn_cl_component_t;

/* ---- Crash loop detection policy ---- */

typedef struct {
    uint32_t max_failures_in_window;  /* failures to trigger crash loop */
    uint64_t window_duration_ms;      /* time window for counting failures */
    uint64_t cooldown_ms;             /* quarantine duration */
    uint32_t max_restarts;            /* max automatic restarts before quarantine */
} ozayn_cl_policy_t;

/* ---- Crash loop manager ---- */

typedef struct {
    ozayn_cl_component_t  components[OZAYN_CL_MAX_COMPONENTS];
    uint32_t              component_count;
    ozayn_cl_policy_t     policy;
    uint32_t              total_crash_loops;
    uint32_t              total_quarantines;
    uint32_t              total_recoveries;
    int                   initialized;
} ozayn_crash_loop_t;

/* ---- Lifecycle ---- */

int  ozayn_cl_init(ozayn_crash_loop_t *mgr, const ozayn_cl_policy_t *policy);
void ozayn_cl_shutdown(ozayn_crash_loop_t *mgr);

/* ---- Component registration ---- */

int  ozayn_cl_register(ozayn_crash_loop_t *mgr, const char *name);
int  ozayn_cl_unregister(ozayn_crash_loop_t *mgr, const char *name);

/* ---- Failure reporting ---- */

/* Report a component failure. Returns 0 normally, -1 if quarantined. */
int  ozayn_cl_record_failure(ozayn_crash_loop_t *mgr, const char *name);

/* Report a successful restart (resets failure count). */
void ozayn_cl_record_success(ozayn_crash_loop_t *mgr, const char *name);

/* ---- Quarantine ---- */

void ozayn_cl_quarantine(ozayn_crash_loop_t *mgr, const char *name);
void ozayn_cl_release(ozayn_crash_loop_t *mgr, const char *name);

/* ---- Query ---- */

int              ozayn_cl_is_quarantined(const ozayn_crash_loop_t *mgr, const char *name);
int              ozayn_cl_in_crash_loop(const ozayn_crash_loop_t *mgr, const char *name);
int              ozayn_cl_can_restart(const ozayn_crash_loop_t *mgr, const char *name);
uint32_t         ozayn_cl_failure_count(const ozayn_crash_loop_t *mgr, const char *name);
uint32_t         ozayn_cl_restart_count(const ozayn_crash_loop_t *mgr, const char *name);
const ozayn_cl_component_t *ozayn_cl_find(const ozayn_crash_loop_t *mgr, const char *name);

/* ---- Stats ---- */

typedef struct {
    uint32_t component_count;
    uint32_t quarantined_count;
    uint32_t total_crash_loops;
    uint32_t total_quarantines;
    uint32_t total_recoveries;
} ozayn_cl_stats_t;

ozayn_cl_stats_t ozayn_cl_stats(const ozayn_crash_loop_t *mgr);

/* ---- Print ---- */

void ozayn_cl_print(const ozayn_crash_loop_t *mgr);
void ozayn_cl_print_component(const ozayn_crash_loop_t *mgr, const char *name);

#endif /* OZAYN_CRASH_LOOP_H */
