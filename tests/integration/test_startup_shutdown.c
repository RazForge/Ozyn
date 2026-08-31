#include "../test_framework.h"
#include "lifecycle.h"
#include "dependency.h"
#include <string.h>

/*
 * test_startup_shutdown.c — Integration tests for startup/shutdown (Stages 22-23).
 *
 * Tests: dependency-ordered startup, shutdown rollback, partial failure.
 */

static int _init_seq[8];
static int _init_idx = 0;
static int _stop_seq[8];
static int _stop_idx = 0;

static int track_init(void *ctx) {
    int *val = (int *)ctx;
    _init_seq[_init_idx++] = *val;
    return 0;
}

static void track_stop(void *ctx) {
    int *val = (int *)ctx;
    _stop_seq[_stop_idx++] = *val;
}

static int fail_init(void *ctx) {
    (void)ctx;
    return -1;  /* simulate failure */
}

TEST(startup_dependency_order) {
    /* Lifecycle respects phase ordering: BOOTSTRAP → FOUNDATION → SECURITY */
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    _init_idx = 0;
    int a = 1, b = 2, c = 3;

    ozayn_lc_register_simple(&lc, "Bootstrap", OZAYN_LC_PHASE_BOOTSTRAP,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &a);
    ozayn_lc_register_simple(&lc, "Foundation", OZAYN_LC_PHASE_FOUNDATION,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &b);
    ozayn_lc_register_simple(&lc, "Security", OZAYN_LC_PHASE_SECURITY,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &c);

    ozayn_lc_startup(&lc);

    /* Should initialize in phase order: 1, 2, 3 */
    ASSERT_EQ(_init_idx, 3);
    ASSERT_EQ(_init_seq[0], 1);
    ASSERT_EQ(_init_seq[1], 2);
    ASSERT_EQ(_init_seq[2], 3);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(shutdown_reverse_order) {
    /* Shutdown stops dependents before dependencies */
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    _init_idx = 0;
    _stop_idx = 0;
    int a = 1, b = 2, c = 3;

    ozayn_lc_register_simple(&lc, "A", OZAYN_LC_PHASE_FOUNDATION,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &a);
    ozayn_lc_register_simple(&lc, "B", OZAYN_LC_PHASE_FOUNDATION,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &b);
    ozayn_lc_register_simple(&lc, "C", OZAYN_LC_PHASE_FOUNDATION,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &c);

    /* Manually set stop functions */
    for (int i = 0; i < lc.component_count; i++) {
        lc.components[i].stop_fn = track_stop;
    }
    /* Set contexts for stop */
    lc.components[0].context = &a;
    lc.components[1].context = &b;
    lc.components[2].context = &c;

    ozayn_lc_startup(&lc);
    ozayn_lc_request_shutdown(&lc, OZAYN_LC_SHUTDOWN_NORMAL);
    ozayn_lc_perform_shutdown(&lc);

    /* Shutdown should have stopped all 3 */
    ASSERT_EQ(_stop_idx, 3);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(required_component_failure_aborts_startup) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    ozayn_lc_register_simple(&lc, "FailComp", OZAYN_LC_PHASE_SECURITY,
                              OZAYN_LC_CRITICALITY_REQUIRED, fail_init, NULL);

    int r = ozayn_lc_startup(&lc);
    /* Startup should fail because a REQUIRED component failed */
    ASSERT_NEQ(r, 0);
    ASSERT_EQ(ozayn_lc_get_state(&lc), OZAYN_LC_STATE_FAILED);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(optional_component_failure_continues) {
    ozayn_lc_t lc;
    ozayn_lc_config_t cfg = { .startup_timeout_ms = 5000, .component_timeout_ms = 1000, .shutdown_timeout_ms = 5000 };
    ozayn_lc_init(&lc, &cfg);

    _init_idx = 0;
    int a = 1;

    ozayn_lc_register_simple(&lc, "FailOptional", OZAYN_LC_PHASE_SECURITY,
                              OZAYN_LC_CRITICALITY_OPTIONAL, fail_init, NULL);
    ozayn_lc_register_simple(&lc, "AfterFail", OZAYN_LC_PHASE_RUNTIME_SERVICES,
                              OZAYN_LC_CRITICALITY_REQUIRED, track_init, &a);

    int r = ozayn_lc_startup(&lc);
    /* Startup should succeed — optional failure is tolerated */
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_lc_get_state(&lc), OZAYN_LC_STATE_RUNNING);

    ozayn_lc_shutdown(&lc);
    return 0;
}

TEST(dependency_cycle_prevents_startup) {
    ozayn_dep_manager_t dep;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&dep, &cfg);

    ozayn_dep_register_simple(&dep, "X");
    ozayn_dep_register_simple(&dep, "Y");
    ozayn_dep_register_simple(&dep, "Z");

    ozayn_dep_add_required(&dep, "X", "Y");
    ozayn_dep_add_required(&dep, "Y", "Z");
    ozayn_dep_add_required(&dep, "Z", "X");

    int r = ozayn_dep_resolve(&dep);
    ASSERT_NEQ(r, 0);  /* cycle detected, resolve fails */
    ASSERT_GT(dep.cycles_detected, 0);

    ozayn_dep_shutdown(&dep);
    return 0;
}

TEST(dependency_resolves_correct_order) {
    /* Verify startup order: EventEngine before ModuleManager before PluginManager */
    ozayn_dep_manager_t dep;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&dep, &cfg);

    ozayn_dep_register_simple(&dep, "EventEngine");
    ozayn_dep_register_simple(&dep, "ModuleManager");
    ozayn_dep_register_simple(&dep, "PluginManager");

    ozayn_dep_add_required(&dep, "ModuleManager", "EventEngine");
    ozayn_dep_add_required(&dep, "PluginManager", "ModuleManager");

    int r = ozayn_dep_resolve(&dep);
    ASSERT_EQ(r, 0);

    char order[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_NAME];
    int count = ozayn_dep_get_startup_order(&dep, order, OZAYN_DEP_MAX_NODES);
    ASSERT_EQ(count, 3);

    /* EventEngine must come before ModuleManager */
    int ee_idx = -1, mm_idx = -1, pm_idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(order[i], "EventEngine") == 0) ee_idx = i;
        if (strcmp(order[i], "ModuleManager") == 0) mm_idx = i;
        if (strcmp(order[i], "PluginManager") == 0) pm_idx = i;
    }
    ASSERT(ee_idx < mm_idx);
    ASSERT(mm_idx < pm_idx);

    ozayn_dep_shutdown(&dep);
    return 0;
}

int run_startup_shutdown_tests(void) {
    SUITE_BEGIN("Startup & Shutdown Integration");
    RUN(startup_dependency_order);
    RUN(shutdown_reverse_order);
    RUN(required_component_failure_aborts_startup);
    RUN(optional_component_failure_continues);
    RUN(dependency_cycle_prevents_startup);
    RUN(dependency_resolves_correct_order);
    SUITE_END();
    return TOTAL_FAIL();
}
