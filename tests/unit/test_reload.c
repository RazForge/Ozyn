#include "../test_framework.h"
#include "reload_mgr.h"
#include <string.h>

/*
 * test_reload.c — Unit tests for Reload Manager (Stage 26).
 *
 * Tests: init, registration, reload capability, state machine, quiesce, audit.
 */

TEST(reload_init_returns_ok) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = {
        .quiesce_timeout_ms = 5000,
        .load_timeout_ms = 10000,
        .health_check_timeout_ms = 3000,
        .rollback_on_fail = 1,
        .max_concurrent = 1,
    };
    int r = ozayn_reload_mgr_init(&mgr, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(!ozayn_reload_is_busy(&mgr));
    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_register_component) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    int r = ozayn_reload_register(&mgr, "TestComp", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(mgr.component_count, 1);

    const ozayn_reload_component_t *comp = ozayn_reload_find(&mgr, "TestComp");
    ASSERT_NOT_NULL(comp);
    ASSERT_STR_EQ(comp->current_version, "1.0.0");
    ASSERT_EQ(comp->capability, OZAYN_RELOAD_SUPPORTED);

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_is_reloadable_query) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Hot", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_register(&mgr, "Cold", "1.0.0", OZAYN_RELOAD_UNSUPPORTED, 1);
    ozayn_reload_register(&mgr, "Restart", "1.0.0", OZAYN_RELOAD_RESTART_REQUIRED, 1);

    ASSERT(ozayn_reload_is_reloadable(&mgr, "Hot"));
    ASSERT(!ozayn_reload_is_reloadable(&mgr, "Cold"));
    ASSERT(!ozayn_reload_is_reloadable(&mgr, "Restart"));

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_unsupported_rejected) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Core", "0.1.0", OZAYN_RELOAD_UNSUPPORTED, 1);

    int r = ozayn_reload_request(&mgr, "Core", "0.2.0", "admin", "test");
    ASSERT_NEQ(r, 0);

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_supported_completes) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "CompA", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_request(&mgr, "CompA", "2.0.0", "admin", "upgrade");

    /* Tick through the state machine */
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    ASSERT(!ozayn_reload_is_busy(&mgr));
    const ozayn_reload_component_t *comp = ozayn_reload_find(&mgr, "CompA");
    ASSERT_STR_EQ(comp->current_version, "2.0.0");

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_active_work_blocks_quiesce) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Svc", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_request_begin(&mgr, "Svc");
    ozayn_reload_request_begin(&mgr, "Svc");

    ASSERT_EQ(ozayn_reload_active_requests(&mgr, "Svc"), 2);

    ozayn_reload_request(&mgr, "Svc", "2.0.0", "admin", "upgrade");
    /* Tick — should stall at quiesce */
    for (int i = 0; i < 3; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }
    ASSERT_EQ(ozayn_reload_get_state(&mgr, "Svc"), OZAYN_RELOAD_STATE_QUIESCING);

    /* Drain the work */
    ozayn_reload_request_end(&mgr, "Svc");
    ozayn_reload_request_end(&mgr, "Svc");

    /* Now it should complete */
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }
    ASSERT(!ozayn_reload_is_busy(&mgr));
    const ozayn_reload_component_t *comp = ozayn_reload_find(&mgr, "Svc");
    ASSERT_STR_EQ(comp->current_version, "2.0.0");

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_state_save_load) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Persistent", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);

    const char *state = "{\"key\":\"value\"}";
    int r = ozayn_reload_save_state(&mgr, "Persistent", state, (uint32_t)strlen(state) + 1, 1);
    ASSERT_EQ(r, 0);

    char loaded[256];
    uint32_t ver = 0;
    r = ozayn_reload_load_state(&mgr, "Persistent", loaded, sizeof(loaded), &ver);
    ASSERT_EQ(r, 0);
    ASSERT_STR_EQ(loaded, state);
    ASSERT_EQ(ver, 1);

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_audit_trail) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Audited", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_request(&mgr, "Audited", "2.0.0", "admin", "test");
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    ASSERT_GT(ozayn_reload_audit_count(&mgr), 0);

    const ozayn_reload_audit_t *entry = ozayn_reload_get_audit(&mgr, 0);
    ASSERT_NOT_NULL(entry);
    ASSERT_STR_EQ(entry->component, "Audited");

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_stats_tracking) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "A", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);
    ozayn_reload_register(&mgr, "B", "1.0.0", OZAYN_RELOAD_UNSUPPORTED, 1);

    /* Successful reload */
    ozayn_reload_request(&mgr, "A", "2.0.0", "admin", "test");
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    ozayn_reload_stats_t stats = ozayn_reload_stats(&mgr);
    ASSERT_EQ(stats.reloadable_count, 1);
    ASSERT_EQ(stats.non_reloadable_count, 1);
    ASSERT_EQ(stats.total_succeeded, 1);

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_state_names) {
    ASSERT_STR_EQ(ozayn_reload_state_name(OZAYN_RELOAD_STATE_IDLE), "IDLE");
    ASSERT_STR_EQ(ozayn_reload_state_name(OZAYN_RELOAD_STATE_COMPLETED), "COMPLETED");
    ASSERT_STR_EQ(ozayn_reload_result_name(OZAYN_RELOAD_RESULT_OK), "OK");
    ASSERT_STR_EQ(ozayn_reload_result_name(OZAYN_RELOAD_RESULT_NOT_RELOADABLE), "NOT_RELOADABLE");
    ASSERT_STR_EQ(ozayn_reload_capability_name(OZAYN_RELOAD_SUPPORTED), "SUPPORTED");
    ASSERT_STR_EQ(ozayn_reload_capability_name(OZAYN_RELOAD_UNSUPPORTED), "UNSUPPORTED");
    return 0;
}

int run_reload_tests(void) {
    SUITE_BEGIN("Reload Manager");
    RUN(reload_init_returns_ok);
    RUN(reload_register_component);
    RUN(reload_is_reloadable_query);
    RUN(reload_unsupported_rejected);
    RUN(reload_supported_completes);
    RUN(reload_active_work_blocks_quiesce);
    RUN(reload_state_save_load);
    RUN(reload_audit_trail);
    RUN(reload_stats_tracking);
    RUN(reload_state_names);
    SUITE_END();
    return TOTAL_FAIL();
}
