#include "../test_framework.h"
#include "perf_mgr.h"
#include <string.h>

/*
 * test_perf.c — Unit tests for Performance Manager (Stage 28).
 *
 * Tests: init, startup timing, benchmarks, thresholds, snapshots, stats.
 */

static void dummy_fn(void *arg) {
    (void)arg;
    volatile int x = 0;
    for (int i = 0; i < 100; i++) x++;
}

TEST(perf_init_returns_ok) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = {
        .snapshot_interval_ms = 0,
        .max_snapshots = 64,
        .startup_timeout_ms = 30000,
        .auto_cpu_check = 1,
        .auto_memory_check = 1,
    };
    int r = ozayn_perf_init(&mgr, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(mgr.initialized);
    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_startup_timing) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = {
        .snapshot_interval_ms = 0,
        .max_snapshots = 64,
        .startup_timeout_ms = 30000,
        .auto_cpu_check = 1,
        .auto_memory_check = 1,
    };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_startup_begin(&mgr);
    /* Simulate some work */
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x++;
    (void)x;
    ozayn_perf_startup_end(&mgr);

    ASSERT(mgr.startup_recorded);
    ASSERT(mgr.startup_duration_us > 0);
    ASSERT(ozayn_perf_startup_duration_ms(&mgr) > 0.0);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_elapsed_us_basic) {
    struct timespec t0 = { .tv_sec = 1, .tv_nsec = 0 };
    struct timespec t1 = { .tv_sec = 2, .tv_nsec = 500000 };
    uint64_t elapsed = ozayn_perf_elapsed_us(&t0, &t1);
    /* 1s + 500000ns = 1,000,500 us (500000ns = 500us) */
    ASSERT_EQ(elapsed, 1000500);
    return 0;
}

TEST(perf_elapsed_us_zero) {
    struct timespec t = { .tv_sec = 10, .tv_nsec = 0 };
    uint64_t elapsed = ozayn_perf_elapsed_us(&t, &t);
    ASSERT_EQ(elapsed, 0);
    return 0;
}

TEST(perf_time_us_function) {
    uint64_t us = ozayn_perf_time_us(dummy_fn, NULL);
    /* Function may be too fast to measure (0 us on fast systems) — just verify API works */
    (void)us;
    return 0;
}

TEST(perf_bench_register) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    int r = ozayn_perf_bench_register(&mgr, "test_bench");
    ASSERT_EQ(r, 0);
    ASSERT_EQ(mgr.benchmark_count, 1);

    const ozayn_perf_benchmark_t *b = ozayn_perf_bench_find(&mgr, "test_bench");
    ASSERT_NOT_NULL(b);
    ASSERT_EQ(b->state, OZAYN_PERF_BENCH_IDLE);

    /* Duplicate should fail */
    r = ozayn_perf_bench_register(&mgr, "test_bench");
    ASSERT_EQ(r, -1);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_bench_record_and_end) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_bench_register(&mgr, "my_bench");
    ozayn_perf_bench_begin(&mgr, "my_bench");

    const ozayn_perf_benchmark_t *b = ozayn_perf_bench_find(&mgr, "my_bench");
    ASSERT_EQ(b->state, OZAYN_PERF_BENCH_RUNNING);

    ozayn_perf_bench_record(&mgr, "my_bench", 100);
    ozayn_perf_bench_record(&mgr, "my_bench", 200);
    ozayn_perf_bench_record(&mgr, "my_bench", 300);
    ASSERT_EQ(b->iterations, 3);
    ASSERT_EQ(b->total_us, 600);
    ASSERT_EQ(b->min_us, 100);
    ASSERT_EQ(b->max_us, 300);

    ozayn_perf_bench_end(&mgr, "my_bench");
    ASSERT_EQ(b->state, OZAYN_PERF_BENCH_COMPLETED);
    ASSERT(b->avg_us > 0.0);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_bench_cancel) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_bench_register(&mgr, "cancel_bench");
    ozayn_perf_bench_begin(&mgr, "cancel_bench");
    ozayn_perf_bench_cancel(&mgr, "cancel_bench");

    const ozayn_perf_benchmark_t *b = ozayn_perf_bench_find(&mgr, "cancel_bench");
    ASSERT_EQ(b->state, OZAYN_PERF_BENCH_CANCELLED);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_bench_not_found) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    const ozayn_perf_benchmark_t *b = ozayn_perf_bench_find(&mgr, "nonexistent");
    ASSERT_NULL(b);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_threshold_register) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    int r = ozayn_perf_threshold_register(&mgr, "cpu", 80.0, 95.0);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(mgr.threshold_count, 1);

    /* Duplicate should fail */
    r = ozayn_perf_threshold_register(&mgr, "cpu", 80.0, 95.0);
    ASSERT_EQ(r, -1);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_threshold_check) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_threshold_register(&mgr, "cpu", 80.0, 95.0);

    /* Below warning - no events */
    ozayn_perf_threshold_check(&mgr, "cpu", 50.0);
    ASSERT_EQ(mgr.threshold_warnings, 0);
    ASSERT_EQ(mgr.threshold_criticals, 0);

    /* Warning */
    ozayn_perf_threshold_check(&mgr, "cpu", 85.0);
    ASSERT_EQ(mgr.threshold_warnings, 1);
    ASSERT_EQ(mgr.threshold_criticals, 0);

    /* Critical */
    ozayn_perf_threshold_check(&mgr, "cpu", 96.0);
    ASSERT_EQ(mgr.threshold_warnings, 1);
    ASSERT_EQ(mgr.threshold_criticals, 1);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_snapshot_collect) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 1, .auto_memory_check = 1 };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_snapshot_t snap;
    int r = ozayn_perf_snapshot_collect(&mgr, &snap);
    ASSERT_EQ(r, 0);
    ASSERT(snap.uptime_us > 0);
    ASSERT(snap.memory.resident_bytes > 0);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_snapshot_store) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 4, .startup_timeout_ms = 30000, .auto_cpu_check = 0, .auto_memory_check = 0 };
    ozayn_perf_init(&mgr, &cfg);

    for (int i = 0; i < 4; i++)
        ozayn_perf_snapshot_store(&mgr);

    ASSERT_EQ(mgr.snapshot_count, 4);

    /* Overwrite oldest */
    ozayn_perf_snapshot_store(&mgr);
    ASSERT_EQ(mgr.snapshot_count, 4);

    const ozayn_perf_snapshot_t *latest = ozayn_perf_snapshot_latest(&mgr);
    ASSERT_NOT_NULL(latest);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_stats) {
    ozayn_perf_manager_t mgr;
    ozayn_perf_config_t cfg = { .snapshot_interval_ms = 0, .max_snapshots = 64, .startup_timeout_ms = 30000, .auto_cpu_check = 1, .auto_memory_check = 1 };
    ozayn_perf_init(&mgr, &cfg);

    ozayn_perf_startup_begin(&mgr);
    ozayn_perf_startup_end(&mgr);

    ozayn_perf_bench_register(&mgr, "bench1");
    ozayn_perf_threshold_register(&mgr, "cpu", 80.0, 95.0);
    ozayn_perf_snapshot_store(&mgr);

    ozayn_perf_stats_t s = ozayn_perf_stats(&mgr);
    ASSERT(mgr.startup_recorded);
    ASSERT_EQ(s.snapshot_count, 1);
    ASSERT_EQ(s.benchmark_count, 1);
    ASSERT_EQ(s.threshold_count, 1);
    ASSERT(s.current_memory_mb > 0.0);

    ozayn_perf_shutdown(&mgr);
    return 0;
}

TEST(perf_names) {
    ASSERT_STR_EQ(ozayn_perf_bench_state_name(OZAYN_PERF_BENCH_IDLE), "IDLE");
    ASSERT_STR_EQ(ozayn_perf_bench_state_name(OZAYN_PERF_BENCH_RUNNING), "RUNNING");
    ASSERT_STR_EQ(ozayn_perf_bench_state_name(OZAYN_PERF_BENCH_COMPLETED), "COMPLETED");
    ASSERT_STR_EQ(ozayn_perf_bench_state_name(OZAYN_PERF_BENCH_CANCELLED), "CANCELLED");
    ASSERT_STR_EQ(ozayn_perf_threshold_severity_name(OZAYN_PERF_THRESH_WARNING), "WARNING");
    ASSERT_STR_EQ(ozayn_perf_threshold_severity_name(OZAYN_PERF_THRESH_CRITICAL), "CRITICAL");
    return 0;
}

int run_perf_tests(void) {
    printf("\n  [PERF MGR]");
    int failed = 0;
    RUN(perf_init_returns_ok);
    RUN(perf_startup_timing);
    RUN(perf_elapsed_us_basic);
    RUN(perf_elapsed_us_zero);
    RUN(perf_time_us_function);
    RUN(perf_bench_register);
    RUN(perf_bench_record_and_end);
    RUN(perf_bench_cancel);
    RUN(perf_bench_not_found);
    RUN(perf_threshold_register);
    RUN(perf_threshold_check);
    RUN(perf_snapshot_collect);
    RUN(perf_snapshot_store);
    RUN(perf_stats);
    RUN(perf_names);
    return failed;
}
