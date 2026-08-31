#include "../test_framework.h"
#include "resource_limits.h"

TEST(rl_init_returns_ok) {
    ozayn_rl_mgr_t mgr;
    int r = ozayn_rl_init(&mgr);
    ASSERT_EQ(r, 0);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_register_counter) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    int r = ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 100);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(mgr.counter_count, 1u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_acquire_success) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 10);
    ASSERT_EQ(ozayn_rl_acquire(&mgr, "tasks", 5), 0);
    ASSERT_EQ(ozayn_rl_current(&mgr, "tasks"), 5u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_acquire_exceeds_limit) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 5);
    ASSERT_NEQ(ozayn_rl_acquire(&mgr, "tasks", 10), 0);
    ASSERT_EQ(ozayn_rl_current(&mgr, "tasks"), 0u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_release) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 10);
    ozayn_rl_acquire(&mgr, "tasks", 5);
    ozayn_rl_release(&mgr, "tasks", 3);
    ASSERT_EQ(ozayn_rl_current(&mgr, "tasks"), 2u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_pressure_levels) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 100);
    ASSERT_EQ((int)ozayn_rl_pressure(&mgr, "tasks"), OZAYN_RL_PRESSURE_NONE);
    ozayn_rl_acquire(&mgr, "tasks", 60);
    ASSERT_EQ((int)ozayn_rl_pressure(&mgr, "tasks"), OZAYN_RL_PRESSURE_MODERATE);
    ozayn_rl_acquire(&mgr, "tasks", 25);
    ASSERT_EQ((int)ozayn_rl_pressure(&mgr, "tasks"), OZAYN_RL_PRESSURE_HIGH);
    ozayn_rl_acquire(&mgr, "tasks", 14);
    ASSERT_EQ((int)ozayn_rl_pressure(&mgr, "tasks"), OZAYN_RL_PRESSURE_CRITICAL);
    ozayn_rl_acquire(&mgr, "tasks", 1);
    ASSERT_EQ((int)ozayn_rl_pressure(&mgr, "tasks"), OZAYN_RL_PRESSURE_EXCEEDED);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_can_acquire) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 10);
    ASSERT(ozayn_rl_can_acquire(&mgr, "tasks", 10));
    ASSERT(!ozayn_rl_can_acquire(&mgr, "tasks", 11));
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_limit_query) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 128);
    ASSERT_EQ(ozayn_rl_limit(&mgr, "tasks"), 128u);
    ASSERT_EQ(ozayn_rl_available(&mgr, "tasks"), 128u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_stats) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 10);
    ozayn_rl_acquire(&mgr, "tasks", 5);
    ozayn_rl_release(&mgr, "tasks", 2);
    ozayn_rl_stats_t s = ozayn_rl_stats(&mgr);
    ASSERT_EQ(s.counter_count, 1u);
    ASSERT_EQ(s.total_acquired, 5u);
    ASSERT_EQ(s.total_released, 2u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

TEST(rl_unregister) {
    ozayn_rl_mgr_t mgr;
    ozayn_rl_init(&mgr);
    ozayn_rl_register(&mgr, "tasks", OZAYN_RL_TASKS, 10);
    ASSERT_EQ(mgr.counter_count, 1u);
    ASSERT_EQ(ozayn_rl_unregister(&mgr, "tasks"), 0);
    ASSERT_EQ(ozayn_rl_current(&mgr, "tasks"), 0u);
    ozayn_rl_shutdown(&mgr);
    return 0;
}

int run_rl_tests(void) {
    SUITE_BEGIN("Resource Limits");
    RUN(rl_init_returns_ok);
    RUN(rl_register_counter);
    RUN(rl_acquire_success);
    RUN(rl_acquire_exceeds_limit);
    RUN(rl_release);
    RUN(rl_pressure_levels);
    RUN(rl_can_acquire);
    RUN(rl_limit_query);
    RUN(rl_stats);
    RUN(rl_unregister);
    SUITE_END();
    return _tf_suite_fail;
}
