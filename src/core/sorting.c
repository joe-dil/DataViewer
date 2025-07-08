#include "core/sorting.h"
#include "core/data_source.h"
#include "core/parser.h"
#include "ui/view_manager.h"
#include "util/utils.h"
#include "util/logging.h"
#include "app_init.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// --- Data Structures for Sorting ---

// Represents a pre-computed sort key for a single row.
// This is the "decorate" part of the Schwartzian Transform.
typedef union {
    long long numeric_key;
    char* string_key;
} SortKey;

// Struct to hold the decorated row information
typedef struct {
    SortKey key;
    bool is_numeric;
    size_t original_index; // Original index within the visible set
} DecoratedRow;


// --- Sorting Helpers ---

// Heuristic to check if a column is numeric by sampling its contents
static bool is_column_numeric(View *view, int column_index) {
    if (!view || view->visible_row_count == 0) return false;

    DataSource *ds = view->data_source;
    // Sample up to 100 rows to make a decision
    size_t sample_size = view->visible_row_count < 100 ? view->visible_row_count : 100;
    char buffer[4096];

    for (size_t i = 0; i < sample_size; i++) {
        size_t actual_row_index = view_get_displayed_row_index(view, i);
        if (actual_row_index == SIZE_MAX) continue;

        FieldDesc fd = ds->ops->get_cell(ds->context, actual_row_index, column_index);
        if (fd.start == NULL || fd.length == 0) continue; // Skip empty cells

        render_field(&fd, buffer, sizeof(buffer));
        
        if (!is_string_numeric(buffer)) {
            return false; // Found a non-numeric value
        }
    }
    
    return true; // All sampled values were numeric
}


// Unified comparison function for qsort
static int compare_decorated_rows(const void *a, const void *b) {
    const DecoratedRow *row_a = (const DecoratedRow *)a;
    const DecoratedRow *row_b = (const DecoratedRow *)b;
    
    int result = 0;
    
    if (row_a->is_numeric) {
        // Numeric comparison
        if (row_a->key.numeric_key < row_b->key.numeric_key) {
            result = -1;
        } else if (row_a->key.numeric_key > row_b->key.numeric_key) {
            result = 1;
        } else {
            result = 0;
        }
    } else {
        // String comparison
        result = strcmp(row_a->key.string_key, row_b->key.string_key);
    }
    
    // If primary keys are equal, use original_index as stable tie-breaker
    if (result == 0) {
        if (row_a->original_index < row_b->original_index) {
            result = -1;
        } else if (row_a->original_index > row_b->original_index) {
            result = 1;
        }
    }
    
    return result;
}

bool is_column_sorted(View *view, int column_index, SortDirection direction) {
    if (!view || view->visible_row_count <= 1) return true;

    bool is_numeric = is_column_numeric(view, column_index);
    char buffer_a[4096], buffer_b[4096];
    DataSource *ds = view->data_source;

    for (size_t i = 0; i < view->visible_row_count - 1; i++) {
        size_t actual_row_a = view_get_displayed_row_index(view, i);
        size_t actual_row_b = view_get_displayed_row_index(view, i + 1);

        FieldDesc fd_a = ds->ops->get_cell(ds->context, actual_row_a, column_index);
        FieldDesc fd_b = ds->ops->get_cell(ds->context, actual_row_b, column_index);

        render_field(&fd_a, buffer_a, sizeof(buffer_a));
        render_field(&fd_b, buffer_b, sizeof(buffer_b));

        int comparison_result;
        if (is_numeric) {
            long long val_a = strtoll(buffer_a, NULL, 10);
            long long val_b = strtoll(buffer_b, NULL, 10);
            if (val_a < val_b) comparison_result = -1;
            else if (val_a > val_b) comparison_result = 1;
            else comparison_result = 0;
        } else {
            comparison_result = strcmp(buffer_a, buffer_b);
        }

        if (direction == SORT_ASC && comparison_result > 0) return false;
        if (direction == SORT_DESC && comparison_result < 0) return false;
    }

    return true;
}


void sort_view(View *view) {
    if (!view || !view->data_source) return;

    // Determine the new sort direction for the current sort_column
    if (view->sort_column == view->last_sorted_column) {
        // Same column as last sort, cycle through states based on actual sort
        if (view->sort_direction == SORT_ASC && is_column_sorted(view, view->sort_column, SORT_ASC)) {
            view->sort_direction = SORT_DESC;
        } else if (view->sort_direction == SORT_DESC && is_column_sorted(view, view->sort_column, SORT_DESC)) {
            view->sort_direction = SORT_NONE;
            view->sort_column = -1; // Reset sort column when sorting is off
        } else {
            // If it's the same column but not currently sorted as expected, or was SORT_NONE, default to ASC
            view->sort_direction = SORT_ASC;
        }
    } else {
        // New column being sorted, default to ASC
        view->sort_direction = SORT_ASC;
    }

    view->last_sorted_column = view->sort_column; // Update last sorted column

    // If sorting is turned off, free the map and return
    if (view->sort_direction == SORT_NONE) {
        SAFE_FREE(view->row_order_map);
        return;
    }

    if (view->visible_row_count == 0) return;

    // --- 1. DECORATE ---
    // Create an array of decorated rows with pre-computed sort keys.

    DecoratedRow *decorated_rows = malloc(view->visible_row_count * sizeof(DecoratedRow));
    if (!decorated_rows) {
        LOG_ERROR("Failed to allocate for decorated rows.");
        return;
    }

    bool is_numeric = is_column_numeric(view, view->sort_column);
    char *string_key_arena = NULL;
    size_t arena_offset = 0;

    if (!is_numeric) {
        // Pre-allocate a single large block for all string keys for performance
        // Average 32 bytes per key as a heuristic.
        size_t arena_size = view->visible_row_count * 32;
        string_key_arena = malloc(arena_size);
        if (!string_key_arena) {
            LOG_ERROR("Failed to allocate string arena for sorting.");
            free(decorated_rows);
            return;
        }
    }
    
    char render_buffer[4096];
    DataSource *ds = view->data_source;

    for (size_t i = 0; i < view->visible_row_count; i++) {
        decorated_rows[i].original_index = i;
        decorated_rows[i].is_numeric = is_numeric;
        size_t actual_row = view_get_actual_row_index(view, i);

        FieldDesc fd = ds->ops->get_cell(ds->context, actual_row, view->sort_column);
        render_field(&fd, render_buffer, sizeof(render_buffer));

        if (is_numeric) {
            decorated_rows[i].key.numeric_key = strtoll(render_buffer, NULL, 10);
        } else {
            size_t len = strlen(render_buffer) + 1;
            // For now, we just crash if the arena is too small. A real implementation
            // might need to realloc.
            if (string_key_arena && arena_offset + len <= view->visible_row_count * 32) {
                char* dest = string_key_arena + arena_offset;
                memcpy(dest, render_buffer, len);
                decorated_rows[i].key.string_key = dest;
                arena_offset += len;
            } else {
                 decorated_rows[i].key.string_key = ""; // Arena full
            }
        }
    }

    // --- 2. SORT ---
    // Sort the decorated array using the unified comparison function
    qsort(decorated_rows, view->visible_row_count, sizeof(DecoratedRow), compare_decorated_rows);

    // Handle descending sort by reversing the sorted array
    if (view->sort_direction == SORT_DESC) {
        for (size_t i = 0; i < view->visible_row_count / 2; i++) {
            DecoratedRow temp = decorated_rows[i];
            decorated_rows[i] = decorated_rows[view->visible_row_count - 1 - i];
            decorated_rows[view->visible_row_count - 1 - i] = temp;
        }
    }


    // --- 3. UNDECORATE ---
    // Create the final row_order_map from the sorted original indices.
    
    // Allocate or re-allocate the map
    if (!view->row_order_map) {
        view->row_order_map = malloc(view->visible_row_count * sizeof(size_t));
    }
    if (!view->row_order_map) {
        LOG_ERROR("Failed to allocate row_order_map for sorting.");
        free(decorated_rows);
        free(string_key_arena);
        return;
    }

    for (size_t i = 0; i < view->visible_row_count; i++) {
        view->row_order_map[i] = decorated_rows[i].original_index;
    }
    
    // Cleanup
    free(decorated_rows);
    free(string_key_arena);
} 