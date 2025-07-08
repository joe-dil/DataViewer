#include "../framework/test_runner.h"
#include "core/sorting.h"
#include "core/data_source.h"
#include "ui/view_manager.h"
#include <string.h>
#include <stdlib.h>

// Mock implementations for dependencies
FieldDesc mock_get_cell(void *context, size_t row, size_t col) {
    (void)col; // This mock only handles one column
    char ***data = (char ***)context;
    return (FieldDesc){ .start = data[row][0], .length = strlen(data[row][0]) };
}

size_t mock_get_row_count(void *context) {
    (void)context; // Unused for this mock
    return 3;
}

size_t mock_get_col_count(void *context) {
    (void)context; // Unused for this mock
    return 1;
}

// --- Test Cases ---
void test_string_sorting_ascending(void);
void test_string_sorting_descending(void);

void test_string_sorting_ascending() {
    // Test data
    char *row1[] = { "banana" };
    char *row2[] = { "apple" };
    char *row3[] = { "cherry" };
    char **data[] = { row1, row2, row3 };

    // Setup DataSource
    DataSource ds = {
        .context = data,
        .ops = &(DataSourceOps){
            .get_cell = mock_get_cell,
            .get_row_count = mock_get_row_count,
            .get_col_count = mock_get_col_count
        }
    };

    // Setup View
    View view = {
        .data_source = &ds,
        .visible_row_count = 3,
        .sort_column = 0,
        .sort_direction = SORT_ASC,
        .last_sorted_column = -1,
        .row_order_map = NULL
    };

    // Perform sort
    sort_view(&view);

    // Assertions
    ASSERT_NOT_NULL(view.row_order_map);
    ASSERT_EQ(view.row_order_map[0], 1); // apple
    ASSERT_EQ(view.row_order_map[1], 0); // banana
    ASSERT_EQ(view.row_order_map[2], 2); // cherry

    // Cleanup
    free(view.row_order_map);
}

void test_string_sorting_descending() {
    // Test data
    char *row1[] = { "banana" };
    char *row2[] = { "apple" };
    char *row3[] = { "cherry" };
    char **data[] = { row1, row2, row3 };

    // Setup DataSource
    DataSource ds = {
        .context = data,
        .ops = &(DataSourceOps){
            .get_cell = mock_get_cell,
            .get_row_count = mock_get_row_count,
            .get_col_count = mock_get_col_count
        }
    };

    // Setup View
    View view = {
        .data_source = &ds,
        .visible_row_count = 3,
        .sort_column = 0,
        .sort_direction = SORT_DESC,
        .last_sorted_column = -1,
        .row_order_map = NULL
    };

    // Perform sort
    sort_view(&view);

    // Assertions
    ASSERT_NOT_NULL(view.row_order_map);
    ASSERT_EQ(view.row_order_map[0], 2); // cherry
    ASSERT_EQ(view.row_order_map[1], 0); // banana
    ASSERT_EQ(view.row_order_map[2], 1); // apple

    // Cleanup
    free(view.row_order_map);
}

// --- Test Suite Definition ---
TestCase sorting_tests[] = {
    {"String Sorting (Ascending)", test_string_sorting_ascending},
    {"String Sorting (Descending)", test_string_sorting_descending},
};
int sorting_suite_size = sizeof(sorting_tests) / sizeof(TestCase);