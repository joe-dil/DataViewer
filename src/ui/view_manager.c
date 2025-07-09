#include "view_manager.h"
#include "view_state.h"
#include "navigation.h" // For init_row_selection
#include "app_init.h"   // For init_view_state
#include "analysis.h"
#include "core/data_source.h"  // For DataSource access
#include "core/parser.h"
#include "util/logging.h"
#include <core/value_index.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "util/utils.h"

ViewManager* init_view_manager(void) {
    ViewManager *manager = calloc(1, sizeof(ViewManager));
    if (!manager) return NULL;
    manager->max_views = 10;
    return manager;
}

static void free_view_resources(View *view) {
    if (!view) return;
    
    // Cleanup selection state
    cleanup_row_selection(view);
    
    // Cleanup data source if owned
    if (view->data_source && view->owns_data_source) {
        destroy_data_source(view->data_source);
    }
    
    // Free other resources
    free(view->row_selected);
    free(view->ranges);
    free(view->row_order_map);
    free(view->visible_rows);  // For backward compatibility
    free_value_index(view->value_index);
    free(view->reverse_row_map);

    // Free the analysis cache
    if (view->analysis_cache) {
        for (size_t i = 0; i < view->analysis_cache_size; i++) {
            if (view->analysis_cache[i]) {
                free_value_index(view->analysis_cache[i]);
            }
        }
        free(view->analysis_cache);
    }
}

void cleanup_view_manager(ViewManager *manager) {
    if (!manager) return;
    View *current = manager->views;
    while (current) {
        View *next = current->next;
        free_view_resources(current);
        free(current);
        current = next;
    }
    free(manager);
}

View* create_main_view(DataSource *data_source) {
    View *main_view = calloc(1, sizeof(View));
    if (!main_view) return NULL;
    
    snprintf(main_view->name, sizeof(main_view->name), "Full Dataset");
    main_view->data_source = data_source;
    main_view->owns_data_source = false;  // Main view doesn't own file data source
    main_view->ranges = NULL;
    main_view->num_ranges = 0;
    main_view->sort_column = -1;
    main_view->last_sorted_column = -1;
    main_view->sort_direction = SORT_NONE;
    main_view->row_order_map = NULL;
    if (data_source && data_source->ops && data_source->ops->get_row_count) {
        main_view->visible_row_count = data_source->ops->get_row_count(data_source->context);
    } else {
        main_view->visible_row_count = 0;
    }
    
    // Initialize analysis cache
    if (data_source && data_source->ops) {
        main_view->analysis_cache_size = data_source->ops->get_col_count(data_source->context);
        main_view->analysis_cache = calloc(main_view->analysis_cache_size, sizeof(ValueIndex*));
    }
    // view_build_reverse_map(main_view); // Defer this until it's needed

    // Initialize cursor position
    main_view->cursor_row = 0;
    main_view->cursor_col = 0;
    main_view->start_row = 0;
    main_view->start_col = 0;
    
    // Note: Selection state is initialized separately by init_row_selection()
    
    return main_view;
}

void switch_to_next_view(ViewManager *manager, ViewState *state) {
    if (!manager || manager->view_count < 2) return;
    if (!manager->current) return; // Should not happen, but safe
    
    // State is now managed directly on the View object; no need to save here.
    
    manager->current = manager->current->next;
    if (!manager->current) {
        manager->current = manager->views;
    }
    reset_view_state_for_new_view(state, manager->current);
}

void switch_to_prev_view(ViewManager *manager, ViewState *state) {
    if (!manager || manager->view_count < 2) return;
    if (!manager->current) return;
    
    // State is now managed directly on the View object; no need to save here.
    
    if (manager->current->prev) {
        manager->current = manager->current->prev;
    } else {
        // Wrap around to the tail of the list
        View *tail = manager->views;
        while (tail && tail->next) {
            tail = tail->next;
        }
        manager->current = tail;
    }
    reset_view_state_for_new_view(state, manager->current);
}

static void renumber_views(ViewManager *manager) {
    if (!manager || !manager->views) return;
    
    View *v = manager->views;
    size_t i = 1;
    while(v) {
        if (i == 1) {
            snprintf(v->name, sizeof(v->name), "View 1 (Main)");
        } else {
            snprintf(v->name, sizeof(v->name), "View %zu (%zu rows)", i, v->visible_row_count);
        }
        v = v->next;
        i++;
    }
}

void close_current_view(ViewManager *manager, ViewState *state) {
    if (!manager || !manager->current || manager->view_count <= 1) {
        // Cannot close the main view
        return;
    }

    View *view_to_close = manager->current;

    // No need to save state; it's already on the view object.

    // Change current view to previous or next
    if (view_to_close->prev) {
        manager->current = view_to_close->prev;
        view_to_close->prev->next = view_to_close->next;
    } else {
        // This was the head, move to the new head
        manager->current = view_to_close->next;
    }

    if (view_to_close->next) {
        view_to_close->next->prev = view_to_close->prev;
    }

    // If we closed the head of the list, update the manager's head pointer
    if (manager->views == view_to_close) {
        manager->views = view_to_close->next;
        if (manager->views) {
            manager->views->prev = NULL;
        }
    }

    // Free resources
    free_view_resources(view_to_close);
    free(view_to_close);

    manager->view_count--;
    renumber_views(manager);
    reset_view_state_for_new_view(state, manager->current);
}

View* create_view_from_selection(ViewManager *manager, 
                               ViewState *state,
                               size_t *selected_rows, 
                               size_t count,
                               DataSource *parent_data_source) {
    if (!manager || manager->view_count >= manager->max_views || !selected_rows || count == 0) {
        // Optionally, set an error message to display to the user
        return NULL;
    }

    View *new_view = calloc(1, sizeof(View));
    if (!new_view) return NULL;

    // --- Range Compression Logic ---
    // This assumes selected_rows is sorted.
    if (count > 0) {
        // Over-allocate for simplicity, then realloc later if memory is a major concern.
        new_view->ranges = malloc(sizeof(RowRange) * count);
        if (!new_view->ranges) {
            free(new_view);
            return NULL;
        }
        
        new_view->num_ranges = 1;
        new_view->ranges[0].start = selected_rows[0];
        new_view->ranges[0].end = selected_rows[0];

        for (size_t i = 1; i < count; i++) {
            if (selected_rows[i] == new_view->ranges[new_view->num_ranges - 1].end + 1) {
                // Extend the current range
                new_view->ranges[new_view->num_ranges - 1].end = selected_rows[i];
            } else {
                // Start a new range
                new_view->num_ranges++;
                new_view->ranges[new_view->num_ranges - 1].start = selected_rows[i];
                new_view->ranges[new_view->num_ranges - 1].end = selected_rows[i];
            }
        }
    } else {
        new_view->ranges = NULL;
        new_view->num_ranges = 0;
    }
    // --- End Range Compression ---

    new_view->visible_row_count = count;
    new_view->data_source = parent_data_source;
    new_view->owns_data_source = false; // This view just filters the parent, doesn't own it.
    new_view->sort_column = -1;
    new_view->last_sorted_column = -1;
    new_view->sort_direction = SORT_NONE;
    new_view->row_order_map = NULL;

    // Initialize analysis cache
    if (parent_data_source && parent_data_source->ops) {
        new_view->analysis_cache_size = parent_data_source->ops->get_col_count(parent_data_source->context);
        new_view->analysis_cache = calloc(new_view->analysis_cache_size, sizeof(ValueIndex*));
    }
    // view_build_reverse_map(new_view); // Defer this until it's needed

    snprintf(new_view->name, sizeof(new_view->name), "View %zu (%zu rows)", manager->view_count + 1, count);

    // Initialize cursor position - inherit column from current view since it's the same data source
    new_view->cursor_row = 0;  // Reset row since indices are different
    new_view->cursor_col = state->current_view->cursor_col;  // Keep same column
    new_view->start_row = 0;
    new_view->start_col = state->current_view->start_col;  // Keep column viewport

    // Initialize selection state for the new view
    init_row_selection(new_view, count);

    // Insert into doubly linked list
    if (manager->current) {
        new_view->next = manager->current->next;
        new_view->prev = manager->current;
        if (manager->current->next) {
            manager->current->next->prev = new_view;
        }
        manager->current->next = new_view;
    } else {
        // Should not happen if a main view exists, but handle defensively
        manager->views = new_view;
    }
    
    manager->view_count++;

    // No need to save state before switching.

    // Switch to the new view
    manager->current = new_view;
    reset_view_state_for_new_view(state, manager->current);

    return new_view;
}

// When switching views, update the state to point to the new active view.
void reset_view_state_for_new_view(ViewState *state, View *new_view) {
    if (!state || !new_view) return;

    // The global state now just needs to point to the current view.
    // All cursor, scroll, and selection state is stored on the view itself.
    state->current_view = new_view;
    
    // Ensure the new view is validated, e.g., cursor within bounds.
    // This could be a separate function call if validation becomes complex.
    size_t col_count = 0;
    if (new_view->data_source && new_view->data_source->ops) {
        col_count = new_view->data_source->ops->get_col_count(new_view->data_source->context);
    }

    if (new_view->visible_row_count > 0 && new_view->cursor_row >= new_view->visible_row_count) {
        new_view->cursor_row = new_view->visible_row_count - 1;
    }
    if (col_count > 0 && new_view->cursor_col >= col_count) {
        new_view->cursor_col = col_count - 1;
    }
    
    state->needs_redraw = true;
}

bool add_view_to_manager(ViewManager *manager, View *view) {
    if (!manager || !view || manager->view_count >= manager->max_views) {
        return false;
    }

    // Insert into doubly linked list after current view
    if (manager->current) {
        view->next = manager->current->next;
        view->prev = manager->current;
        if (manager->current->next) {
            manager->current->next->prev = view;
        }
        manager->current->next = view;
    } else {
        // This is the first view
        manager->views = view;
    }

    manager->view_count++;
    return true;
}

size_t view_get_displayed_row_index(const View *view, size_t display_row) {
    if (!view) return SIZE_MAX;

    // If the view is sorted, use the map to find the permuted index.
    // Otherwise, the display order is the natural order.
    size_t visible_set_index = (view->row_order_map)
                             ? view->row_order_map[display_row]
                             : display_row;

    return view_get_actual_row_index(view, visible_set_index);
}

size_t view_get_actual_row_index(const View *view, size_t display_row) {
    if (!view) return SIZE_MAX;

    // If there are no ranges, the view is a direct 1:1 mapping
    if (view->num_ranges == 0) {
        // This assumes that if there are no ranges, all rows from the source are visible.
        size_t total_rows = view->data_source->ops->get_row_count(view->data_source->context);
        if (display_row < total_rows) {
            return display_row;
        } else {
            return SIZE_MAX; // Out of bounds
        }
    }
    
    size_t current_base = 0;
    for (size_t i = 0; i < view->num_ranges; i++) {
        size_t range_len = view->ranges[i].end - view->ranges[i].start + 1;
        if (display_row < current_base + range_len) {
            // The target row is in this range
            size_t offset = display_row - current_base;
            return view->ranges[i].start + offset;
        }
        current_base += range_len;
    }
    
    return SIZE_MAX; // display_row is out of bounds
} 

void view_build_reverse_map(View *view) {
    if (!view || !view->data_source) return;

    free(view->reverse_row_map); // Free existing map if any

    view->reverse_row_map_size = view->data_source->ops->get_row_count(view->data_source->context);
    if (view->reverse_row_map_size == 0) {
        view->reverse_row_map = NULL;
        return;
    }

    view->reverse_row_map = malloc(view->reverse_row_map_size * sizeof(size_t));
    if (!view->reverse_row_map) {
        view->reverse_row_map_size = 0;
        return; // Allocation failed
    }

    // Initialize with a sentinel value
    for (size_t i = 0; i < view->reverse_row_map_size; i++) {
        view->reverse_row_map[i] = SIZE_MAX;
    }

    // Populate the map
    for (size_t i = 0; i < view->visible_row_count; i++) {
        size_t actual_row = view_get_displayed_row_index(view, i);
        if (actual_row < view->reverse_row_map_size) {
            view->reverse_row_map[actual_row] = i;
        }
    }
}

void propagate_selection_to_parent(View *child_view) {
    if (!child_view || !child_view->parent || child_view->parent_source_column < 0) {
        return;
    }
    LOG_DEBUG("Propagating selection from child '%s' to parent '%s'", child_view->name, child_view->parent->name);

    View *parent_view = child_view->parent;
    DataSource *parent_ds = parent_view->data_source;
    DataSource *child_ds = child_view->data_source;

    // 1. Get the value from the selected cell in the child view's "Value" column (column 0)
    LOG_DEBUG("Child cursor row: %zu", child_view->cursor_row);
    FieldDesc child_fd = child_ds->ops->get_cell(child_ds->context, child_view->cursor_row, 0);
    if (child_fd.start == NULL) {
        LOG_WARN("Child cell is NULL, aborting propagation.");
        return;
    }
    char child_value[4096];
    render_field(&child_fd, child_value, sizeof(child_value));
    LOG_DEBUG("Child value: '%s'", child_value);

    // 2. Clear previous selections in the parent view
    if (parent_view->selection_count > 0) {
        LOG_DEBUG("Clearing %zu selections in parent view.", parent_view->selection_count);
        memset(parent_view->row_selected, 0, parent_view->total_rows * sizeof(bool));
        parent_view->selection_count = 0;
    }

    // 3. Iterate through the parent view and select matching rows
    char parent_value[4096];
    LOG_DEBUG("Iterating %zu visible rows in parent.", parent_view->visible_row_count);
    for (size_t i = 0; i < parent_view->visible_row_count; i++) {
        LOG_DEBUG("Processing parent row (display index): %zu", i);
        size_t actual_row = view_get_displayed_row_index(parent_view, i);
        if (actual_row == SIZE_MAX) {
            LOG_WARN("Could not get actual row for display row %zu", i);
            continue;
        }
        LOG_DEBUG("Actual parent row index: %zu", actual_row);

        FieldDesc parent_fd = parent_ds->ops->get_cell(parent_ds->context, actual_row, child_view->parent_source_column);
        if (parent_fd.start == NULL) {
            continue;
        }
        render_field(&parent_fd, parent_value, sizeof(parent_value));

        if (strcmp(child_value, parent_value) == 0) {
            LOG_DEBUG("Match found! Selecting row %zu in parent.", i);
            if (!parent_view->row_selected[i]) {
                parent_view->row_selected[i] = true;
                parent_view->selection_count++;
            }
        }
    }
    LOG_DEBUG("Propagation complete. Parent has %zu selections.", parent_view->selection_count);
} 

// A simple hash set for strings to quickly check for selected values
typedef struct {
    char **values;
    size_t size;
    size_t count;
} StringHashSet;

static StringHashSet* create_string_hash_set(size_t initial_size) {
    StringHashSet *set = calloc(1, sizeof(StringHashSet));
    if (!set) return NULL;
    set->size = initial_size > 0 ? initial_size : 16;
    set->values = calloc(set->size, sizeof(char*));
    if (!set->values) {
        free(set);
        return NULL;
    }
    return set;
}

static void free_string_hash_set(StringHashSet *set) {
    if (!set) return;
    for (size_t i = 0; i < set->size; i++) {
        free(set->values[i]);
    }
    free(set->values);
    free(set);
}

static void add_to_string_hash_set(StringHashSet *set, const char *value) {
    if (!set || !value) return;
    // Simple linear probing for collision resolution
    uint32_t hash = fnv1a_hash(value);
    size_t index = hash % set->size;
    while (set->values[index] != NULL) {
        if (strcmp(set->values[index], value) == 0) return; // Already exists
        index = (index + 1) % set->size;
    }
    set->values[index] = strdup(value);
    set->count++;
    // Note: resizing logic omitted for simplicity in this context
}

static bool string_hash_set_contains(const StringHashSet *set, const char *value) {
    if (!set || !value) return false;
    uint32_t hash = fnv1a_hash(value);
    size_t index = hash % set->size;
    while (set->values[index] != NULL) {
        if (strcmp(set->values[index], value) == 0) return true;
        index = (index + 1) % set->size;
    }
    return false;
}


void update_parent_selection_from_child(View *child_view) {
    if (!child_view || !child_view->parent || !child_view->value_index) {
        return;
    }

    View *parent_view = child_view->parent;
    
    // 1. Get all selected values from the child view
    size_t *selected_child_rows = NULL;
    size_t selected_count = get_selected_rows(child_view, &selected_child_rows);

    // 2. Clear parent's selection to rebuild it
    clear_all_selections(parent_view);

    if (selected_count == 0) {
        free(selected_child_rows);
        return; // No selection to propagate
    }

    // 3. Use the index to find and apply selections
    char buffer[4096];
    DataSource *child_ds = child_view->data_source;
    for (size_t i = 0; i < selected_count; i++) {
        FieldDesc fd = child_ds->ops->get_cell(child_ds->context, selected_child_rows[i], 0);
        if (fd.start) {
            render_field(&fd, buffer, sizeof(buffer));
            const RowIndexArray *rows = get_from_value_index(child_view->value_index, buffer);
            if (rows) {
                for (size_t j = 0; j < rows->count; j++) {
                    size_t actual_row = rows->indices[j];
                    if (actual_row < parent_view->reverse_row_map_size) {
                        size_t disp_row = parent_view->reverse_row_map[actual_row];
                        if (disp_row != SIZE_MAX && !parent_view->row_selected[disp_row]) {
                            toggle_row_selection(parent_view, disp_row);
                        }
                    }
                }
            }
        }
    }

    free(selected_child_rows);
} 