#include "../framework/test_runner.h"
#include "util/utils.h"

// --- Test Cases ---

void test_is_string_numeric_positive(void) {
    ASSERT_EQ(is_string_numeric("123"), true);
    ASSERT_EQ(is_string_numeric("  456  "), true);
    ASSERT_EQ(is_string_numeric("-789"), true);
}

void test_is_string_numeric_negative(void) {
    ASSERT_EQ(is_string_numeric("abc"), false);
    ASSERT_EQ(is_string_numeric("123a"), false);
    ASSERT_EQ(is_string_numeric("a123"), false);
    ASSERT_EQ(is_string_numeric("12 34"), false);
    ASSERT_EQ(is_string_numeric(""), false);
    ASSERT_EQ(is_string_numeric("  "), false);
}

// --- Test Suite ---

TestCase utils_tests[] = {
    {"Is String Numeric (Positive)", test_is_string_numeric_positive},
    {"Is String Numeric (Negative)", test_is_string_numeric_negative},
};

int utils_suite_size = sizeof(utils_tests) / sizeof(TestCase);
