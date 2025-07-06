#ifndef INPUT_ROUTER_H
#define INPUT_ROUTER_H

#include "view_state.h"

// Forward declaration to avoid circular dependencies
struct DSVViewer;

// Input router function declarations

// Route input to appropriate handler
InputResult route_input(int ch, struct DSVViewer *viewer, ViewState *state);

// Handle global commands (quit, help, tab switching)
GlobalResult handle_global_input(int ch, ViewState *state);

// Handle table-specific navigation
InputResult handle_table_input(int ch, struct DSVViewer *viewer, ViewState *state);

#endif // INPUT_ROUTER_H 