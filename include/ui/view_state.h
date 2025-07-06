#ifndef VIEW_STATE_H
#define VIEW_STATE_H

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
        size_t cursor_row;      // Current cursor row position in data
        size_t cursor_col;      // Current cursor column position in data
        bool *row_selected;        // Bitmap for row selection
        size_t selection_count;    // Number of selected rows
        size_t total_rows;         // Total rows in view (for bounds checking)
        size_t selection_anchor;   // Anchor point for shift-selection
        bool in_selection_mode;    // Currently doing shift-selection
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

// Initialize view state
void init_view_state(ViewState *state);

#endif // VIEW_STATE_H 