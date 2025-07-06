#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "view_state.h"

typedef struct View {
    ViewState state;
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
View* create_main_view(ViewState *initial_state, size_t total_rows);
void switch_to_next_view(ViewManager *manager);
void switch_to_prev_view(ViewManager *manager);
void close_current_view(ViewManager *manager);
View* create_view_from_selection(ViewManager *manager, const ViewState *source_state, size_t *selected_rows, size_t count);

#endif // VIEW_MANAGER_H 