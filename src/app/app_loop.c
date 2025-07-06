#include "app_init.h"
#include "input_router.h"
#include "view_state.h"
#include <ncurses.h>
#include <stdbool.h>

void init_view_state(ViewState *state) {
    state->current_panel = PANEL_TABLE_VIEW;
    state->needs_redraw = true;
    state->table_view.table_start_row = 0;
    state->table_view.table_start_col = 0;
    // Note: cursor_row will need to be initialized after we know if headers are shown
    // For now, default to 0, but the main program should adjust this
    state->table_view.cursor_row = 0;
    state->table_view.cursor_col = 0;
}

void run_viewer(DSVViewer *viewer) {
    ViewState view_state;
    init_view_state(&view_state);
    
    // Force initial draw
    view_state.needs_redraw = true;

    while (1) {
        // Only redraw when needed
        if (view_state.needs_redraw) {
            display_data(viewer, 
                        view_state.table_view.table_start_row, 
                        view_state.table_view.table_start_col,
                        view_state.table_view.cursor_row,
                        view_state.table_view.cursor_col);
            view_state.needs_redraw = false;
        }

        int ch = getch();
        
        // Handle mouse events and other special cases that can cause busy loops
        if (ch == ERR || ch == KEY_MOUSE || ch == KEY_RESIZE) {
            continue; // Skip processing, just continue the main loop
        }
        
        // Route input through the new input router
        InputResult result = route_input(ch, viewer, &view_state);
        
        switch (result) {
            case INPUT_CONSUMED:
                // Panel handled the input, redraw flag already set if needed
                break;
                
            case INPUT_IGNORED:
                // Panel didn't recognize the input, could log or handle specially
                break;
                
            case INPUT_GLOBAL: {
                // Handle global commands
                GlobalResult global_result = handle_global_input(ch, &view_state);
                switch (global_result) {
                    case GLOBAL_QUIT:
                        return;  // Exit the viewer
                        
                    case GLOBAL_SHOW_HELP:
                        show_help();
                        view_state.needs_redraw = true;  // Redraw after modal
                        break;
                        
                    case GLOBAL_SWITCH_PANEL:
                        // Future: Handle panel switching
                        // For now, just force redraw
                        view_state.needs_redraw = true;
                        break;
                        
                    case GLOBAL_CONTINUE:
                        // Should not happen when result is INPUT_GLOBAL
                        break;
                }
                break;
            }
        }
    }
} 