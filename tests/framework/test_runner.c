#include "test_runner.h"
#include <string.h>

// --- Global Test State ---
int tests_run = 0;
int tests_passed = 0;
int current_test_failed = 0;

// --- Test Runner Implementation ---

void run_test(TestCase test) {
    tests_run++;
    current_test_failed = 0; // Reset failure flag for the current test

    printf(TEST_COLOR_YELLOW "  Running: %s" TEST_COLOR_RESET "\n", test.name);

    // Execute the actual test function
    test.func();

    if (current_test_failed) {
        // The TEST_ASSERT macro handles the specific failure message
    } else {
        printf(TEST_COLOR_GREEN "    ✓ PASSED" TEST_COLOR_RESET "\n");
        tests_passed++;
    }
}

void run_test_suite(TestCase tests[], int count) {
    printf("Running %d test(s)...\n", count);
    for (int i = 0; i < count; i++) {
        run_test(tests[i]);
    }
    print_test_summary();
}

void print_test_summary(void) {
    printf("\n--- Test Summary ---\n");
    printf("Tests Run:    %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    
    if (tests_run > tests_passed) {
        printf(TEST_COLOR_RED "Tests Failed: %d\n" TEST_COLOR_RESET, tests_run - tests_passed);
    } else {
        printf(TEST_COLOR_GREEN "All tests passed!\n" TEST_COLOR_RESET);
    }
    printf("--------------------\n");
} 