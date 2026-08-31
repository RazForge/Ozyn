#ifndef OZAYN_PERF_MGR_H
#define OZAYN_PERF_MGR_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * perf_mgr.h — Performance & Optimization Manager (Stage 28).
 *
 * Self-contained header — no circular includes.
 * Collects, tracks, and reports runtime performance metrics.
 *
 * Responsibilities:
 *   - Startup time measurement
 *   - CPU usage tracking (user/system time)
 *   - Memory usage tracking (RSS, virtual)
 *   - Event throughput measurement
 *   - Benchmark subsystem operations
 *   - Threshold alerts with event integration
 *   - Periodic metric snapshots
 */

/* ---- Constants ---- */

#define OZAYN_PERF_MAX_BENCHMARKS    32
#define OZAYN_PERF_MAX_NAME          64
#define OZAYN_PERF_MAX_SNAPSHOTS     256
#define OZAYN_PERF_MAX_THRESHOLDS   16

/* ---- Metric types ---- */

typedef enum {
    OZAYN_PERF_METRIC_COUNTER = 0,   /* monotonically increasing */
    OZAYN_PERF_METRIC_GAUGE   = 1,   /* can go up or down */
} ozayn_perf_metric_type_t;

/* ---- Threshold severity ---- */

typedef enum {
    OZAYN_PERF_THRESH_WARNING   = 0,
    OZAYN_PERF_THRESH_CRITICAL  = 1,
} ozayn_perf_threshold_severity_t;

/* ---- CPU usage snapshot ---- */

typedef struct {
    uint64_t user_us;    /* user CPU time in microseconds */
    uint64_t system_us;  /* system CPU time in microseconds */
    double   user_pct;   /* user CPU percentage (0.0 - 100.0+) */
    double   system_pct; /* system CPU percentage (0.0 - 100.0+) */
    double   total_pct;  /* total CPU percentage */
} ozayn_perf_cpu_t;

/* ---- Memory usage snapshot ---- */

typedef struct {
    uint64_t resident_bytes;   /* RSS (resident set size) */
    uint64_t virtual_bytes;    /* virtual memory size */
    uint64_t shared_bytes;     /* shared memory */
    double   resident_mb;      /* RSS in megabytes */
    double   virtual_mb;       /* virtual in megabytes */
} ozayn_perf_memory_t;

/* ---- Event throughput snapshot ---- */

typedef struct {
    uint64_t published_total;   /* total events published */
    uint64_t dispatched_total;  /* total events dispatched */
    uint64_t dropped_total;     /* total events dropped (overflow) */
    uint32_t queue_depth;       /* current queue depth */
    uint32_t queue_capacity;    /* max queue capacity */
    double   publish_rate;      /* events/sec since last snapshot */
    double   dispatch_rate;     /* dispatches/sec since last snapshot */
} ozayn_perf_events_t;

/* ---- Scheduler throughput snapshot ---- */

typedef struct {
    uint64_t tasks_submitted;    /* total tasks submitted */
    uint64_t tasks_executed;     /* total tasks executed */
    uint64_t tasks_completed;    /* total tasks completed */
    uint32_t tasks_active;       /* currently active tasks */
    double   submit_rate;        /* tasks/sec since last snapshot */
} ozayn_perf_scheduler_t;

/* ---- Performance snapshot ---- */

typedef struct {
    uint64_t             timestamp_us;   /* monotonic microsecond timestamp */
    uint64_t             uptime_us;      /* time since init */
    ozayn_perf_cpu_t     cpu;
    ozayn_perf_memory_t  memory;
    ozayn_perf_events_t  events;
    ozayn_perf_scheduler_t scheduler;
} ozayn_perf_snapshot_t;

/* ---- Benchmark descriptor ---- */

typedef enum {
    OZAYN_PERF_BENCH_IDLE       = 0,
    OZAYN_PERF_BENCH_RUNNING    = 1,
    OZAYN_PERF_BENCH_COMPLETED  = 2,
    OZAYN_PERF_BENCH_FAILED     = 3,
    OZAYN_PERF_BENCH_CANCELLED  = 4,
} ozayn_perf_bench_state_t;

typedef struct {
    int                            active;
    char                           name[OZAYN_PERF_MAX_NAME];
    ozayn_perf_bench_state_t       state;
    uint32_t                       iterations;
    uint32_t                       total_iterations;
    uint64_t                       total_us;
    uint64_t                       min_us;
    uint64_t                       max_us;
    uint64_t                       last_us;
    double                         avg_us;
    double                         stddev_us;
} ozayn_perf_benchmark_t;

/* ---- Threshold descriptor ---- */

typedef struct {
    int                                    active;
    char                                   name[OZAYN_PERF_MAX_NAME];
    ozayn_perf_threshold_severity_t        severity;
    double                                 warning_threshold;
    double                                 critical_threshold;
} ozayn_perf_threshold_t;

/* ---- Performance statistics ---- */

typedef struct {
    uint64_t startup_duration_us;
    uint32_t snapshot_count;
    uint32_t benchmark_count;
    uint32_t threshold_count;
    uint32_t threshold_warnings;
    uint32_t threshold_criticals;
    double   current_cpu_pct;
    double   current_memory_mb;
    uint64_t events_published_total;
    uint64_t events_dispatched_total;
    uint64_t events_dropped_total;
} ozayn_perf_stats_t;

/* ---- Configuration ---- */

typedef struct {
    uint32_t snapshot_interval_ms;  /* auto-snapshot interval (0=disabled) */
    uint32_t max_snapshots;         /* circular buffer size */
    uint32_t startup_timeout_ms;    /* max time to record as startup */
    int      auto_cpu_check;        /* check CPU each snapshot */
    int      auto_memory_check;     /* check memory each snapshot */
} ozayn_perf_config_t;

/* ---- Forward declarations for events ---- */
typedef struct ozayn_event_engine_s ozayn_event_engine_t;

/* ---- Manager ---- */

typedef struct {
    ozayn_perf_config_t         config;
    ozayn_event_engine_t       *events;
    /* Init time */
    struct timespec             init_time;
    int                         initialized;
    /* Startup timing */
    struct timespec             startup_start;
    struct timespec             startup_end;
    uint64_t                    startup_duration_us;
    int                         startup_recorded;
    /* Previous CPU snapshot (for delta calculation) */
    uint64_t                    prev_cpu_user_us;
    uint64_t                    prev_cpu_system_us;
    struct timespec             prev_snapshot_time;
    /* Snapshots (circular buffer) */
    ozayn_perf_snapshot_t       snapshots[OZAYN_PERF_MAX_SNAPSHOTS];
    uint32_t                    snapshot_head;
    uint32_t                    snapshot_count;
    /* Benchmarks */
    ozayn_perf_benchmark_t      benchmarks[OZAYN_PERF_MAX_BENCHMARKS];
    uint32_t                    benchmark_count;
    /* Thresholds */
    ozayn_perf_threshold_t      thresholds[OZAYN_PERF_MAX_THRESHOLDS];
    uint32_t                    threshold_count;
    /* Stats counters */
    uint32_t                    threshold_warnings;
    uint32_t                    threshold_criticals;
    /* Event counters (for publish/dispatch rate calculation) */
    uint64_t                    last_published_total;
    uint64_t                    last_dispatched_total;
    uint64_t                    last_dropped_total;
    uint32_t                    last_queue_depth;
    /* Scheduler counters (for rate calculation) */
    uint64_t                    last_tasks_submitted;
    uint64_t                    last_tasks_executed;
    uint64_t                    last_tasks_completed;
    uint32_t                    last_tasks_active;
} ozayn_perf_manager_t;

/* ================================================================
 * API
 * ================================================================ */

/* Lifecycle */
int  ozayn_perf_init(ozayn_perf_manager_t *mgr, const ozayn_perf_config_t *config);
void ozayn_perf_shutdown(ozayn_perf_manager_t *mgr);
void ozayn_perf_set_events(ozayn_perf_manager_t *mgr, ozayn_event_engine_t *events);

/* Startup timing */
void ozayn_perf_startup_begin(ozayn_perf_manager_t *mgr);
void ozayn_perf_startup_end(ozayn_perf_manager_t *mgr);
uint64_t ozayn_perf_startup_duration_us(const ozayn_perf_manager_t *mgr);
double ozayn_perf_startup_duration_ms(const ozayn_perf_manager_t *mgr);

/* Snapshot collection */
int  ozayn_perf_snapshot_collect(ozayn_perf_manager_t *mgr,
                                  ozayn_perf_snapshot_t *out);
int  ozayn_perf_snapshot_store(ozayn_perf_manager_t *mgr);
const ozayn_perf_snapshot_t *ozayn_perf_snapshot_get(
                                  const ozayn_perf_manager_t *mgr, uint32_t index);
uint32_t ozayn_perf_snapshot_count(const ozayn_perf_manager_t *mgr);
const ozayn_perf_snapshot_t *ozayn_perf_snapshot_latest(
                                  const ozayn_perf_manager_t *mgr);

/* Benchmarks */
int  ozayn_perf_bench_register(ozayn_perf_manager_t *mgr, const char *name);
int  ozayn_perf_bench_begin(ozayn_perf_manager_t *mgr, const char *name);
int  ozayn_perf_bench_record(ozayn_perf_manager_t *mgr, const char *name, uint64_t duration_us);
int  ozayn_perf_bench_end(ozayn_perf_manager_t *mgr, const char *name);
int  ozayn_perf_bench_cancel(ozayn_perf_manager_t *mgr, const char *name);
const ozayn_perf_benchmark_t *ozayn_perf_bench_find(
                                  const ozayn_perf_manager_t *mgr, const char *name);
uint32_t ozayn_perf_bench_count(const ozayn_perf_manager_t *mgr);

/* Thresholds */
int  ozayn_perf_threshold_register(ozayn_perf_manager_t *mgr,
                                    const char *name,
                                    double warning,
                                    double critical);
void ozayn_perf_threshold_check(ozayn_perf_manager_t *mgr,
                                 const char *name, double value);

/* Elapsed time utility */
uint64_t ozayn_perf_elapsed_us(const struct timespec *start,
                                const struct timespec *end);

/* Timing utility: time a function call */
uint64_t ozayn_perf_time_us(void (*fn)(void *arg), void *arg);

/* Stats */
ozayn_perf_stats_t ozayn_perf_stats(const ozayn_perf_manager_t *mgr);

/* Print / debug */
void ozayn_perf_print_snapshot(const ozayn_perf_manager_t *mgr,
                                const ozayn_perf_snapshot_t *snap);
void ozayn_perf_print_latest(const ozayn_perf_manager_t *mgr);
void ozayn_perf_print_benchmarks(const ozayn_perf_manager_t *mgr);
void ozayn_perf_print_thresholds(const ozayn_perf_manager_t *mgr);
void ozayn_perf_print_stats(const ozayn_perf_manager_t *mgr);

/* Names */
const char *ozayn_perf_bench_state_name(ozayn_perf_bench_state_t state);
const char *ozayn_perf_threshold_severity_name(ozayn_perf_threshold_severity_t sev);

#endif /* OZAYN_PERF_MGR_H */
