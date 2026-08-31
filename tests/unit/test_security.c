#include "../test_framework.h"
#include "security.h"
#include "authorization.h"
#include <string.h>

/*
 * test_security.c — Unit tests for Security & Authorization (Stages 14-15).
 *
 * Tests: identity registration, authentication, authorization, role assignment.
 */

/* ---- Security tests ---- */

TEST(security_init_returns_ok) {
    ozayn_security_manager_t mgr;
    int r = ozayn_security_init(&mgr, 1);
    ASSERT_EQ(r, OZAYN_OK);
    ASSERT(mgr.initialized);
    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_register_identity) {
    ozayn_security_manager_t mgr;
    ozayn_security_init(&mgr, 1);

    /* ozayn.core is already registered by init — use a different name */
    ozayn_result_t r = ozayn_security_register_identity(&mgr,
        "ozayn.test", "Test Component",
        OZAYN_IDENTITY_MODULE, OZAYN_AUTH_TRUST, 0, 0);
    ASSERT_EQ(r, OZAYN_OK);
    ASSERT_EQ(mgr.identity_count, 2);

    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_authenticate_trusted) {
    ozayn_security_manager_t mgr;
    ozayn_security_init(&mgr, 1);

    ozayn_security_register_identity(&mgr,
        "ozayn.test", "Test Component",
        OZAYN_IDENTITY_MODULE, OZAYN_AUTH_TRUST, 0, 0);

    ozayn_peer_creds_t creds = { .uid = 1000, .gid = 1000, .pid = 1234, .valid = 1 };
    ozayn_auth_result_t r = ozayn_security_authenticate(&mgr, "ozayn.test", &creds);
    ASSERT_EQ(r, OZAYN_AUTH_OK);

    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_authenticate_not_found) {
    ozayn_security_manager_t mgr;
    ozayn_security_init(&mgr, 1);

    ozayn_peer_creds_t creds = { .uid = 1000, .gid = 1000, .pid = 1234, .valid = 1 };
    ozayn_auth_result_t r = ozayn_security_authenticate(&mgr, "nonexistent", &creds);
    ASSERT_EQ(r, OZAYN_AUTH_ERR_NOT_FOUND);

    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_revoke_identity) {
    ozayn_security_manager_t mgr;
    ozayn_security_init(&mgr, 1);

    ozayn_security_register_identity(&mgr,
        "ozayn.old", "Old Component",
        OZAYN_IDENTITY_MODULE, OZAYN_AUTH_TRUST, 0, 0);

    ozayn_result_t r = ozayn_security_revoke_identity(&mgr, "ozayn.old");
    ASSERT_EQ(r, OZAYN_OK);

    ozayn_trust_state_t ts = ozayn_security_get_trust_state(&mgr, "ozayn.old");
    ASSERT_EQ(ts, OZAYN_TRUST_REVOKED);

    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_is_trusted_query) {
    ozayn_security_manager_t mgr;
    ozayn_security_init(&mgr, 1);

    ASSERT(ozayn_security_is_trusted(&mgr, "ozayn.core"));
    ASSERT(!ozayn_security_is_trusted(&mgr, "nonexistent"));

    ozayn_security_shutdown(&mgr);
    return 0;
}

TEST(security_auth_result_names) {
    /* Implementation returns "SUCCESS" for OZAYN_AUTH_OK */
    ASSERT_STR_EQ(ozayn_auth_result_name(OZAYN_AUTH_OK), "SUCCESS");
    ASSERT_STR_EQ(ozayn_auth_result_name(OZAYN_AUTH_ERR_NOT_FOUND), "NOT_FOUND");
    return 0;
}

/* ---- Authorization tests ---- */

TEST(authorization_init_returns_ok) {
    ozayn_authorization_manager_t mgr;
    int r = ozayn_authorization_init(&mgr, 1);
    ASSERT_EQ(r, OZAYN_OK);
    ASSERT(mgr.initialized);
    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_register_permission) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    /* Authorization may already have default permissions; register a unique one */
    int before = mgr.permission_count;
    ozayn_result_t r = ozayn_authorization_register_permission(&mgr,
        "custom.read", "Custom read permission");
    ASSERT_EQ(r, OZAYN_OK);
    ASSERT_EQ(mgr.permission_count, before + 1);

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_create_role) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    int before = mgr.role_count;
    ozayn_result_t r = ozayn_authorization_create_role(&mgr, "CUSTOM_ROLE");
    ASSERT_EQ(r, OZAYN_OK);
    ASSERT_EQ(mgr.role_count, before + 1);

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_assign_role_to_identity) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    ozayn_authorization_create_role(&mgr, "ADMIN");
    ozayn_result_t r = ozayn_authorization_assign_role(&mgr, "ozayn.core", "ADMIN");
    ASSERT_EQ(r, OZAYN_OK);

    ASSERT(ozayn_authorization_identity_has_role(&mgr, "ozayn.core", "ADMIN"));

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_allow_authorized) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    ozayn_authorization_register_permission(&mgr, "camera.read", "Read camera");
    ozayn_authorization_create_role(&mgr, "VISION");
    ozayn_authorization_role_add_permission(&mgr, "VISION", "camera.read");
    ozayn_authorization_assign_role(&mgr, "ozayn.vision", "VISION");

    ozayn_authz_result_t r = ozayn_authorize(&mgr, "ozayn.vision", "camera", "read");
    ASSERT_EQ(r.decision, OZAYN_AUTHZ_ALLOW);

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_deny_unauthorized) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    ozayn_authorization_register_permission(&mgr, "core.shutdown", "Shutdown core");
    ozayn_authorization_create_role(&mgr, "ADMIN");
    ozayn_authorization_role_add_permission(&mgr, "ADMIN", "core.shutdown");
    ozayn_authorization_assign_role(&mgr, "ozayn.core", "ADMIN");

    /* ozayn.vision has no role, should be denied */
    ozayn_authz_result_t r = ozayn_authorize(&mgr, "ozayn.vision", "core", "shutdown");
    ASSERT_EQ(r.decision, OZAYN_AUTHZ_DENY);

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_deny_by_default) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    /* No permissions, no roles — everything denied */
    ozayn_authz_result_t r = ozayn_authorize(&mgr, "anyone", "any", "thing");
    ASSERT_EQ(r.decision, OZAYN_AUTHZ_DENY);

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_revoke_role) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    ozayn_authorization_create_role(&mgr, "TEMP");
    ozayn_authorization_assign_role(&mgr, "ozayn.tmp", "TEMP");
    ASSERT(ozayn_authorization_identity_has_role(&mgr, "ozayn.tmp", "TEMP"));

    ozayn_authorization_revoke_role(&mgr, "ozayn.tmp", "TEMP");
    ASSERT(!ozayn_authorization_identity_has_role(&mgr, "ozayn.tmp", "TEMP"));

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

TEST(authorization_role_permission_check) {
    ozayn_authorization_manager_t mgr;
    ozayn_authorization_init(&mgr, 1);

    ozayn_authorization_register_permission(&mgr, "resource.read", "Read");
    ozayn_authorization_create_role(&mgr, "READER");
    ozayn_authorization_role_add_permission(&mgr, "READER", "resource.read");

    ASSERT(ozayn_authorization_role_has_permission(&mgr, "READER", "resource.read"));
    ASSERT(!ozayn_authorization_role_has_permission(&mgr, "READER", "resource.write"));

    ozayn_authorization_shutdown(&mgr);
    return 0;
}

int run_security_tests(void) {
    SUITE_BEGIN("Security & Authorization");
    RUN(security_init_returns_ok);
    RUN(security_register_identity);
    RUN(security_authenticate_trusted);
    RUN(security_authenticate_not_found);
    RUN(security_revoke_identity);
    RUN(security_is_trusted_query);
    RUN(security_auth_result_names);
    RUN(authorization_init_returns_ok);
    RUN(authorization_register_permission);
    RUN(authorization_create_role);
    RUN(authorization_assign_role_to_identity);
    RUN(authorization_allow_authorized);
    RUN(authorization_deny_unauthorized);
    RUN(authorization_deny_by_default);
    RUN(authorization_revoke_role);
    RUN(authorization_role_permission_check);
    SUITE_END();
    return TOTAL_FAIL();
}
