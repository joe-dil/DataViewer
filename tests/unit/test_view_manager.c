#include "framework/test_runner.h"
#include "ui/view_manager.h"
#include "core/data_source.h"
#include "memory/in_memory_table.h"
#include "ui/navigation.h"
#include <string.h>
#include <stdlib.h>

// Helper to create a basic view from an in-memory table
static View* create_test_view(const char* name, const char** headers, const char*** data, int rows, int cols) {
    InMemoryTable* table = create_in_memory_table(name, cols, headers);
    for (int i = 0; i < rows; i++) {
        add_in_memory_table_row(table, data[i]);
    }

    DataSource* ds = create_memory_data_source(table);
    View* view = calloc(1, sizeof(View));
    strncpy(view->name, name, sizeof(view->name) - 1);
    view->data_source = ds;
    view->owns_data_source = true;
    view->visible_row_count = ds->ops->get_row_count(ds->context);
    view->total_rows = view->visible_row_count;

    init_row_selection(view, view->total_rows);

    // For simplicity, make all rows visible initially in a single range
    view->ranges = malloc(sizeof(RowRange));
    view->num_ranges = 1;
    view->ranges[0] = (RowRange){ .start = 0, .end = view->visible_row_count > 0 ? view->visible_row_count - 1 : 0 };

    // Initialize the row_order_map for identity mapping (no sorting)
    view->row_order_map = malloc(view->visible_row_count * sizeof(size_t));
    for (size_t i = 0; i < view->visible_row_count; i++) {
        view->row_order_map[i] = i;
    }

    return view;
}

static void teardown_test_view(View* view) {
    if (!view) return;
    if (view->owns_data_source) {
        destroy_data_source(view->data_source);
    }
    free(view->row_selected);
    free(view->ranges);
    free(view->row_order_map);
    free(view);
}

static void setup_view_manager_tests(void) {
    // No-op for now
}

static void teardown_view_manager_tests(void) {
    // No-op for now
}

void propagate_selection(void) {
    // 1. Create Parent View
    const char* parent_headers[] = {"ID", "Color", "Value"};
    const char* parent_data[][3] = {
        {"1", "Red", "A"},
        {"2", "Blue", "B"},
        {"3", "Red", "C"},
        {"4", "Green", "A"},
        {"5", "Blue", "D"}
    };
    View* parent_view = create_test_view("Parent", parent_headers, (const char***)parent_data, 5, 3);

    // 2. Create Child View (e.g., frequency of "Color" column)
    const char* child_headers[] = {"Value", "Count"};
    const char* child_data[][2] = {
        {"Red", "2"},
        {"Blue", "2"},
        {"Green", "1"}
    };
    View* child_view = create_test_view("Child", child_headers, (const char***)child_data, 3, 2);

    // 3. Link them
    child_view->parent = parent_view;
    child_view->parent_source_column = 1; // "Color" column

    // 4. Set cursor on child and propagate
    child_view->cursor_row = 1; // Select "Blue"
    propagate_selection_to_parent(child_view);

    // 5. Assertions
    TEST_ASSERT(parent_view->selection_count == 2, "Selection count should be 2 for 'Blue'");
    TEST_ASSERT(!parent_view->row_selected[0], "Row 0 (Red) should not be selected");
    TEST_ASSERT(parent_view->row_selected[1], "Row 1 (Blue) should be selected");
    TEST_ASSERT(!parent_view->row_selected[2], "Row 2 (Red) should not be selected");
    TEST_ASSERT(!parent_view->row_selected[3], "Row 3 (Green) should not be selected");
    TEST_ASSERT(parent_view->row_selected[4], "Row 4 (Blue) should be selected");

    // 6. Propagate a different selection
    child_view->cursor_row = 0; // Select "Red"
    propagate_selection_to_parent(child_view);

    TEST_ASSERT(parent_view->selection_count == 2, "Selection count should be 2 for 'Red'");
    TEST_ASSERT(parent_view->row_selected[0], "Row 0 (Red) should be selected");
    TEST_ASSERT(!parent_view->row_selected[1], "Row 1 (Blue) should not be selected");
    TEST_ASSERT(parent_view->row_selected[2], "Row 2 (Red) should be selected");
    TEST_ASSERT(!parent_view->row_selected[3], "Row 3 (Green) should not be selected");
    TEST_ASSERT(!parent_view->row_selected[4], "Row 4 (Blue) should not be selected");

    // 7. Cleanup
    teardown_test_view(parent_view);
    teardown_test_view(child_view);
}

void propagate_selection_null_case(void) {
    propagate_selection_to_parent(NULL);
    TEST_ASSERT(true, "Executing with NULL should not crash");
}

// --- Test Suite ---

TestCase view_manager_tests[] = {
    {"Propagate Selection", propagate_selection},
    {"Propagate Selection with NULL", propagate_selection_null_case},
};

int view_manager_suite_size = sizeof(view_manager_tests) / sizeof(TestCase); 