#include "../test_framework.h"
#include "circuit_breaker.h"

TEST(cb_init_returns_ok) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = {
        .failure_threshold = 5,
        .success_threshold = 3,
        .timeout_ms = 10000,
        .max_calls_half_open = 2,
    };
    int r = ozayn_cb_init(&cb, "TestCB", &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(cb.initialized);
    ASSERT_EQ((int)ozayn_cb_get_state(&cb), OZAYN_CB_CLOSED);
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_allow_call_closed) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 5, 3, 10000, 2 };
    ozayn_cb_init(&cb, "CB", &cfg);
    ASSERT(ozayn_cb_allow_call(&cb));
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_trips_on_failures) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 3, 2, 10000, 1 };
    ozayn_cb_init(&cb, "CB", &cfg);
    for (int i = 0; i < 3; i++) ozayn_cb_record_failure(&cb);
    ASSERT_EQ((int)ozayn_cb_get_state(&cb), OZAYN_CB_OPEN);
    ASSERT(!ozayn_cb_allow_call(&cb));
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_reset_to_closed) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 3, 2, 10000, 1 };
    ozayn_cb_init(&cb, "CB", &cfg);
    for (int i = 0; i < 3; i++) ozayn_cb_record_failure(&cb);
    ozayn_cb_reset(&cb);
    ASSERT_EQ((int)ozayn_cb_get_state(&cb), OZAYN_CB_CLOSED);
    ASSERT(ozayn_cb_allow_call(&cb));
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_success_clears_failures) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 5, 2, 10000, 1 };
    ozayn_cb_init(&cb, "CB", &cfg);
    ozayn_cb_record_failure(&cb);
    ozayn_cb_record_failure(&cb);
    ozayn_cb_record_success(&cb);
    ASSERT_EQ(cb.failure_count, 0u);
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_stats_tracking) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 5, 2, 10000, 1 };
    ozayn_cb_init(&cb, "CB", &cfg);
    ozayn_cb_record_failure(&cb);
    ozayn_cb_record_success(&cb);
    ozayn_cb_stats_t s = ozayn_cb_stats(&cb);
    ASSERT_EQ(s.total_failures, 1u);
    ASSERT_EQ(s.total_calls, 2u);
    ozayn_cb_shutdown(&cb);
    return 0;
}

TEST(cb_state_names) {
    ASSERT_STR_EQ(ozayn_cb_state_name(OZAYN_CB_CLOSED), "CLOSED");
    ASSERT_STR_EQ(ozayn_cb_state_name(OZAYN_CB_OPEN), "OPEN");
    ASSERT_STR_EQ(ozayn_cb_state_name(OZAYN_CB_HALF_OPEN), "HALF_OPEN");
    return 0;
}

TEST(cb_is_open_check) {
    ozayn_circuit_breaker_t cb;
    ozayn_cb_config_t cfg = { 3, 2, 10000, 1 };
    ozayn_cb_init(&cb, "CB", &cfg);
    ASSERT(!ozayn_cb_is_open(&cb));
    for (int i = 0; i < 3; i++) ozayn_cb_record_failure(&cb);
    ASSERT(ozayn_cb_is_open(&cb));
    ozayn_cb_shutdown(&cb);
    return 0;
}

int run_cb_tests(void) {
    SUITE_BEGIN("Circuit Breaker");
    RUN(cb_init_returns_ok);
    RUN(cb_allow_call_closed);
    RUN(cb_trips_on_failures);
    RUN(cb_reset_to_closed);
    RUN(cb_success_clears_failures);
    RUN(cb_stats_tracking);
    RUN(cb_state_names);
    RUN(cb_is_open_check);
    SUITE_END();
    return _tf_suite_fail;
}
