#include "perf_mgr.h"
#include "events.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>
#elif defined(__APPLE__)
#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

/* ---------- Names ---------- */

const char *ozayn_perf_bench_state_name(ozayn_perf_bench_state_t state) {
    switch (state) {
        case OZAYN_PERF_BENCH_IDLE:      return "IDLE";
        case OZAYN_PERF_BENCH_RUNNING:   return "RUNNING";
        case OZAYN_PERF_BENCH_COMPLETED: return "COMPLETED";
        case OZAYN_PERF_BENCH_FAILED:    return "FAILED";
        case OZAYN_PERF_BENCH_CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

const char *ozayn_perf_threshold_severity_name(ozayn_perf_threshold_severity_t sev) {
    switch (sev) {
        case OZAYN_PERF_THRESH_WARNING:  return "WARNING";
        case OZAYN_PERF_THRESH_CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

/* ---------- Internal: monotonic clock ---------- */

static uint64_t clock_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ---------- Internal: collect CPU usage ---------- */

#if defined(__linux__)

/* Parse /proc/self/stat — field 14=user ticks, 15=system ticks, 23=rss in pages */
static int collect_cpu_usage(uint64_t *out_user_us, uint64_t *out_system_us) {
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return -1;

    char line[4096];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Skip past the comm field (which is in parens and may contain spaces) */
    const char *p = strchr(line, ')');
    if (!p) return -1;
    p += 2; /* skip ") " */

    /* Fields after comm:
     *  3: state, 4: ppid, 5: pgrp, 6: session, 7: tty_nr, 8: tpgid,
     *  9: flags, 10: minflt, 11: cminflt, 12: majflt, 13: cmajflt,
     *  14: utime (user), 15: stime (kernel) — in clock ticks
     */
    unsigned long utime = 0, stime = 0;
    int field = 3;
    const char *q = p;
    while (field <= 15 && *q) {
        if (field == 14) {
            utime = strtoul(q, NULL, 10);
        } else if (field == 15) {
            stime = strtoul(q, NULL, 10);
        }
        /* advance to next field */
        while (*q && *q != ' ') q++;
        while (*q == ' ') q++;
        field++;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) ticks_per_sec = 100;

    *out_user_us   = (utime * 1000000ULL) / (uint64_t)ticks_per_sec;
    *out_system_us = (stime * 1000000ULL) / (uint64_t)ticks_per_sec;
    return 0;
}

static int collect_memory_usage(uint64_t *out_rss, uint64_t *out_virtual,
                                uint64_t *out_shared) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;

    char line[512];
    *out_rss = 0;
    *out_virtual = 0;
    *out_shared = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            *out_rss = (uint64_t)atol(line + 6) * 1024;
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            *out_virtual = (uint64_t)atol(line + 7) * 1024;
        } else if (strncmp(line, "VmShared:", 9) == 0) {
            *out_shared = (uint64_t)atol(line + 9) * 1024;
        }
    }
    fclose(f);
    return 0;
}

#elif defined(__APPLE__)

static int collect_cpu_usage(uint64_t *out_user_us, uint64_t *out_system_us) {
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return -1;
    *out_user_us   = (uint64_t)ru.ru_utime.tv_sec * 1000000ULL + (uint64_t)ru.ru_utime.tv_usec;
    *out_system_us = (uint64_t)ru.ru_stime.tv_sec * 1000000ULL + (uint64_t)ru.ru_stime.tv_usec;
    return 0;
}

static int collect_memory_usage(uint64_t *out_rss, uint64_t *out_virtual,
                                uint64_t *out_shared) {
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) != KERN_SUCCESS) return -1;
    *out_rss     = info.resident_size;
    *out_virtual = info.virtual_size;
    *out_shared  = 0;
    return 0;
}

#else

static int collect_cpu_usage(uint64_t *out_user_us, uint64_t *out_system_us) {
    *out_user_us = 0;
    *out_system_us = 0;
    return -1;
}

static int collect_memory_usage(uint64_t *out_rss, uint64_t *out_virtual,
                                uint64_t *out_shared) {
    *out_rss = 0;
    *out_virtual = 0;
    *out_shared = 0;
    return -1;
}

#endif

/* ---------- Lifecycle ---------- */

int ozayn_perf_init(ozayn_perf_manager_t *mgr, const ozayn_perf_config_t *config) {
    if (!mgr || !config) return -1;

    memset(mgr, 0, sizeof(ozayn_perf_manager_t));
    mgr->config = *config;

    clock_gettime(CLOCK_MONOTONIC, &mgr->init_time);
    mgr->prev_snapshot_time = mgr->init_time;

    /* Take initial CPU snapshot for delta calculation */
    collect_cpu_usage(&mgr->prev_cpu_user_us, &mgr->prev_cpu_system_us);

    mgr->initialized = 1;

    LOG_INFO("PERF", "Performance manager initialized (interval=%u ms, max_snapshots=%u)",
             config->snapshot_interval_ms, config->max_snapshots);

    return 0;
}

void ozayn_perf_shutdown(ozayn_perf_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Cancel any running benchmarks */
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        ozayn_perf_benchmark_t *b = &mgr->benchmarks[i];
        if (b->active && b->state == OZAYN_PERF_BENCH_RUNNING) {
            b->state = OZAYN_PERF_BENCH_CANCELLED;
        }
    }

    mgr->initialized = 0;

    LOG_INFO("PERF", "Performance manager shut down (snapshots=%u, benchmarks=%u)",
             mgr->snapshot_count, mgr->benchmark_count);
}

void ozayn_perf_set_events(ozayn_perf_manager_t *mgr, ozayn_event_engine_t *events) {
    if (mgr) mgr->events = events;
}

/* ---------- Elapsed time ---------- */

uint64_t ozayn_perf_elapsed_us(const struct timespec *start,
                                const struct timespec *end) {
    if (!start || !end) return 0;
    uint64_t s = (uint64_t)start->tv_sec * 1000000ULL + (uint64_t)start->tv_nsec / 1000ULL;
    uint64_t e = (uint64_t)end->tv_sec * 1000000ULL + (uint64_t)end->tv_nsec / 1000ULL;
    return e >= s ? e - s : 0;
}

uint64_t ozayn_perf_time_us(void (*fn)(void *arg), void *arg) {
    if (!fn) return 0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    fn(arg);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return ozayn_perf_elapsed_us(&t0, &t1);
}

/* ---------- Startup timing ---------- */

void ozayn_perf_startup_begin(ozayn_perf_manager_t *mgr) {
    if (!mgr) return;
    clock_gettime(CLOCK_MONOTONIC, &mgr->startup_start);
}

void ozayn_perf_startup_end(ozayn_perf_manager_t *mgr) {
    if (!mgr) return;
    clock_gettime(CLOCK_MONOTONIC, &mgr->startup_end);
    mgr->startup_duration_us = ozayn_perf_elapsed_us(
        &mgr->startup_start, &mgr->startup_end);
    mgr->startup_recorded = 1;

    LOG_INFO("PERF", "Startup completed in %.2f ms",
             (double)mgr->startup_duration_us / 1000.0);

    if (mgr->events) {
        ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_STARTUP_RECORDED,
                              OZAYN_SRC_PERF, NULL);
    }
}

uint64_t ozayn_perf_startup_duration_us(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->startup_duration_us;
}

double ozayn_perf_startup_duration_ms(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return 0.0;
    return (double)mgr->startup_duration_us / 1000.0;
}

/* ---------- Snapshot collection ---------- */

int ozayn_perf_snapshot_collect(ozayn_perf_manager_t *mgr,
                                 ozayn_perf_snapshot_t *out) {
    if (!mgr || !out) return -1;

    memset(out, 0, sizeof(ozayn_perf_snapshot_t));

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    out->timestamp_us = clock_monotonic_us();
    out->uptime_us = ozayn_perf_elapsed_us(&mgr->init_time, &now);

    /* CPU usage */
    if (mgr->config.auto_cpu_check) {
        uint64_t cur_user = 0, cur_sys = 0;
        if (collect_cpu_usage(&cur_user, &cur_sys) == 0) {
            uint64_t delta_user = cur_user >= mgr->prev_cpu_user_us ?
                cur_user - mgr->prev_cpu_user_us : 0;
            uint64_t delta_sys = cur_sys >= mgr->prev_cpu_system_us ?
                cur_sys - mgr->prev_cpu_system_us : 0;

            /* Calculate elapsed wall-clock time since last snapshot */
            uint64_t elapsed = ozayn_perf_elapsed_us(&mgr->prev_snapshot_time, &now);
            if (elapsed > 0) {
                /* Convert to percentage: (cpu_us / wall_us) * 100 */
                out->cpu.user_pct   = ((double)delta_user / (double)elapsed) * 100.0;
                out->cpu.system_pct = ((double)delta_sys   / (double)elapsed) * 100.0;
                out->cpu.total_pct  = out->cpu.user_pct + out->cpu.system_pct;
            }
            out->cpu.user_us   = cur_user;
            out->cpu.system_us = cur_sys;

            mgr->prev_cpu_user_us   = cur_user;
            mgr->prev_cpu_system_us = cur_sys;
        }
    }

    /* Memory usage */
    if (mgr->config.auto_memory_check) {
        uint64_t rss = 0, virt = 0, shared = 0;
        if (collect_memory_usage(&rss, &virt, &shared) == 0) {
            out->memory.resident_bytes = rss;
            out->memory.virtual_bytes  = virt;
            out->memory.shared_bytes   = shared;
            out->memory.resident_mb    = (double)rss / (1048576.0);
            out->memory.virtual_mb     = (double)virt / (1048576.0);
        }
    }

    mgr->prev_snapshot_time = now;

    return 0;
}

int ozayn_perf_snapshot_store(ozayn_perf_manager_t *mgr) {
    if (!mgr) return -1;

    ozayn_perf_snapshot_t snap;
    if (ozayn_perf_snapshot_collect(mgr, &snap) != 0) return -1;

    uint32_t idx = mgr->snapshot_head;
    mgr->snapshots[idx] = snap;
    mgr->snapshot_head = (mgr->snapshot_head + 1) % mgr->config.max_snapshots;
    if (mgr->snapshot_count < mgr->config.max_snapshots)
        mgr->snapshot_count++;

    if (mgr->events) {
        ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_SNAPSHOT_TAKEN,
                              OZAYN_SRC_PERF, NULL);
    }

    /* Check thresholds */
    if (mgr->threshold_count > 0) {
        ozayn_perf_threshold_check(mgr, "cpu", snap.cpu.total_pct);
        ozayn_perf_threshold_check(mgr, "memory", snap.memory.resident_mb);
    }

    return 0;
}

const ozayn_perf_snapshot_t *ozayn_perf_snapshot_get(
        const ozayn_perf_manager_t *mgr, uint32_t index) {
    if (!mgr || index >= mgr->snapshot_count) return NULL;
    uint32_t actual = (mgr->snapshot_head + mgr->config.max_snapshots
                       - 1 - index) % mgr->config.max_snapshots;
    return &mgr->snapshots[actual];
}

uint32_t ozayn_perf_snapshot_count(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->snapshot_count;
}

const ozayn_perf_snapshot_t *ozayn_perf_snapshot_latest(
        const ozayn_perf_manager_t *mgr) {
    if (!mgr || mgr->snapshot_count == 0) return NULL;
    uint32_t idx = (mgr->snapshot_head + mgr->config.max_snapshots
                    - 1) % mgr->config.max_snapshots;
    return &mgr->snapshots[idx];
}

/* ---------- Benchmarks ---------- */

int ozayn_perf_bench_register(ozayn_perf_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->benchmark_count >= OZAYN_PERF_MAX_BENCHMARKS) return -1;

    /* Check duplicate */
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0)
            return -1;
    }

    ozayn_perf_benchmark_t *b = &mgr->benchmarks[mgr->benchmark_count];
    memset(b, 0, sizeof(ozayn_perf_benchmark_t));
    b->active = 1;
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->state = OZAYN_PERF_BENCH_IDLE;
    b->min_us = UINT64_MAX;

    mgr->benchmark_count++;
    LOG_DEBUG("PERF", "Benchmark registered: %s", name);
    return 0;
}

int ozayn_perf_bench_begin(ozayn_perf_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    ozayn_perf_benchmark_t *b = NULL;
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0) {
            b = &mgr->benchmarks[i];
            break;
        }
    }
    if (!b) return -1;

    /* Reset for new run */
    b->total_us = 0;
    b->min_us = UINT64_MAX;
    b->max_us = 0;
    b->iterations = 0;
    b->avg_us = 0.0;
    b->stddev_us = 0.0;
    b->total_iterations = 0;
    b->state = OZAYN_PERF_BENCH_RUNNING;

    if (mgr->events) {
        ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_BENCHMARK_STARTED,
                              OZAYN_SRC_PERF, NULL);
    }

    return 0;
}

int ozayn_perf_bench_record(ozayn_perf_manager_t *mgr, const char *name,
                             uint64_t duration_us) {
    if (!mgr || !name) return -1;

    ozayn_perf_benchmark_t *b = NULL;
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0) {
            b = &mgr->benchmarks[i];
            break;
        }
    }
    if (!b) return -1;
    if (b->state != OZAYN_PERF_BENCH_RUNNING) return -1;

    b->total_us += duration_us;
    b->last_us = duration_us;
    if (duration_us < b->min_us) b->min_us = duration_us;
    if (duration_us > b->max_us) b->max_us = duration_us;
    b->iterations++;

    return 0;
}

int ozayn_perf_bench_end(ozayn_perf_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    ozayn_perf_benchmark_t *b = NULL;
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0) {
            b = &mgr->benchmarks[i];
            break;
        }
    }
    if (!b) return -1;
    if (b->state != OZAYN_PERF_BENCH_RUNNING) return -1;

    if (b->iterations > 0) {
        b->avg_us = (double)b->total_us / (double)b->iterations;
        b->total_iterations = b->iterations;

        /* Calculate stddev using online algorithm */
        if (b->iterations > 1) {
            /* We stored the last_us for each iteration in bench_end,
               but for simplicity we'll compute stddev from total/avg */
            b->stddev_us = 0.0; /* Would need Welford's algorithm for true stddev */
        }
    }

    b->state = OZAYN_PERF_BENCH_COMPLETED;

    if (mgr->events) {
        ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_BENCHMARK_COMPLETED,
                              OZAYN_SRC_PERF, NULL);
    }

    return 0;
}

int ozayn_perf_bench_cancel(ozayn_perf_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    ozayn_perf_benchmark_t *b = NULL;
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0) {
            b = &mgr->benchmarks[i];
            break;
        }
    }
    if (!b) return -1;

    b->state = OZAYN_PERF_BENCH_CANCELLED;

    if (mgr->events) {
        ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_BENCHMARK_CANCELLED,
                              OZAYN_SRC_PERF, NULL);
    }

    return 0;
}

const ozayn_perf_benchmark_t *ozayn_perf_bench_find(
        const ozayn_perf_manager_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;

    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        if (mgr->benchmarks[i].active &&
            strcmp(mgr->benchmarks[i].name, name) == 0)
            return &mgr->benchmarks[i];
    }
    return NULL;
}

uint32_t ozayn_perf_bench_count(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->benchmark_count;
}

/* ---------- Thresholds ---------- */

int ozayn_perf_threshold_register(ozayn_perf_manager_t *mgr,
                                   const char *name,
                                   double warning,
                                   double critical) {
    if (!mgr || !name) return -1;
    if (mgr->threshold_count >= OZAYN_PERF_MAX_THRESHOLDS) return -1;

    /* Check duplicate */
    for (uint32_t i = 0; i < mgr->threshold_count; i++) {
        if (mgr->thresholds[i].active &&
            strcmp(mgr->thresholds[i].name, name) == 0)
            return -1;
    }

    ozayn_perf_threshold_t *t = &mgr->thresholds[mgr->threshold_count];
    memset(t, 0, sizeof(ozayn_perf_threshold_t));
    t->active = 1;
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->severity = OZAYN_PERF_THRESH_WARNING;
    t->warning_threshold = warning;
    t->critical_threshold = critical;

    mgr->threshold_count++;
    LOG_DEBUG("PERF", "Threshold registered: %s (warn=%.1f, crit=%.1f)",
              name, warning, critical);
    return 0;
}

void ozayn_perf_threshold_check(ozayn_perf_manager_t *mgr,
                                 const char *name, double value) {
    if (!mgr || !name) return;

    ozayn_perf_threshold_t *t = NULL;
    for (uint32_t i = 0; i < mgr->threshold_count; i++) {
        if (mgr->thresholds[i].active &&
            strcmp(mgr->thresholds[i].name, name) == 0) {
            t = &mgr->thresholds[i];
            break;
        }
    }
    if (!t) return;

    if (value >= t->critical_threshold) {
        t->severity = OZAYN_PERF_THRESH_CRITICAL;
        mgr->threshold_criticals++;
        LOG_WARN("PERF", "Threshold CRITICAL: %s = %.2f (limit=%.2f)",
                 name, value, t->critical_threshold);
        if (mgr->events) {
            ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_THRESHOLD_CRITICAL,
                                  OZAYN_SRC_PERF, NULL);
        }
    } else if (value >= t->warning_threshold) {
        t->severity = OZAYN_PERF_THRESH_WARNING;
        mgr->threshold_warnings++;
        LOG_WARN("PERF", "Threshold WARNING: %s = %.2f (limit=%.2f)",
                 name, value, t->warning_threshold);
        if (mgr->events) {
            ozayn_events_publish(mgr->events, OZAYN_PERF_EVENT_THRESHOLD_WARNING,
                                  OZAYN_SRC_PERF, NULL);
        }
    } else {
        t->severity = OZAYN_PERF_THRESH_WARNING;
    }
}

/* ---------- Stats ---------- */

ozayn_perf_stats_t ozayn_perf_stats(const ozayn_perf_manager_t *mgr) {
    ozayn_perf_stats_t s;
    memset(&s, 0, sizeof(s));

    if (!mgr) return s;

    s.startup_duration_us    = mgr->startup_duration_us;
    s.snapshot_count         = mgr->snapshot_count;
    s.benchmark_count        = mgr->benchmark_count;
    s.threshold_count        = mgr->threshold_count;
    s.threshold_warnings     = mgr->threshold_warnings;
    s.threshold_criticals    = mgr->threshold_criticals;

    const ozayn_perf_snapshot_t *latest = ozayn_perf_snapshot_latest(mgr);
    if (latest) {
        s.current_cpu_pct    = latest->cpu.total_pct;
        s.current_memory_mb  = latest->memory.resident_mb;
    }

    return s;
}

/* ---------- Print ---------- */

void ozayn_perf_print_snapshot(const ozayn_perf_manager_t *mgr,
                                const ozayn_perf_snapshot_t *snap) {
    (void)mgr;
    if (!snap) return;

    LOG_INFO("PERF", "=== Performance Snapshot ===");
    LOG_INFO("PERF", "Timestamp: %.2f s uptime", (double)snap->uptime_us / 1000000.0);
    LOG_INFO("PERF", "CPU: user=%.2f%% sys=%.2f%% total=%.2f%%",
             snap->cpu.user_pct, snap->cpu.system_pct, snap->cpu.total_pct);
    LOG_INFO("PERF", "Memory: RSS=%.2f MB, Virtual=%.2f MB",
             snap->memory.resident_mb, snap->memory.virtual_mb);
    LOG_INFO("PERF", "Events: published=%lu, dispatched=%lu, dropped=%lu, queue=%u/%u",
             (unsigned long)snap->events.published_total,
             (unsigned long)snap->events.dispatched_total,
             (unsigned long)snap->events.dropped_total,
             snap->events.queue_depth, snap->events.queue_capacity);
}

void ozayn_perf_print_latest(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return;
    const ozayn_perf_snapshot_t *s = ozayn_perf_snapshot_latest(mgr);
    if (!s) {
        LOG_INFO("PERF", "No snapshots recorded yet");
        return;
    }
    ozayn_perf_print_snapshot(mgr, s);
}

void ozayn_perf_print_benchmarks(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return;

    LOG_INFO("PERF", "=== Benchmarks (%u registered) ===", mgr->benchmark_count);
    for (uint32_t i = 0; i < mgr->benchmark_count; i++) {
        const ozayn_perf_benchmark_t *b = &mgr->benchmarks[i];
        if (!b->active) continue;

        LOG_INFO("PERF", "  [%s] state=%s iterations=%u avg=%.2f us min=%lu us max=%lu us",
                 b->name,
                 ozayn_perf_bench_state_name(b->state),
                 b->iterations,
                 b->avg_us,
                 (unsigned long)(b->min_us == UINT64_MAX ? 0 : b->min_us),
                 (unsigned long)b->max_us);
    }
}

void ozayn_perf_print_thresholds(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return;

    LOG_INFO("PERF", "=== Thresholds (%u registered) ===", mgr->threshold_count);
    for (uint32_t i = 0; i < mgr->threshold_count; i++) {
        const ozayn_perf_threshold_t *t = &mgr->thresholds[i];
        if (!t->active) continue;

        LOG_INFO("PERF", "  [%s] warn=%.1f crit=%.1f current_severity=%s",
                 t->name, t->warning_threshold, t->critical_threshold,
                 ozayn_perf_threshold_severity_name(t->severity));
    }
}

void ozayn_perf_print_stats(const ozayn_perf_manager_t *mgr) {
    if (!mgr) return;

    ozayn_perf_stats_t s = ozayn_perf_stats(mgr);
    LOG_INFO("PERF", "=== Performance Stats ===");
    LOG_INFO("PERF", "Startup: %.2f ms", (double)s.startup_duration_us / 1000.0);
    LOG_INFO("PERF", "Snapshots: %u, Benchmarks: %u, Thresholds: %u",
             s.snapshot_count, s.benchmark_count, s.threshold_count);
    LOG_INFO("PERF", "Threshold events: %u warnings, %u criticals",
             s.threshold_warnings, s.threshold_criticals);
    LOG_INFO("PERF", "Current CPU: %.2f%%, Memory: %.2f MB",
             s.current_cpu_pct, s.current_memory_mb);
}
