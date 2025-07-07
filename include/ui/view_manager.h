#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "view_state.h"

typedef struct View {
    char name[64];
    size_t *visible_rows;       // NULL for main view
    size_t visible_row_count;   // Same as total for main view
    struct View *next;
    struct View *prev;
} View;

typedef struct {
    View *views;          // Head of doubly-linked list
    View *current;
    size_t view_count;
    size_t max_views;     // Set to 10
} ViewManager;

ViewManager* init_view_manager(void);
void cleanup_view_manager(ViewManager *manager);
View* create_main_view(size_t total_rows);
void switch_to_next_view(ViewManager *manager, ViewState *state);
void switch_to_prev_view(ViewManager *manager, ViewState *state);
void close_current_view(ViewManager *manager, ViewState *state);
View* create_view_from_selection(ViewManager *manager, ViewState *state, size_t *selected_rows, size_t count);
void reset_view_state_for_new_view(ViewState *state, View *new_view);

#endif // VIEW_MANAGER_H 