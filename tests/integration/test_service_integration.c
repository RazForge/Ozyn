#include "../test_framework.h"
#include "security.h"
#include "authorization.h"
#include "reload_mgr.h"
#include "dependency.h"
#include "lifecycle.h"
#include <string.h>

/*
 * test_service_integration.c — Integration tests: security + authorization + reload.
 *
 * Tests: unauthorized reload rejected, authorized reload succeeds, audit trail.
 */

TEST(unauthorized_reload_rejected) {
    ozayn_security_manager_t sec;
    ozayn_security_init(&sec, 1);

    ozayn_authorization_manager_t authz;
    ozayn_authorization_init(&authz, 1);
    ozayn_authorization_set_security(&authz, &sec);

    ozayn_authorization_register_permission(&authz, "module.reload", "Reload module");
    ozayn_authorization_create_role(&authz, "RELOAD_ADMIN");
    ozayn_authorization_role_add_permission(&authz, "RELOAD_ADMIN", "module.reload");

    ozayn_security_register_identity(&sec, "ozayn.admin", "Admin",
                                     OZAYN_IDENTITY_CORE, OZAYN_AUTH_TRUST, 0, 0);
    ozayn_authorization_assign_role(&authz, "ozayn.admin", "RELOAD_ADMIN");

    ozayn_security_register_identity(&sec, "ozayn.viewer", "Viewer",
                                     OZAYN_IDENTITY_MODULE, OZAYN_AUTH_TRUST, 0, 0);

    ozayn_authz_result_t r1 = ozayn_authorize(&authz, "ozayn.admin", "module", "reload");
    ASSERT_EQ(r1.decision, OZAYN_AUTHZ_ALLOW);

    ozayn_authz_result_t r2 = ozayn_authorize(&authz, "ozayn.viewer", "module", "reload");
    ASSERT_EQ(r2.decision, OZAYN_AUTHZ_DENY);

    ozayn_authorization_shutdown(&authz);
    ozayn_security_shutdown(&sec);
    return 0;
}

TEST(reload_with_state_preservation) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Persistent", "1.0.0", OZAYN_RELOAD_SUPPORTED, 1);

    /* Save state before reload */
    const char *state = "{\"counter\":42}";
    uint32_t save_ver = 1;
    ozayn_reload_save_state(&mgr, "Persistent", state, (uint32_t)strlen(state) + 1, save_ver);

    /* Reload */
    ozayn_reload_request(&mgr, "Persistent", "2.0.0", "admin", "upgrade");
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    /* Verify state preserved — version may be incremented by reload process */
    char loaded[256];
    uint32_t ver = 0;
    int r = ozayn_reload_load_state(&mgr, "Persistent", loaded, sizeof(loaded), &ver);
    ASSERT_EQ(r, 0);
    ASSERT_STR_EQ(loaded, state);
    ASSERT_GE(ver, 1);

    const ozayn_reload_component_t *comp = ozayn_reload_find(&mgr, "Persistent");
    ASSERT_STR_EQ(comp->current_version, "2.0.0");

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(reload_audit_trail_records_all) {
    ozayn_reload_mgr_t mgr;
    ozayn_reload_config_t cfg = { .quiesce_timeout_ms = 5000, .load_timeout_ms = 10000, .health_check_timeout_ms = 3000, .rollback_on_fail = 1, .max_concurrent = 1 };
    ozayn_reload_mgr_init(&mgr, &cfg);

    ozayn_reload_register(&mgr, "Service", "1.0.0", OZAYN_RELOAD_SUPPORTED, 0);

    ozayn_reload_request(&mgr, "Service", "2.0.0", "admin", "v2");
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    ozayn_reload_request(&mgr, "Service", "3.0.0", "admin", "v3");
    for (int i = 0; i < 20; i++) {
        if (!ozayn_reload_is_busy(&mgr)) break;
        ozayn_reload_tick(&mgr);
    }

    uint32_t count = ozayn_reload_audit_count(&mgr);
    ASSERT_GE(count, 2);

    const ozayn_reload_audit_t *e0 = ozayn_reload_get_audit(&mgr, 0);
    const ozayn_reload_audit_t *e1 = ozayn_reload_get_audit(&mgr, 1);
    ASSERT_STR_EQ(e0->component, "Service");
    ASSERT_STR_EQ(e1->component, "Service");

    ozayn_reload_mgr_shutdown(&mgr);
    return 0;
}

TEST(dependency_blocks_lifecycle) {
    /* After propagate_failure, dependents are BLOCKED.
     * can_start checks if deps are READY/DISCOVERED — a BLOCKED dep causes can_start=0. */
    ozayn_dep_manager_t dep;
    ozayn_dep_config_t cfg = { .resolve_timeout_ms = 5000, .fail_on_cycle = 1, .fail_on_missing = 0 };
    ozayn_dep_init(&dep, &cfg);

    ozayn_dep_register_simple(&dep, "DB");
    ozayn_dep_register_simple(&dep, "Cache");
    ozayn_dep_register_simple(&dep, "API");

    ozayn_dep_add_required(&dep, "Cache", "DB");
    ozayn_dep_add_required(&dep, "API", "DB");
    ozayn_dep_add_required(&dep, "API", "Cache");

    /* Set DB to BLOCKED to simulate failure */
    ozayn_dep_set_state(&dep, "DB", OZAYN_DEP_STATE_BLOCKED);

    /* Cache depends on DB (BLOCKED) — can't start */
    ASSERT(!ozayn_dep_can_start(&dep, "Cache"));

    /* API depends on DB (BLOCKED) and Cache — can't start */
    ASSERT(!ozayn_dep_can_start(&dep, "API"));

    ozayn_dep_shutdown(&dep);
    return 0;
}

TEST(security_boundary_enforced) {
    ozayn_security_manager_t sec;
    ozayn_security_init(&sec, 1);

    ASSERT(!ozayn_security_is_trusted(&sec, "unknown.process"));

    ozayn_peer_creds_t creds = { .uid = 9999, .gid = 9999, .pid = 9999, .valid = 1 };
    ozayn_auth_result_t r = ozayn_security_authenticate(&sec, "unknown.process", &creds);
    ASSERT_EQ(r, OZAYN_AUTH_ERR_NOT_FOUND);

    ozayn_security_shutdown(&sec);
    return 0;
}

TEST(authorization_deny_unknown_permission) {
    ozayn_authorization_manager_t authz;
    ozayn_authorization_init(&authz, 1);

    ozayn_authz_result_t r = ozayn_authorize(&authz, "anyone", "unknown", "action");
    ASSERT_EQ(r.decision, OZAYN_AUTHZ_DENY);
    ASSERT_EQ(r.reason, OZAYN_DENY_REASON_UNKNOWN_PERMISSION);

    ozayn_authorization_shutdown(&authz);
    return 0;
}

int run_service_integration_tests(void) {
    SUITE_BEGIN("Service Integration");
    RUN(unauthorized_reload_rejected);
    RUN(reload_with_state_preservation);
    RUN(reload_audit_trail_records_all);
    RUN(dependency_blocks_lifecycle);
    RUN(security_boundary_enforced);
    RUN(authorization_deny_unknown_permission);
    SUITE_END();
    return TOTAL_FAIL();
}
