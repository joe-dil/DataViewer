#ifndef VIEW_STATE_H
#define VIEW_STATE_H

#include <stddef.h>
#include <stdbool.h>
#include "analysis.h"
#include "memory/in_memory_table.h"

// Forward declaration to avoid circular dependencies
struct DSVViewer;
struct View;

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
    PANEL_FREQ_ANALYSIS,
    // Future: PANEL_COLUMN_STATS, PANEL_ROW_DETAILS
} PanelType;

// View state that tracks current panel and redraw needs
typedef struct {
    PanelType current_panel;
    bool needs_redraw;
    struct View *current_view; // The view whose data is being displayed
    
    // Panel-specific state has been moved to the View struct
    // to support per-view cursor positions, scroll offsets, and selections.
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