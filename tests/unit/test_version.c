#include "../test_framework.h"
#include "version.h"

TEST(semver_parse_valid) {
    ozayn_semver_t v;
    ASSERT_EQ(ozayn_semver_parse(&v, "1.2.3"), 0);
    ASSERT_EQ(v.major, 1);
    ASSERT_EQ(v.minor, 2);
    ASSERT_EQ(v.patch, 3);
    return 0;
}

TEST(semver_parse_prerelease) {
    ozayn_semver_t v;
    ASSERT_EQ(ozayn_semver_parse(&v, "1.0.0-rc1"), 0);
    ASSERT_EQ(v.major, 1);
    ASSERT_EQ(v.minor, 0);
    ASSERT_EQ(v.patch, 0);
    ASSERT_STR_EQ(v.prerelease, "rc1");
    return 0;
}

TEST(semver_parse_build_metadata) {
    ozayn_semver_t v;
    ASSERT_EQ(ozayn_semver_parse(&v, "1.0.0+build.123"), 0);
    ASSERT_EQ(v.major, 1);
    ASSERT_STR_EQ(v.build, "build.123");
    return 0;
}

TEST(semver_parse_invalid) {
    ozayn_semver_t v;
    ASSERT_NEQ(ozayn_semver_parse(&v, "not_a_version"), 0);
    ASSERT_NEQ(ozayn_semver_parse(&v, "1.2"), 0);
    ASSERT_NEQ(ozayn_semver_parse(&v, ""), 0);
    return 0;
}

TEST(semver_compare_equal) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.2.3");
    ozayn_semver_parse(&b, "1.2.3");
    ASSERT_EQ(ozayn_semver_compare(&a, &b), OZAYN_VER_EQUAL);
    return 0;
}

TEST(semver_compare_greater) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.2.4");
    ozayn_semver_parse(&b, "1.2.3");
    ASSERT_EQ(ozayn_semver_compare(&a, &b), OZAYN_VER_GREATER);
    return 0;
}

TEST(semver_compare_less) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.2.3");
    ozayn_semver_parse(&b, "1.2.4");
    ASSERT_EQ(ozayn_semver_compare(&a, &b), OZAYN_VER_LESS);
    return 0;
}

TEST(semver_compare_incompatible) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.0.0");
    ozayn_semver_parse(&b, "2.0.0");
    ASSERT_EQ(ozayn_semver_compare(&a, &b), OZAYN_VER_INCOMPATIBLE);
    return 0;
}

TEST(semver_compare_prerelease_less) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.0.0-alpha");
    ozayn_semver_parse(&b, "1.0.0");
    ASSERT_EQ(ozayn_semver_compare(&a, &b), OZAYN_VER_LESS);
    return 0;
}

TEST(semver_major_compatible) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.2.3");
    ozayn_semver_parse(&b, "1.9.0");
    ASSERT_EQ(ozayn_semver_major_compatible(&a, &b), 0);
    return 0;
}

TEST(semver_major_incompatible) {
    ozayn_semver_t a, b;
    ozayn_semver_parse(&a, "1.0.0");
    ozayn_semver_parse(&b, "2.0.0");
    ASSERT_NEQ(ozayn_semver_major_compatible(&a, &b), 0);
    return 0;
}

TEST(semver_format) {
    ozayn_semver_t v;
    ozayn_semver_parse(&v, "1.2.3");
    char buf[64];
    ASSERT_EQ(ozayn_semver_format(&v, buf, sizeof(buf)), 0);
    ASSERT_STR_EQ(buf, "1.2.3");
    return 0;
}

TEST(build_identity_init) {
    ozayn_build_identity_t id;
    ozayn_build_identity_init(&id);
    ASSERT(id.version[0] != '\0');
    ASSERT(id.platform[0] != '\0');
    ASSERT(id.arch[0] != '\0');
    ASSERT(id.compiler[0] != '\0');
    return 0;
}

TEST(version_string) {
    const char *v = ozayn_version_string();
    ASSERT_NOT_NULL(v);
    ASSERT(v[0] != '\0');
    return 0;
}

TEST(version_components) {
    ASSERT_EQ(ozayn_version_major(), 0);
    ASSERT_EQ(ozayn_version_minor(), 1);
    return 0;
}

TEST(full_version_string) {
    char buf[128];
    ASSERT_EQ(ozayn_full_version_string(buf, sizeof(buf)), 0);
    ASSERT(buf[0] != '\0');
    return 0;
}

int run_version_tests(void) {
    SUITE_BEGIN("Version");
    RUN(semver_parse_valid);
    RUN(semver_parse_prerelease);
    RUN(semver_parse_build_metadata);
    RUN(semver_parse_invalid);
    RUN(semver_compare_equal);
    RUN(semver_compare_greater);
    RUN(semver_compare_less);
    RUN(semver_compare_incompatible);
    RUN(semver_compare_prerelease_less);
    RUN(semver_major_compatible);
    RUN(semver_major_incompatible);
    RUN(semver_format);
    RUN(build_identity_init);
    RUN(version_string);
    RUN(version_components);
    RUN(full_version_string);
    SUITE_END();
    return _tf_suite_fail;
}
