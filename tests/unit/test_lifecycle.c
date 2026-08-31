#include "../test_framework.h"
#include "lifecycle.h"
#include <string.h>

/*
 * test_lifecycle.c — Unit tests for Lifecycle Coordinator (Stage 22).
 *
 * Tests: init, component registration, startup, shutdown, query.
 */

static int _init_called = 0;
static int _stop_called = 0;

static int test_init(void *ctx) { (void)ctx; _init_called = 1; return 0; }
static void test_stop(void *ctx) { (void)ctx; _stop_called = 1; }

TEST(lc_init_returns_ok) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = {
        .startup_timeout_ms  = 5000,
        .component_timeout_ms = 1000,
        .shutdown_timeout_ms = 5000,
    };
    int r = ozayn_lc_init(&lc, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(lc.initialized);
    ASSERT_EQ(ozayn_lc_get_state(&lc), OZAYN_LC_STATE_CREATED);
    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_register_component) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    int r = ozayn_lc_register_simple(&lc, "TestComp",
                                      OZAYN_LC_PHASE_FOUNDATION,
                                      OZAYN_LC_CRITICALITY_REQUIRED,
                                      test_init, NULL);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_lc_component_count(&lc), 1);

    const ozayn_lc_component_t *comp = ozayn_lc_get_component(&lc, "TestComp");
    ASSERT_NOT_NULL(comp);
    ASSERT_STR_EQ(comp->name, "TestComp");

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_register_multiple_components) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    ozayn_lc_register_simple(&lc, "C1", OZAYN_LC_PHASE_BOOTSTRAP, OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);
    ozayn_lc_register_simple(&lc, "C2", OZAYN_LC_PHASE_FOUNDATION, OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);
    ozayn_lc_register_simple(&lc, "C3", OZAYN_LC_PHASE_SECURITY, OZAYN_LC_CRITICALITY_OPTIONAL, test_init, NULL);

    ASSERT_EQ(ozayn_lc_component_count(&lc), 3);
    ASSERT_EQ(ozayn_lc_component_count_by_phase(&lc, OZAYN_LC_PHASE_BOOTSTRAP), 1);
    ASSERT_EQ(ozayn_lc_component_count_by_phase(&lc, OZAYN_LC_PHASE_FOUNDATION), 1);
    ASSERT_EQ(ozayn_lc_component_count_by_phase(&lc, OZAYN_LC_PHASE_SECURITY), 1);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_startup_transitions_to_running) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    _init_called = 0;

    ozayn_lc_register_simple(&lc, "CompA", OZAYN_LC_PHASE_BOOTSTRAP,
                              OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);

    int r = ozayn_lc_startup(&lc);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_lc_get_state(&lc), OZAYN_LC_STATE_RUNNING);
    ASSERT(_init_called);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_shutdown_transitions_to_stopped) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    _stop_called = 0;

    ozayn_lc_register_simple(&lc, "CompA", OZAYN_LC_PHASE_BOOTSTRAP,
                              OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);
    lc.components[0].stop_fn = test_stop;
    ozayn_lc_startup(&lc);

    /* request_shutdown internally calls perform_shutdown when RUNNING */
    ozayn_lc_request_shutdown(&lc, OZAYN_LC_SHUTDOWN_NORMAL);

    ASSERT_EQ(ozayn_lc_get_state(&lc), OZAYN_LC_STATE_STOPPED);
    ASSERT(_stop_called);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_phase_name_returns_correct) {
    ASSERT_STR_EQ(ozayn_lc_phase_name(OZAYN_LC_PHASE_BOOTSTRAP), "BOOTSTRAP");
    ASSERT_STR_EQ(ozayn_lc_phase_name(OZAYN_LC_PHASE_READY), "READY");
    return 0;
}

TEST(lc_state_name_returns_correct) {
    ASSERT_STR_EQ(ozayn_lc_state_name(OZAYN_LC_STATE_CREATED), "CREATED");
    ASSERT_STR_EQ(ozayn_lc_state_name(OZAYN_LC_STATE_RUNNING), "RUNNING");
    ASSERT_STR_EQ(ozayn_lc_state_name(OZAYN_LC_STATE_STOPPED), "STOPPED");
    return 0;
}

TEST(lc_readiness_check_passes) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    /* Readiness only checks REQUIRED components in STARTED or INITIALIZED state.
     * After startup, the component may be in STARTED state. */
    ozayn_lc_register_simple(&lc, "CompA", OZAYN_LC_PHASE_BOOTSTRAP,
                              OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);
    ozayn_lc_startup(&lc);

    /* Check component state after startup */
    const ozayn_lc_component_t *comp = ozayn_lc_get_component(&lc, "CompA");
    ASSERT(comp->state == OZAYN_LC_COMP_STARTED || comp->state == OZAYN_LC_COMP_INITIALIZED);

    /* Readiness check should pass for REQUIRED components that are started */
    int r = ozayn_lc_readiness_check(&lc);
    ASSERT_EQ(r, 1);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(lc_is_running_query) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    ASSERT(!ozayn_lc_is_running(&lc));

    ozayn_lc_register_simple(&lc, "CompA", OZAYN_LC_PHASE_BOOTSTRAP,
                              OZAYN_LC_CRITICALITY_REQUIRED, test_init, NULL);
    ozayn_lc_startup(&lc);
    ASSERT(ozayn_lc_is_running(&lc));

    ozayn_lc_shutdown(&lc);
    return 0;
}

int run_lifecycle_tests(void) {
    SUITE_BEGIN("Lifecycle Coordinator");
    RUN(lc_init_returns_ok);
    RUN(lc_register_component);
    RUN(lc_register_multiple_components);
    RUN(lc_startup_transitions_to_running);
    RUN(lc_shutdown_transitions_to_stopped);
    RUN(lc_phase_name_returns_correct);
    RUN(lc_state_name_returns_correct);
    RUN(lc_readiness_check_passes);
    RUN(lc_is_running_query);
    SUITE_END();
    return TOTAL_FAIL();
}
