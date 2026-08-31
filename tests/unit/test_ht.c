#include "../test_framework.h"
#include "health_tracker.h"

TEST(ht_init_returns_ok) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = {
        .check_interval_ms = 1000,
        .auto_degrade = 1,
        .auto_fail = 1,
        .max_misses = 3,
    };
    int r = ozayn_ht_init(&ht, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(ht.initialized);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_register_and_get) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_CRITICAL, 1000, 5000);
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_STARTING);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_set_state) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_CRITICAL, 1000, 5000);
    ozayn_ht_set_state(&ht, "Comp", OZAYN_HT_DEGRADED);
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_DEGRADED);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_heartbeat) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_IMPORTANT, 1000, 5000);
    /* Set to HEALTHY before heartbeat */
    ozayn_ht_set_state(&ht, "Comp", OZAYN_HT_HEALTHY);
    ozayn_ht_heartbeat(&ht, "Comp");
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_HEALTHY);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_quarantine) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_OPTIONAL, 1000, 5000);
    ozayn_ht_quarantine(&ht, "Comp");
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_QUARANTINED);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_unquarantine) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_OPTIONAL, 1000, 5000);
    ozayn_ht_quarantine(&ht, "Comp");
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_QUARANTINED);
    ozayn_ht_unquarantine(&ht, "Comp");
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_RECOVERING);
    ozayn_ht_shutdown(&ht);
    return 0;
}

TEST(ht_state_names) {
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_HEALTHY), "HEALTHY");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_DEGRADED), "DEGRADED");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_FAILED), "FAILED");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_STARTING), "STARTING");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_STOPPING), "STOPPING");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_STOPPED), "STOPPED");
    ASSERT_STR_EQ(ozayn_ht_state_name(OZAYN_HT_QUARANTINED), "QUARANTINED");
    return 0;
}

TEST(ht_cannot_heartbeat_quarantined) {
    ozayn_health_tracker_t ht;
    ozayn_ht_watchdog_config_t cfg = { 1000, 1, 1, 3 };
    ozayn_ht_init(&ht, &cfg);
    ozayn_ht_register(&ht, "Comp", OZAYN_HT_OPTIONAL, 1000, 5000);
    ozayn_ht_quarantine(&ht, "Comp");
    /* Heartbeat on quarantined component should not change state */
    ozayn_ht_heartbeat(&ht, "Comp");
    ASSERT_EQ((int)ozayn_ht_get_state(&ht, "Comp"), OZAYN_HT_QUARANTINED);
    ozayn_ht_shutdown(&ht);
    return 0;
}

int run_ht_tests(void) {
    SUITE_BEGIN("Health Tracker");
    RUN(ht_init_returns_ok);
    RUN(ht_register_and_get);
    RUN(ht_set_state);
    RUN(ht_heartbeat);
    RUN(ht_quarantine);
    RUN(ht_unquarantine);
    RUN(ht_state_names);
    RUN(ht_cannot_heartbeat_quarantined);
    SUITE_END();
    return _tf_suite_fail;
}
