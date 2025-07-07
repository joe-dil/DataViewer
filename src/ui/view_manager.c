#include "view_manager.h"
#include "navigation.h" // For init_row_selection
#include "app_init.h"   // For init_view_state
#include "analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ViewManager* init_view_manager(void) {
    ViewManager *manager = calloc(1, sizeof(ViewManager));
    if (!manager) return NULL;
    manager->max_views = 10;
    return manager;
}

void cleanup_view_manager(ViewManager *manager) {
    if (!manager) return;
    View *current = manager->views;
    while (current) {
        View *next = current->next;
        // State is now global, so no need to clean up freq results or selections here.
        if (current->data_source && current->owns_data_source) {
            destroy_data_source(current->data_source);
        }
        free(current->visible_rows);
        free(current);
        current = next;
    }
    free(manager);
}

View* create_main_view(size_t total_rows, DataSource *data_source) {
    View *main_view = calloc(1, sizeof(View));
    if (!main_view) return NULL;
    
    snprintf(main_view->name, sizeof(main_view->name), "Full Dataset");
    main_view->data_source = data_source;
    main_view->owns_data_source = false;  // Main view doesn't own file data source
    main_view->visible_rows = NULL; // Main view shows all rows
    main_view->visible_row_count = total_rows;
    
    return main_view;
}

void switch_to_next_view(ViewManager *manager, ViewState *state) {
    if (!manager || manager->view_count < 2) return;
    if (!manager->current) return; // Should not happen, but safe
    manager->current = manager->current->next;
    if (!manager->current) {
        manager->current = manager->views;
    }
    reset_view_state_for_new_view(state, manager->current);
}

void switch_to_prev_view(ViewManager *manager, ViewState *state) {
    if (!manager || manager->view_count < 2) return;
    if (!manager->current) return;
    
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
    if (view_to_close->data_source && view_to_close->owns_data_source) {
        destroy_data_source(view_to_close->data_source);
    }
    free(view_to_close->visible_rows);
    // Selections are not preserved per-view in this model, so no cleanup needed here.
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

    // Copy selected rows
    new_view->visible_rows = malloc(count * sizeof(size_t));
    if (!new_view->visible_rows) {
        free(new_view);
        return NULL;
    }
    memcpy(new_view->visible_rows, selected_rows, count * sizeof(size_t));
    new_view->visible_row_count = count;
    new_view->data_source = parent_data_source;
    new_view->owns_data_source = false; // This view just filters the parent, doesn't own it.

    snprintf(new_view->name, sizeof(new_view->name), "View %zu (%zu rows)", manager->view_count + 1, count);

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

    // Switch to the new view
    manager->current = new_view;
    reset_view_state_for_new_view(state, manager->current);

    return new_view;
}

// When switching views, reset the UI state to sensible defaults for the new view's data.
void reset_view_state_for_new_view(ViewState *state, View *new_view) {
    if (!state || !new_view) return;

    // Reset cursor and scroll positions
    state->table_view.cursor_row = 0;
    state->table_view.cursor_col = 0;
    state->table_view.table_start_row = 0;
    state->table_view.table_start_col = 0;

    // Update total rows to reflect the new view
    state->table_view.total_rows = new_view->visible_row_count;
    
    // Clear any existing selections
    clear_all_selections(state);
    
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