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

#endif // NAVIGATION_H 