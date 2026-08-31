#include "../test_framework.h"
#include "config_validate.h"

TEST(cv_init_returns_ok) {
    ozayn_config_validator_t cv;
    int r = ozayn_cv_init(&cv);
    ASSERT_EQ(r, 0);
    ASSERT(cv.initialized);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_add_range_int) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    ASSERT_EQ(ozayn_cv_add_range_int(&cv, "workers", 1, 128, 1), 0);
    ASSERT_EQ(cv.constraint_count, 1u);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_add_enum) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    const char *vals[] = { "sync", "async" };
    ASSERT_EQ(ozayn_cv_add_enum(&cv, "mode", vals, 2, 1), 0);
    ASSERT_EQ(cv.constraint_count, 1u);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_validate_valid_config) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    ozayn_cv_add_range_int(&cv, "workers", 1, 128, 1);
    ozayn_cv_add_range_uint(&cv, "queue", 64, 65536, 1);
    const char *modes[] = { "sync", "async" };
    ozayn_cv_add_enum(&cv, "mode", modes, 2, 1);

    ozayn_cv_key_value_t keys[3];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "workers"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 8;
    strcpy(keys[1].key, "queue");   keys[1].type = OZAYN_CV_TYPE_UINT; keys[1].value.u = 1024;
    strcpy(keys[2].key, "mode");    keys[2].type = OZAYN_CV_TYPE_STRING;
    strcpy(keys[2].value.s, "async");

    ASSERT_EQ(ozayn_cv_validate(&cv, keys, 3), 0);
    ASSERT(!ozayn_cv_has_errors(&cv));
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_validate_out_of_range) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    ozayn_cv_add_range_int(&cv, "workers", 1, 128, 1);

    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "workers"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 999;

    ASSERT_NEQ(ozayn_cv_validate(&cv, keys, 1), 0);
    ASSERT(ozayn_cv_has_errors(&cv));
    ASSERT_GT(ozayn_cv_error_count(&cv), 0u);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_validate_invalid_enum) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    const char *modes[] = { "sync", "async" };
    ozayn_cv_add_enum(&cv, "mode", modes, 2, 1);

    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "mode"); keys[0].type = OZAYN_CV_TYPE_STRING;
    strcpy(keys[0].value.s, "parallel");

    ASSERT_NEQ(ozayn_cv_validate(&cv, keys, 1), 0);
    ASSERT(ozayn_cv_has_errors(&cv));
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_validate_missing_required) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    ozayn_cv_add_range_int(&cv, "workers", 1, 128, 1); /* required */

    /* Provide keys but omit the required one */
    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "other"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 42;

    ASSERT_NEQ(ozayn_cv_validate(&cv, keys, 1), 0);
    ASSERT(ozayn_cv_has_errors(&cv));
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_snapshot_save_restore) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);

    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "workers"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 8;

    ASSERT_EQ(ozayn_cv_snapshot_save(&cv, keys, 1), 0);
    ASSERT_EQ(ozayn_cv_version(&cv), 1u);
    ASSERT_NOT_NULL(ozayn_cv_snapshot_current(&cv));

    ASSERT_EQ(ozayn_cv_snapshot_restore(&cv, 0), 0);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_version_increments) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);

    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "k"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 1;

    ozayn_cv_snapshot_save(&cv, keys, 1);
    ozayn_cv_snapshot_save(&cv, keys, 1);
    ozayn_cv_snapshot_save(&cv, keys, 1);
    ASSERT_EQ(ozayn_cv_version(&cv), 3u);
    ozayn_cv_shutdown(&cv);
    return 0;
}

TEST(cv_clear_errors) {
    ozayn_config_validator_t cv;
    ozayn_cv_init(&cv);
    ozayn_cv_add_range_int(&cv, "workers", 1, 128, 1);

    ozayn_cv_key_value_t keys[1];
    memset(keys, 0, sizeof(keys));
    strcpy(keys[0].key, "workers"); keys[0].type = OZAYN_CV_TYPE_INT; keys[0].value.i = 999;

    ozayn_cv_validate(&cv, keys, 1);
    ASSERT(ozayn_cv_has_errors(&cv));
    ozayn_cv_clear_errors(&cv);
    ASSERT(!ozayn_cv_has_errors(&cv));
    ozayn_cv_shutdown(&cv);
    return 0;
}

int run_cv_tests(void) {
    SUITE_BEGIN("Config Validator");
    RUN(cv_init_returns_ok);
    RUN(cv_add_range_int);
    RUN(cv_add_enum);
    RUN(cv_validate_valid_config);
    RUN(cv_validate_out_of_range);
    RUN(cv_validate_invalid_enum);
    RUN(cv_validate_missing_required);
    RUN(cv_snapshot_save_restore);
    RUN(cv_version_increments);
    RUN(cv_clear_errors);
    SUITE_END();
    return _tf_suite_fail;
}
