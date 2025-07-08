#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "view_state.h"

// Forward declaration
struct DSVViewer;
struct View;

void navigate_up(ViewState *state);
void navigate_down(ViewState *state, const struct DSVViewer *viewer);
void navigate_left(ViewState *state);
void navigate_right(ViewState *state, const struct DSVViewer *viewer);
void navigate_page_up(ViewState *state, const struct DSVViewer *viewer);
void navigate_page_down(ViewState *state, const struct DSVViewer *viewer);
void navigate_home(ViewState *state);
void navigate_end(ViewState *state, const struct DSVViewer *viewer);

// Row selection functions - now work with View instead of ViewState
void init_row_selection(struct View *view, size_t total_rows);
void toggle_row_selection(struct View *view, size_t row);
bool is_row_selected(const struct View *view, size_t row);
void clear_all_selections(struct View *view);
size_t get_selected_rows(const struct View *view, size_t **rows);
void cleanup_row_selection(struct View *view);

#endif // NAVIGATION_H 