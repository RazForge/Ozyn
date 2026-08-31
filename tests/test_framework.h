#ifndef OZAYN_TEST_FRAMEWORK_H
#define OZAYN_TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

/*
 * test_framework.h — Lightweight C test framework for OZAYN Core.
 *
 * Usage:
 *   TEST(test_name) {
 *       ASSERT(condition);
 *       ASSERT_EQ(a, b);
 *       PASS();
 *   }
 *
 *   int main(void) {
 *       SUITE_BEGIN("Suite Name");
 *       RUN(test_name);
 *       SUITE_END();
 *       return FAILED() ? 1 : 0;
 *   }
 */

/* ---- Counters (per-file, reset per suite) ---- */
static int _tf_pass __attribute__((unused)) = 0;
static int _tf_fail __attribute__((unused)) = 0;
static int _tf_total __attribute__((unused)) = 0;
static int _tf_suite_pass __attribute__((unused)) = 0;
static int _tf_suite_fail __attribute__((unused)) = 0;
static int _tf_global_pass __attribute__((unused)) = 0;
static int _tf_global_fail __attribute__((unused)) = 0;

/* ---- Test function type ---- */
typedef int (*tf_test_fn)(void);

/* ---- Internal: run a single test ---- */
#define RUN(test_fn) do { \
    _tf_total++; \
    _tf_suite_total++; \
    printf("    [%03d] %-50s ", _tf_total, #test_fn); \
    fflush(stdout); \
    if (test_fn() == 0) { \
        _tf_pass++; \
        _tf_suite_pass++; \
        printf("PASS\n"); \
    } else { \
        _tf_fail++; \
        _tf_suite_fail++; \
    } \
} while(0)

/* ---- Assertions ---- */
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n      %s:%d: ASSERT(%s) failed\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while(0)

#define ASSERT_MSG(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL\n      %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n      %s:%d: %s == %s (got %d vs %d)\n", \
               __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        return 1; \
    } \
} while(0)

#define ASSERT_NEQ(a, b) do { \
    if ((a) == (b)) { \
        printf("FAIL\n      %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    } \
} while(0)

#define ASSERT_NULL(p) do { \
    if ((p) != NULL) { \
        printf("FAIL\n      %s:%d: %s should be NULL\n", __FILE__, __LINE__, #p); \
        return 1; \
    } \
} while(0)

#define ASSERT_NOT_NULL(p) do { \
    if ((p) == NULL) { \
        printf("FAIL\n      %s:%d: %s should not be NULL\n", __FILE__, __LINE__, #p); \
        return 1; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n      %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        return 1; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    if ((a) <= (b)) { \
        printf("FAIL\n      %s:%d: %s > %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    } \
} while(0)

#define ASSERT_GE(a, b) do { \
    if ((a) < (b)) { \
        printf("FAIL\n      %s:%d: %s >= %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    } \
} while(0)

#define ASSERT_LT(a, b) do { \
    if ((a) >= (b)) { \
        printf("FAIL\n      %s:%d: %s < %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    } \
} while(0)

/* ---- Test definition ---- */
#define TEST(name) static int name(void)

/* ---- Suite macros ---- */
static int _tf_suite_total __attribute__((unused)) = 0;

#define SUITE_BEGIN(name) do { \
    _tf_suite_pass = 0; \
    _tf_suite_fail = 0; \
    _tf_suite_total = 0; \
    printf("\n  %s\n", name); \
    printf("  "); \
    for (int _i = 0; _i < (int)strlen(name); _i++) printf("-"); \
    printf("\n"); \
} while(0)

#define SUITE_END() do { \
    _tf_global_pass += _tf_suite_pass; \
    _tf_global_fail += _tf_suite_fail; \
    printf("\n  Result: %d/%d passed", _tf_suite_pass, _tf_suite_pass + _tf_suite_fail); \
    if (_tf_suite_fail > 0) printf(" (%d FAILED)", _tf_suite_fail); \
    printf("\n"); \
} while(0)

#define FAILED() (_tf_global_fail > 0)
#define TOTAL_PASS() (_tf_global_pass)
#define TOTAL_FAIL() (_tf_global_fail)
#define TOTAL_TESTS() (_tf_global_pass + _tf_global_fail)

#endif
