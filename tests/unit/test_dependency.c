#include "../test_framework.h"
#include "dependency.h"
#include <string.h>

/*
 * test_dependency.c — Unit tests for Dependency Manager (Stage 23).
 *
 * Tests: registration, edges, resolution, cycle detection, failure propagation.
 */

TEST(dep_init_returns_ok) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    int r = ozayn_dep_init(&mgr, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(mgr.initialized);
    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_register_single_node) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    int r = ozayn_dep_register_simple(&mgr, "A");
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_dep_node_count(&mgr), 1);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_register_multiple_nodes) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_register_simple(&mgr, "C");

    ASSERT_EQ(ozayn_dep_node_count(&mgr), 3);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_add_edge) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");

    int r = ozayn_dep_add_required(&mgr, "A", "B");
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_dep_edge_count(&mgr), 1);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_resolve_linear_chain) {
    /* A -> B -> C means startup order: C, B, A */
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_register_simple(&mgr, "C");

    ozayn_dep_add_required(&mgr, "A", "B");
    ozayn_dep_add_required(&mgr, "B", "C");

    int r = ozayn_dep_resolve(&mgr);
    ASSERT_EQ(r, 0);

    char order[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_NAME];
    int count = ozayn_dep_get_startup_order(&mgr, order, OZAYN_DEP_MAX_NODES);
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(order[0], "C");
    ASSERT_STR_EQ(order[1], "B");
    ASSERT_STR_EQ(order[2], "A");

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_cycle_detection) {
    /* A -> B -> C -> A: circular */
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_register_simple(&mgr, "C");

    ozayn_dep_add_required(&mgr, "A", "B");
    ozayn_dep_add_required(&mgr, "B", "C");
    ozayn_dep_add_required(&mgr, "C", "A");

    int r = ozayn_dep_resolve(&mgr);
    ASSERT_NEQ(r, 0);
    ASSERT_GT(mgr.cycles_detected, 0);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_shutdown_order_reversed) {
    /* A -> B -> C: shutdown order should be A, B, C */
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_register_simple(&mgr, "C");

    ozayn_dep_add_required(&mgr, "A", "B");
    ozayn_dep_add_required(&mgr, "B", "C");
    ozayn_dep_resolve(&mgr);

    char order[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_NAME];
    int count = ozayn_dep_get_shutdown_order(&mgr, order, OZAYN_DEP_MAX_NODES);
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(order[0], "A");
    ASSERT_STR_EQ(order[1], "B");
    ASSERT_STR_EQ(order[2], "C");

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_get_state_returns_correct) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "X");
    ozayn_dep_state_t s = ozayn_dep_get_state(&mgr, "X");
    ASSERT_EQ(s, OZAYN_DEP_STATE_DISCOVERED);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_propagate_failure_blocks_dependents) {
    /* A -> B, B fails -> A blocked */
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_add_required(&mgr, "A", "B");

    ozayn_dep_propagate_failure(&mgr, "B");
    ozayn_dep_state_t s = ozayn_dep_get_state(&mgr, "A");
    ASSERT_EQ(s, OZAYN_DEP_STATE_BLOCKED);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_can_start_checks_deps) {
    /* can_start returns 1 when all REQUIRED deps are READY or DISCOVERED.
     * When a dep is BLOCKED, can_start returns 0. */
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 0 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_add_required(&mgr, "A", "B");

    /* B is DISCOVERED — can_start allows it */
    ASSERT(ozayn_dep_can_start(&mgr, "A"));

    /* Now block B */
    ozayn_dep_set_state(&mgr, "B", OZAYN_DEP_STATE_BLOCKED);
    ASSERT(!ozayn_dep_can_start(&mgr, "A"));

    /* B is READY — can_start allows it */
    ozayn_dep_set_state(&mgr, "B", OZAYN_DEP_STATE_READY);
    ASSERT(ozayn_dep_can_start(&mgr, "A"));

    ozayn_dep_shutdown(&mgr);
    return 0;
}

TEST(dep_forward_lookup) {
    ozayn_dep_manager_t mgr;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 1 };
    ozayn_dep_init(&mgr, &cfg);

    ozayn_dep_register_simple(&mgr, "A");
    ozayn_dep_register_simple(&mgr, "B");
    ozayn_dep_register_simple(&mgr, "C");
    ozayn_dep_add_required(&mgr, "A", "B");
    ozayn_dep_add_required(&mgr, "A", "C");

    char deps[OZAYN_DEP_MAX_PER_NODE][OZAYN_DEP_MAX_NAME];
    int count = ozayn_dep_get_dependencies(&mgr, "A", deps, OZAYN_DEP_MAX_PER_NODE);
    ASSERT_EQ(count, 2);

    ozayn_dep_shutdown(&mgr);
    return 0;
}

int run_dependency_tests(void) {
    SUITE_BEGIN("Dependency Manager");
    RUN(dep_init_returns_ok);
    RUN(dep_register_single_node);
    RUN(dep_register_multiple_nodes);
    RUN(dep_add_edge);
    RUN(dep_resolve_linear_chain);
    RUN(dep_cycle_detection);
    RUN(dep_shutdown_order_reversed);
    RUN(dep_get_state_returns_correct);
    RUN(dep_propagate_failure_blocks_dependents);
    RUN(dep_can_start_checks_deps);
    RUN(dep_forward_lookup);
    SUITE_END();
    return TOTAL_FAIL();
}
