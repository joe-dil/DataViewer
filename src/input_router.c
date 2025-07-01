#include "constants.h"
#include "input_router.h"
#include "viewer.h"
#include <ncurses.h>

void init_view_state(ViewState *state) {
    state->current_panel = PANEL_TABLE_VIEW;
    state->needs_redraw = true;
    state->table_view.table_start_row = 0;
    state->table_view.table_start_col = 0;
}

GlobalResult handle_global_input(int ch, ViewState *state) {
    (void)state; // Suppress unused parameter warning for now
    
    switch (ch) {
        case 'q':
            return GLOBAL_QUIT;
        case 'h':
        case 'H':
            return GLOBAL_SHOW_HELP;
        // Future: Tab key for panel switching
        // case '\t':
        //     return GLOBAL_SWITCH_PANEL;
        default:
            return GLOBAL_CONTINUE;
    }
}

InputResult handle_table_input(int ch, struct DSVViewer *viewer, ViewState *state) {
    size_t old_row = state->table_view.table_start_row;
    size_t old_col = state->table_view.table_start_col;
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int page_size = rows - 1;

    switch (ch) {
        case KEY_UP:
            if (state->table_view.table_start_row > 0) {
                state->table_view.table_start_row--;
            }
            break;
            
        case KEY_DOWN:
            if (state->table_view.table_start_row + 1 < viewer->file_data->num_lines) {
                state->table_view.table_start_row++;
            }
            break;
            
        case KEY_LEFT:
            if (state->table_view.table_start_col > 0) {
                state->table_view.table_start_col--;
            }
            break;
            
        case KEY_RIGHT:
            // Stop scrolling if the last column is already fully visible
            if (state->table_view.table_start_col < viewer->display_state->num_cols - 1) {
                // Check if the last column is already fully visible from current position
                int x = 0;
                bool last_column_fully_visible = false;
                
                for (size_t col = state->table_view.table_start_col; col < viewer->display_state->num_cols; col++) {
                    int col_width = viewer->display_state->col_widths[col];
                    
                    // If this is the last column, check if it fits completely (including final separator)
                    if (col == viewer->display_state->num_cols - 1) {
                        if (x + col_width + SEPARATOR_WIDTH <= cols) { // Column + final separator space
                            last_column_fully_visible = true;
                        }
                        break;
                    }
                    
                    // For non-last columns, just advance position
                    x += col_width + SEPARATOR_WIDTH;
                    if (x >= cols) break; // No point checking further if we're already past screen
                }
                
                // Only allow scrolling if the last column is not yet fully visible
                if (!last_column_fully_visible) {
                    state->table_view.table_start_col++;
                }
            }
            break;
            
        case KEY_PPAGE:
            state->table_view.table_start_row = (state->table_view.table_start_row > (size_t)page_size) 
                ? state->table_view.table_start_row - page_size : 0;
            break;
            
        case KEY_NPAGE:
            state->table_view.table_start_row += page_size;
            if (state->table_view.table_start_row >= viewer->file_data->num_lines) {
                state->table_view.table_start_row = (viewer->file_data->num_lines > 0) 
                    ? viewer->file_data->num_lines - 1 : 0;
            }
            break;
            
        case KEY_HOME:
            state->table_view.table_start_row = 0;
            state->table_view.table_start_col = 0;
            break;
            
        case KEY_END:
            state->table_view.table_start_row = (viewer->file_data->num_lines > 0) 
                ? viewer->file_data->num_lines - 1 : 0;
            break;
            
        default:
            // Unknown keys are ignored by table view
            return INPUT_IGNORED;
    }

    // Check if state changed
    if (state->table_view.table_start_row != old_row || state->table_view.table_start_col != old_col) {
        state->needs_redraw = true;
        return INPUT_CONSUMED;
    }
    
    // No change, but we recognized the key
    return INPUT_CONSUMED;
}

InputResult route_input(int ch, struct DSVViewer *viewer, ViewState *state) {
    // First, check for global commands
    GlobalResult global_result = handle_global_input(ch, state);
    if (global_result != GLOBAL_CONTINUE) {
        return INPUT_GLOBAL;
    }
    
    // Route to current panel handler
    switch (state->current_panel) {
        case PANEL_TABLE_VIEW:
            return handle_table_input(ch, viewer, state);
            
        case PANEL_HELP:
            // Help panel would handle its own input here
            // For now, just return ignored
            return INPUT_IGNORED;
            
        default:
            return INPUT_IGNORED;
    }
} 