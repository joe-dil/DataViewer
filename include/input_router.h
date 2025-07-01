#ifndef INPUT_ROUTER_H
#define INPUT_ROUTER_H

#include <stddef.h>
#include <stdbool.h>

// Forward declaration to avoid circular dependencies
struct DSVViewer;

// Input handling result - tells the main loop what to do next
typedef enum {
    INPUT_CONSUMED,    // Panel handled it, redraw if needed
    INPUT_IGNORED,     // Invalid for this panel, try global commands
    INPUT_GLOBAL       // Global command (Tab, Quit, Help)
} InputResult;

// Panel types for future extensibility
typedef enum {
    PANEL_TABLE_VIEW,
    PANEL_HELP,
    // Future: PANEL_COLUMN_STATS, PANEL_ROW_DETAILS
} PanelType;

// View state that tracks current panel and redraw needs
typedef struct {
    PanelType current_panel;
    bool needs_redraw;
    
    // Panel-specific state
    struct {
        size_t table_start_row;
        size_t table_start_col;
    } table_view;
    
    // Future panel states can be added here
} ViewState;

// Global command results
typedef enum {
    GLOBAL_CONTINUE,     // Continue with current panel
    GLOBAL_SWITCH_PANEL, // Switch to different panel
    GLOBAL_QUIT,         // Exit application
    GLOBAL_SHOW_HELP     // Show help modal
} GlobalResult;

// Input router function declarations

// Initialize view state
void init_view_state(ViewState *state);

// Route input to appropriate handler
InputResult route_input(int ch, struct DSVViewer *viewer, ViewState *state);

// Handle global commands (quit, help, tab switching)
GlobalResult handle_global_input(int ch, ViewState *state);

// Handle table-specific navigation
InputResult handle_table_input(int ch, struct DSVViewer *viewer, ViewState *state);

#endif // INPUT_ROUTER_H 