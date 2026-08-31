#include "../test_framework.h"
#include "crash_loop.h"

TEST(cl_init_returns_ok) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = {
        .max_failures_in_window = 3,
        .window_duration_ms = 60000,
        .cooldown_ms = 300000,
        .max_restarts = 5,
    };
    int r = ozayn_cl_init(&cl, &policy);
    ASSERT_EQ(r, 0);
    ASSERT(cl.initialized);
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_register_component) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 3, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ASSERT_EQ(ozayn_cl_register(&cl, "Comp"), 0);
    ASSERT_EQ(cl.component_count, 1u);
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_record_failure) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 3, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    ASSERT_EQ(ozayn_cl_record_failure(&cl, "Comp"), 0);
    ASSERT_EQ(ozayn_cl_failure_count(&cl, "Comp"), 1u);
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_crash_loop_detection) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 3, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    for (int i = 0; i < 3; i++) ozayn_cl_record_failure(&cl, "Comp");
    ASSERT(ozayn_cl_is_quarantined(&cl, "Comp"));
    ASSERT(!ozayn_cl_can_restart(&cl, "Comp"));
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_record_success_releases) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 3, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    for (int i = 0; i < 2; i++) ozayn_cl_record_failure(&cl, "Comp");
    ozayn_cl_record_success(&cl, "Comp");
    ASSERT(!ozayn_cl_is_quarantined(&cl, "Comp"));
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_restart_limit) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 10, 60000, 300000, 2 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    /* Reach restart limit without triggering crash loop */
    for (int i = 0; i < 2; i++) {
        /* Each failure doesn't hit the window limit (10) but reaches max_restarts (2) */
        ozayn_cl_record_failure(&cl, "Comp");
        ozayn_cl_record_success(&cl, "Comp");
    }
    ASSERT_EQ(ozayn_cl_restart_count(&cl, "Comp"), 2u);
    /* Now one more failure should hit restart limit */
    ozayn_cl_record_failure(&cl, "Comp");
    ASSERT(ozayn_cl_is_quarantined(&cl, "Comp"));
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_manual_quarantine_release) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 10, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    ozayn_cl_quarantine(&cl, "Comp");
    ASSERT(ozayn_cl_is_quarantined(&cl, "Comp"));
    ozayn_cl_release(&cl, "Comp");
    ASSERT(!ozayn_cl_is_quarantined(&cl, "Comp"));
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_stats) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 2, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    for (int i = 0; i < 2; i++) ozayn_cl_record_failure(&cl, "Comp");
    ozayn_cl_stats_t s = ozayn_cl_stats(&cl);
    ASSERT_EQ(s.total_crash_loops, 1u);
    ASSERT_EQ(s.total_quarantines, 1u);
    ozayn_cl_shutdown(&cl);
    return 0;
}

TEST(cl_find_component) {
    ozayn_crash_loop_t cl;
    ozayn_cl_policy_t policy = { 3, 60000, 300000, 5 };
    ozayn_cl_init(&cl, &policy);
    ozayn_cl_register(&cl, "Comp");
    const ozayn_cl_component_t *found = ozayn_cl_find(&cl, "Comp");
    ASSERT_NOT_NULL(found);
    ASSERT_NULL(ozayn_cl_find(&cl, "Nonexistent"));
    ozayn_cl_shutdown(&cl);
    return 0;
}

int run_cl_tests(void) {
    SUITE_BEGIN("Crash Loop Detector");
    RUN(cl_init_returns_ok);
    RUN(cl_register_component);
    RUN(cl_record_failure);
    RUN(cl_crash_loop_detection);
    RUN(cl_record_success_releases);
    RUN(cl_restart_limit);
    RUN(cl_manual_quarantine_release);
    RUN(cl_stats);
    RUN(cl_find_component);
    SUITE_END();
    return _tf_suite_fail;
}
