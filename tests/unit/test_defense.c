#include "../test_framework.h"
#include "defense.h"

TEST(defense_not_null_ok) {
    int x = 42;
    ASSERT_EQ(ozayn_defense_not_null(&x, "x"), 0);
    return 0;
}

TEST(defense_not_null_rejects) {
    ASSERT_NEQ(ozayn_defense_not_null(NULL, "null"), 0);
    return 0;
}

TEST(defense_not_empty_ok) {
    ASSERT_EQ(ozayn_defense_not_empty("hello", "str"), 0);
    return 0;
}

TEST(defense_not_empty_rejects) {
    ASSERT_NEQ(ozayn_defense_not_empty("", "empty"), 0);
    return 0;
}

TEST(defense_in_range_ok) {
    ASSERT_EQ(ozayn_defense_in_range_i32(50, 0, 100, "v"), 0);
    return 0;
}

TEST(defense_in_range_rejects) {
    ASSERT_NEQ(ozayn_defense_in_range_i32(150, 0, 100, "v"), 0);
    return 0;
}

TEST(defense_strlcpy_ok) {
    char buf[8];
    ozayn_defense_strlcpy(buf, "hello", sizeof(buf));
    ASSERT_STR_EQ(buf, "hello");
    return 0;
}

TEST(defense_strlcpy_truncates) {
    char buf[4];
    ozayn_defense_strlcpy(buf, "hello world", sizeof(buf));
    ASSERT_EQ((int)strlen(buf), 3);
    return 0;
}

TEST(defense_is_identifier_ok) {
    ASSERT(ozayn_defense_is_identifier("EventEngine"));
    ASSERT(ozayn_defense_is_identifier("my_module"));
    return 0;
}

TEST(defense_is_identifier_rejects) {
    ASSERT(!ozayn_defense_is_identifier("../etc/passwd"));
    ASSERT(!ozayn_defense_is_identifier(""));
    return 0;
}

TEST(defense_is_path_safe_ok) {
    ASSERT(ozayn_defense_is_path_safe("data/config"));
    ASSERT(ozayn_defense_is_path_safe("modules/test.so"));
    return 0;
}

TEST(defense_is_path_safe_rejects) {
    ASSERT(!ozayn_defense_is_path_safe("../etc/passwd"));
    ASSERT(!ozayn_defense_is_path_safe("~root"));
    ASSERT(!ozayn_defense_is_path_safe(""));
    ASSERT(!ozayn_defense_is_path_safe(NULL));
    return 0;
}

TEST(defense_log_record) {
    ozayn_defense_log_t log;
    ozayn_defense_log_init(&log);
    ASSERT_EQ(ozayn_defense_log_count(&log), 0u);
    ozayn_defense_log_record(&log, "test", "violation", -1);
    ASSERT_EQ(ozayn_defense_log_count(&log), 1u);
    return 0;
}

int run_defense_tests(void) {
    SUITE_BEGIN("Defense");
    RUN(defense_not_null_ok);
    RUN(defense_not_null_rejects);
    RUN(defense_not_empty_ok);
    RUN(defense_not_empty_rejects);
    RUN(defense_in_range_ok);
    RUN(defense_in_range_rejects);
    RUN(defense_strlcpy_ok);
    RUN(defense_strlcpy_truncates);
    RUN(defense_is_identifier_ok);
    RUN(defense_is_identifier_rejects);
    RUN(defense_is_path_safe_ok);
    RUN(defense_is_path_safe_rejects);
    RUN(defense_log_record);
    SUITE_END();
    return _tf_suite_fail;
}
