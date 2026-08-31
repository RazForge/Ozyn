/*
 * test_main.c — OZAYN Core Test Runner.
 *
 * Runs all unit, integration, and system tests.
 * Exit code: 0 = all pass, 1 = any failure.
 */

#include "test_framework.h"
#include <stdio.h>

/* Unit test declarations */
extern int run_events_tests(void);
extern int run_dependency_tests(void);
extern int run_lifecycle_tests(void);
extern int run_security_tests(void);
extern int run_reload_tests(void);

/* Integration test declarations */
extern int run_startup_shutdown_tests(void);
extern int run_service_integration_tests(void);

/* System test declarations */
extern int run_full_lifecycle_tests(void);

int main(void) {
    int total_pass = 0;
    int total_fail = 0;
    int suite_pass, suite_fail;

    printf("\n");
    printf("  ======================================================\n");
    printf("  OZAYN CORE TEST SUITE\n");
    printf("  Version: 0.1 (Genesis)\n");
    printf("  ======================================================\n");

    /* Unit tests */
    printf("\n  --- UNIT TESTS ---");
    suite_fail = run_events_tests();    suite_pass = 11 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;
    suite_fail = run_dependency_tests(); suite_pass = 11 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;
    suite_fail = run_lifecycle_tests();  suite_pass = 9 - suite_fail;  total_pass += suite_pass; total_fail += suite_fail;
    suite_fail = run_security_tests();   suite_pass = 16 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;
    suite_fail = run_reload_tests();     suite_pass = 10 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;

    /* Integration tests */
    printf("\n  --- INTEGRATION TESTS ---");
    suite_fail = run_startup_shutdown_tests();    suite_pass = 6 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;
    suite_fail = run_service_integration_tests(); suite_pass = 6 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;

    /* System tests */
    printf("\n  --- SYSTEM TESTS ---");
    suite_fail = run_full_lifecycle_tests();      suite_pass = 5 - suite_fail; total_pass += suite_pass; total_fail += suite_fail;

    /* Summary */
    int total_tests = total_pass + total_fail;
    printf("\n  ======================================================\n");
    printf("  TOTAL: %d/%d passed", total_pass, total_tests);
    if (total_fail > 0) {
        printf(" (%d FAILED)", total_fail);
    } else {
        printf(" -- ALL PASS");
    }
    printf("\n  ======================================================\n\n");

    return total_fail > 0 ? 1 : 0;
}
