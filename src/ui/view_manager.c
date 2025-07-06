#include "view_manager.h"
#include "navigation.h" // For init_row_selection
#include "app_init.h"   // For init_view_state
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
        free(current->visible_rows);
        cleanup_row_selection(&current->state);
        free(current);
        current = next;
    }
    free(manager);
}

View* create_main_view(ViewState *initial_state, size_t total_rows) {
    View *main_view = calloc(1, sizeof(View));
    if (!main_view) return NULL;
    
    memcpy(&main_view->state, initial_state, sizeof(ViewState));
    snprintf(main_view->name, sizeof(main_view->name), "View 1 (Main)");
    main_view->visible_rows = NULL; // Main view shows all rows
    main_view->visible_row_count = total_rows;
    main_view->state.table_view.total_rows = total_rows;
    
    // The row selection state is part of the initial_state, so it should be "moved"
    // rather than re-initialized, but let's make sure it's consistent.
    // The bitmap is copied by memcpy, so we need to make sure the original
    // initial_state doesn't free it.
    initial_state->table_view.row_selected = NULL;

    return main_view;
}

void switch_to_next_view(ViewManager *manager) {
    if (!manager || manager->view_count < 2) return;
    if (!manager->current) return; // Should not happen, but safe
    manager->current = manager->current->next;
    if (!manager->current) {
        manager->current = manager->views;
    }
}

void switch_to_prev_view(ViewManager *manager) {
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

void close_current_view(ViewManager *manager) {
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
    }

    // Free resources
    free(view_to_close->visible_rows);
    cleanup_row_selection(&view_to_close->state);
    free(view_to_close);

    manager->view_count--;
    renumber_views(manager);
}

View* create_view_from_selection(ViewManager *manager, 
                               size_t *selected_rows, 
                               size_t count) {
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

    // Initialize new view state
    init_view_state(&new_view->state);
    // This view has its own selection state, independent of the source
    init_row_selection(&new_view->state, count);
    new_view->state.table_view.total_rows = count;

    snprintf(new_view->name, sizeof(new_view->name), "View %zu (%zu rows)", manager->view_count + 1, count);

    // Insert into doubly linked list
    new_view->prev = manager->current;
    new_view->next = manager->current->next;
    if (manager->current->next) {
        manager->current->next->prev = new_view;
    }
    manager->current->next = new_view;
    
    manager->view_count++;
    manager->current = new_view;

    return new_view;
} 