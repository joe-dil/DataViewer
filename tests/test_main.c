#include "framework/test_runner.h"

// --- Test Suite Declarations ---

// Each test suite is defined in its own file (e.g., test_display.c)
// and provides an array of TestCase structs and its size.
extern TestCase display_tests[];
extern int display_suite_size;

extern TestCase memory_tests[];
extern int memory_suite_size;

extern TestCase config_tests[];
extern int config_suite_size;

// --- Main Test Runner ---

int main(void) {
    printf("========== Running Unit Test Suites ==========\n");
    
    // Run all the different test suites
    run_test_suite(display_tests, display_suite_size);
    run_test_suite(memory_tests, memory_suite_size);
    run_test_suite(config_tests, config_suite_size);

    printf("============================================\n");

    // The final summary will be printed by the last call to run_test_suite
    // You could also have a separate final summary call if you prefer.
    
    // Exit with a status code indicating failure if any tests failed
    if (tests_run > tests_passed) {
        return 1;
    }
    
    return 0;
} 