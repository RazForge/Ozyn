#include "../test_framework.h"
#include "release_mgr.h"

TEST(release_mgr_init) {
    ozayn_release_mgr_t mgr;
    ASSERT_EQ(ozayn_release_mgr_init(&mgr), 0);
    ASSERT(mgr.initialized);
    ozayn_release_mgr_shutdown(&mgr);
    ASSERT(!mgr.initialized);
    return 0;
}

TEST(manifest_init) {
    ozayn_rel_manifest_t m;
    ASSERT_EQ(ozayn_release_manifest_init(&m), 0);
    ASSERT_EQ(m.manifest_version, 1);
    ASSERT_STR_EQ(m.state_format, "1.0");
    return 0;
}

TEST(manifest_write_read) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ozayn_release_add_dependency(&mgr, "libfoo", "1.0", "2.0", 1);
    ASSERT_EQ(ozayn_release_manifest_write(&mgr.manifest, "/tmp/test_release.manifest"), 0);

    ozayn_rel_manifest_t m2;
    ASSERT_EQ(ozayn_release_manifest_read(&m2, "/tmp/test_release.manifest"), 0);
    ASSERT_EQ(m2.dep_count, 1);
    ASSERT_STR_EQ(m2.deps[0].name, "libfoo");
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(add_dependency) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ASSERT_EQ(ozayn_release_add_dependency(&mgr, "libc", "2.0", "", 1), 0);
    ASSERT_EQ(mgr.manifest.dep_count, 1);
    ASSERT_STR_EQ(mgr.manifest.deps[0].name, "libc");
    ASSERT_EQ(mgr.manifest.deps[0].required, 1);
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(verify_dependencies) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ozayn_release_add_dependency(&mgr, "liba", "1.0", "", 1);
    ASSERT_EQ(ozayn_release_verify_dependencies(&mgr), 0);
    ASSERT(ozayn_release_deps_satisfied(&mgr));
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(checksum_data) {
    const char *data = "hello world";
    uint32_t h1 = ozayn_release_checksum_data(data, 11);
    uint32_t h2 = ozayn_release_checksum_data(data, 11);
    ASSERT_EQ(h1, h2);
    ASSERT_NEQ(h1, 0);
    return 0;
}

TEST(checksum_different_data) {
    uint32_t h1 = ozayn_release_checksum_data("abc", 3);
    uint32_t h2 = ozayn_release_checksum_data("def", 3);
    ASSERT_NEQ(h1, h2);
    return 0;
}

TEST(add_integrity) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ASSERT_EQ(ozayn_release_add_integrity(&mgr, "file.bin", 12345), 0);
    ASSERT_EQ(mgr.manifest.file_count, 1);
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(backup_create) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    /* Backup of a non-existent source just creates the record */
    ASSERT_EQ(ozayn_release_backup_create(&mgr, "0.0.1", "/tmp"), 0);
    ASSERT_EQ(mgr.backup_count, 1);
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(smoke_test) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ASSERT_EQ(ozayn_release_smoke_add(&mgr, OZAYN_REL_SMOKE_BINARY, 1, "ok", 100), 0);
    ASSERT_EQ(ozayn_release_smoke_add(&mgr, OZAYN_REL_SMOKE_STARTUP, 0, "fail", 200), 0);
    ASSERT_EQ(ozayn_release_smoke_passed_count(&mgr), 1);
    ASSERT_EQ(ozayn_release_smoke_failed_count(&mgr), 1);
    ASSERT(!ozayn_release_smoke_all_passed(&mgr));
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(gate_test) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ASSERT_EQ(ozayn_release_gate_add(&mgr, OZAYN_REL_GATE_BUILD, 1, "ok"), 0);
    ASSERT_EQ(ozayn_release_gate_add(&mgr, OZAYN_REL_GATE_UNIT_TESTS, 1, "ok"), 0);
    ASSERT(ozayn_release_gates_all_passed(&mgr));
    ASSERT_EQ(ozayn_release_gate_passed_count(&mgr), 2);
    ASSERT_EQ(ozayn_release_gate_failed_count(&mgr), 0);
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(deploy_log) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    ASSERT_EQ(ozayn_release_deploy_log(&mgr, "install", "0.1.0", "OK", "success"), 0);
    ASSERT_EQ(mgr.deploy_log_count, 1);
    const ozayn_rel_deploy_log_t *e = ozayn_release_deploy_log_get(&mgr, 0);
    ASSERT_NOT_NULL(e);
    ASSERT_STR_EQ(e->action, "install");
    ASSERT_STR_EQ(e->version, "0.1.0");
    ASSERT_NULL(ozayn_release_deploy_log_get(&mgr, 99));
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

TEST(release_readiness) {
    ozayn_release_mgr_t mgr;
    ozayn_release_mgr_init(&mgr);
    /* No gates or smoke tests → not ready */
    ASSERT(!ozayn_release_is_ready(&mgr));
    ozayn_release_gate_add(&mgr, OZAYN_REL_GATE_BUILD, 1, "ok");
    ASSERT(!ozayn_release_is_ready(&mgr)); /* still no smoke tests */
    ozayn_release_smoke_add(&mgr, OZAYN_REL_SMOKE_BINARY, 1, "ok", 100);
    ASSERT(ozayn_release_is_ready(&mgr));
    ozayn_release_mgr_shutdown(&mgr);
    return 0;
}

int run_release_tests(void) {
    SUITE_BEGIN("Release Manager");
    RUN(release_mgr_init);
    RUN(manifest_init);
    RUN(manifest_write_read);
    RUN(add_dependency);
    RUN(verify_dependencies);
    RUN(checksum_data);
    RUN(checksum_different_data);
    RUN(add_integrity);
    RUN(backup_create);
    RUN(smoke_test);
    RUN(gate_test);
    RUN(deploy_log);
    RUN(release_readiness);
    SUITE_END();
    return _tf_suite_fail;
}
