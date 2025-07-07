#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "view_state.h"
#include "core/data_source.h"

typedef struct View {
    char name[64];
    DataSource *data_source;      // NEW: Data source for this view
    bool owns_data_source;        // NEW: If true, free on view cleanup
    size_t *visible_rows;         // NULL = show all rows, array = show subset
    size_t visible_row_count;     // Number of rows in this view
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
View* create_main_view(size_t total_rows, DataSource *data_source);
void switch_to_next_view(ViewManager *manager, ViewState *state);
void switch_to_prev_view(ViewManager *manager, ViewState *state);
void close_current_view(ViewManager *manager, ViewState *state);
View* create_view_from_selection(ViewManager *manager, ViewState *state, 
                                 size_t *selected_rows, size_t count,
                                 DataSource *parent_data_source);
void reset_view_state_for_new_view(ViewState *state, View *new_view);
bool add_view_to_manager(ViewManager *manager, View *view);

#endif // VIEW_MANAGER_H 