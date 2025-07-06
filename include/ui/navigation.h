#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "view_state.h"

// Forward declaration
struct DSVViewer;

void navigate_up(ViewState *state);
void navigate_down(ViewState *state, const struct DSVViewer *viewer);
void navigate_left(ViewState *state);
void navigate_right(ViewState *state, const struct DSVViewer *viewer);
void navigate_page_up(ViewState *state, const struct DSVViewer *viewer);
void navigate_page_down(ViewState *state, const struct DSVViewer *viewer);
void navigate_home(ViewState *state);
void navigate_end(ViewState *state, const struct DSVViewer *viewer);

// Row selection functions
void init_row_selection(ViewState *state, size_t total_rows);
void toggle_row_selection(ViewState *state, size_t row);
bool is_row_selected(const ViewState *state, size_t row);
void clear_all_selections(ViewState *state);
size_t get_selected_rows(const ViewState *state, size_t **rows);
void cleanup_row_selection(ViewState *state);
void select_range(ViewState *state, size_t from, size_t to);
void begin_selection_mode(ViewState *state, size_t anchor);
void update_selection_mode(ViewState *state, size_t current);

#endif // NAVIGATION_H 