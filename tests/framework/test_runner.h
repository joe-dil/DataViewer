#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdio.h>

// --- Color Codes for Terminal Output ---
#define TEST_COLOR_GREEN   "\x1b[32m"
#define TEST_COLOR_RED     "\x1b[31m"
#define TEST_COLOR_YELLOW  "\x1b[33m"
#define TEST_COLOR_RESET   "\x1b[0m"

// --- Test Case Definition ---

// A single test case function pointer
typedef void (*test_func_ptr)(void);

// Struct to hold a test case's name and function
typedef struct {
    const char* name;
    test_func_ptr func;
} TestCase;

// --- Core Test Macros ---

// Internal variables for tracking test state
extern int tests_run;
extern int tests_passed;
extern int current_test_failed;

// TEST_ASSERT: The core assertion macro.
// If the condition is false, it prints a failure message and sets a flag.
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("%s    ✗ FAILED: %s%s\n", TEST_COLOR_RED, TEST_COLOR_RESET, message); \
            current_test_failed = 1; \
        } \
    } while (0)

// Additional assertion macros for convenience
#define ASSERT_EQ(actual, expected) \
    TEST_ASSERT((actual) == (expected), "Expected values to be equal")

#define ASSERT_NE(actual, expected) \
    TEST_ASSERT((actual) != (expected), "Expected values to be different")

#define ASSERT_GT(actual, threshold) \
    TEST_ASSERT((actual) > (threshold), "Expected value to be greater than threshold")

#define ASSERT_LT(actual, threshold) \
    TEST_ASSERT((actual) < (threshold), "Expected value to be less than threshold")

#define ASSERT_NULL(ptr) \
    TEST_ASSERT((ptr) == NULL, "Expected pointer to be NULL")

#define ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT((ptr) != NULL, "Expected pointer to be non-NULL")

// --- Test Runner Functions ---

// Run a single test case by name
void run_test(TestCase test);

// Run a suite of tests
void run_test_suite(TestCase tests[], int count);

// Print the final summary of all tests run
void print_test_summary(void);

#endif // TEST_RUNNER_H 