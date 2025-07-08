#include "core/sorting.h"
#include "core/data_source.h"
#include "core/parser.h"
#include "ui/view_manager.h"
#include "util/utils.h"
#include "util/logging.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// --- Sorting Helpers ---

// Helper to check if a string contains only digits (and an optional leading '-')
static bool is_string_numeric(const char *s) {
    if (!s || *s == '\0') return false;
    char *endptr;
    strtoll(s, &endptr, 10);
    // The string is numeric if the end pointer is at the null terminator
    return *endptr == '\0';
}

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


// Context structure to pass to the comparison function
typedef struct {
    View *view;
    char *buffer_a;
    char *buffer_b;
    size_t buffer_size;
    bool is_numeric;
} SortContext;

#ifdef __APPLE__
// macOS/BSD version of the comparison function for qsort_r
static int compare_rows_apple(void *context, const void *a, const void *b) {
    SortContext *ctx = (SortContext*)context;
    View *view = ctx->view;
    DataSource *ds = view->data_source;

    size_t index_a = *(const size_t *)a;
    size_t index_b = *(const size_t *)b;

    // Translate from visible set index to actual data source row index
    size_t actual_row_a = view_get_actual_row_index(view, index_a);
    size_t actual_row_b = view_get_actual_row_index(view, index_b);

    if (actual_row_a == SIZE_MAX || actual_row_b == SIZE_MAX) {
        return 0; // Should not happen
    }

    // Get cell data as strings
    FieldDesc fd_a = ds->ops->get_cell(ds->context, actual_row_a, view->sort_column);
    render_field(&fd_a, ctx->buffer_a, ctx->buffer_size);

    FieldDesc fd_b = ds->ops->get_cell(ds->context, actual_row_b, view->sort_column);
    render_field(&fd_b, ctx->buffer_b, ctx->buffer_size);

    int result;
    if (ctx->is_numeric) {
        long long val_a = strtoll(ctx->buffer_a, NULL, 10);
        long long val_b = strtoll(ctx->buffer_b, NULL, 10);
        if (val_a < val_b) result = -1;
        else if (val_a > val_b) result = 1;
        else result = 0;
    } else {
        result = strcmp(ctx->buffer_a, ctx->buffer_b);
    }

    if (view->sort_direction == SORT_DESC) {
        return -result;
    }
    return result;
}
#else
// Standard Linux/glibc version of the comparison function
static int compare_rows(const void *a, const void *b, void *context) {
    SortContext *ctx = (SortContext*)context;
    View *view = ctx->view;
    DataSource *ds = view->data_source;

    size_t index_a = *(const size_t *)a;
    size_t index_b = *(const size_t *)b;

    // Translate from visible set index to actual data source row index
    size_t actual_row_a = view_get_actual_row_index(view, index_a);
    size_t actual_row_b = view_get_actual_row_index(view, index_b);

    if (actual_row_a == SIZE_MAX || actual_row_b == SIZE_MAX) {
        return 0; // Should not happen
    }

    // Get cell data as strings
    FieldDesc fd_a = ds->ops->get_cell(ds->context, actual_row_a, view->sort_column);
    render_field(&fd_a, ctx->buffer_a, ctx->buffer_size);

    FieldDesc fd_b = ds->ops->get_cell(ds->context, actual_row_b, view->sort_column);
    render_field(&fd_b, ctx->buffer_b, ctx->buffer_size);

    int result;
    if (ctx->is_numeric) {
        long long val_a = strtoll(ctx->buffer_a, NULL, 10);
        long long val_b = strtoll(ctx->buffer_b, NULL, 10);
        if (val_a < val_b) result = -1;
        else if (val_a > val_b) result = 1;
        else result = 0;
    } else {
        result = strcmp(ctx->buffer_a, ctx->buffer_b);
    }

    if (view->sort_direction == SORT_DESC) {
        return -result;
    }
    return result;
}
#endif

void sort_view(View *view) {
    if (!view || !view->data_source) return;

    // If sorting is turned off, free the map and return
    if (view->sort_direction == SORT_NONE) {
        SAFE_FREE(view->row_order_map);
        return;
    }

    if (view->visible_row_count == 0) return;

    // Allocate or re-allocate the map
    if (!view->row_order_map) {
        view->row_order_map = malloc(view->visible_row_count * sizeof(size_t));
        if (!view->row_order_map) {
            LOG_ERROR("Failed to allocate row_order_map for sorting.");
            return;
        }
    }

    // Initialize map with identity mapping (0, 1, 2, ...)
    for (size_t i = 0; i < view->visible_row_count; i++) {
        view->row_order_map[i] = i;
    }

    // Prepare context for the comparison function
    SortContext context;
    context.view = view;
    context.is_numeric = is_column_numeric(view, view->sort_column);
    // These buffers are used to avoid repeated mallocs inside the comparator
    context.buffer_size = 4096; // A reasonable max field size for comparison
    context.buffer_a = malloc(context.buffer_size);
    context.buffer_b = malloc(context.buffer_size);

    if (!context.buffer_a || !context.buffer_b) {
        LOG_ERROR("Failed to allocate sort buffers.");
        free(context.buffer_a);
        free(context.buffer_b);
        // Don't sort, leave the map as identity
        return;
    }

    // Use the correct version of qsort_r based on the platform
#ifdef __APPLE__
    qsort_r(view->row_order_map, view->visible_row_count, sizeof(size_t), &context, compare_rows_apple);
#else
    qsort_r(view->row_order_map, view->visible_row_count, sizeof(size_t), compare_rows, &context);
#endif
    
    // Cleanup
    free(context.buffer_a);
    free(context.buffer_b);
} 