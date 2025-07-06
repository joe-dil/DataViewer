#include "constants.h"
#include "input_router.h"
#include "viewer.h"
#include <ncurses.h>

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
    // Track old state for change detection
    size_t old_row = state->table_view.table_start_row;
    size_t old_col = state->table_view.table_start_col;
    size_t old_cursor_row = state->table_view.cursor_row;
    size_t old_cursor_col = state->table_view.cursor_col;
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int visible_rows = rows - 1; // Minus status line
    
    // If headers are shown, we lose one more row for display
    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    switch (ch) {
        case KEY_UP:
            if (state->table_view.cursor_row > 0) {
                state->table_view.cursor_row--;
                // Scroll up if cursor moves above viewport
                if (state->table_view.cursor_row < state->table_view.table_start_row) {
                    state->table_view.table_start_row = state->table_view.cursor_row;
                }
            }
            break;
            
        case KEY_DOWN:
            if (state->table_view.cursor_row + 1 < viewer->file_data->num_lines) {
                state->table_view.cursor_row++;
                // Scroll down if cursor moves below viewport
                if (state->table_view.cursor_row >= state->table_view.table_start_row + visible_rows) {
                    state->table_view.table_start_row = state->table_view.cursor_row - visible_rows + 1;
                }
            }
            break;
            
        case KEY_LEFT:
            if (state->table_view.cursor_col > 0) {
                state->table_view.cursor_col--;
                // Scroll left if cursor moves before viewport
                if (state->table_view.cursor_col < state->table_view.table_start_col) {
                    state->table_view.table_start_col = state->table_view.cursor_col;
                }
            }
            break;
            
        case KEY_RIGHT:
            if (state->table_view.cursor_col + 1 < viewer->display_state->num_cols) {
                // First check if the last column is already fully visible
                int x = 0;
                bool last_column_visible = false;
                
                for (size_t col = state->table_view.table_start_col; col < viewer->display_state->num_cols; col++) {
                    int col_width = viewer->display_state->col_widths[col];
                    
                    // Check if this is the last column
                    if (col == viewer->display_state->num_cols - 1) {
                        if (x + col_width + SEPARATOR_WIDTH <= cols) {
                            last_column_visible = true;
                        }
                        break;
                    }
                    
                    x += col_width + SEPARATOR_WIDTH;
                    if (x >= cols) break;
                }
                
                // If last column is visible, only allow cursor movement within visible columns
                if (last_column_visible) {
                    // Count how many columns are actually visible
                    size_t last_visible_col = state->table_view.table_start_col;
                    x = 0;
                    for (size_t col = state->table_view.table_start_col; col < viewer->display_state->num_cols; col++) {
                        int col_width = viewer->display_state->col_widths[col];
                        if (x + col_width + SEPARATOR_WIDTH <= cols) {
                            last_visible_col = col;
                            x += col_width + SEPARATOR_WIDTH;
                        } else {
                            break;
                        }
                    }
                    
                    // Only move if cursor would stay in visible area
                    if (state->table_view.cursor_col < last_visible_col) {
                        state->table_view.cursor_col++;
                    }
                } else {
                    // Last column not visible, allow normal movement with scrolling
                    state->table_view.cursor_col++;
                    if (state->table_view.cursor_col >= state->table_view.table_start_col + 5) {
                        // Before scrolling, check if it would make sense
                        size_t new_start = state->table_view.table_start_col + 1;
                        
                        // Check if scrolling would still show content
                        x = 0;
                        bool would_show_content = false;
                        for (size_t col = new_start; col < viewer->display_state->num_cols; col++) {
                            int col_width = viewer->display_state->col_widths[col];
                            if (x + col_width <= cols) {
                                would_show_content = true;
                                break;
                            }
                            x += col_width + SEPARATOR_WIDTH;
                        }
                        
                        if (would_show_content) {
                            state->table_view.table_start_col++;
                        }
                    }
                }
            }
            break;
            
        case KEY_PPAGE:
            // Move viewport up by one page
            if (state->table_view.table_start_row > (size_t)visible_rows) {
                state->table_view.table_start_row -= visible_rows;
            } else {
                state->table_view.table_start_row = 0;
            }
            // Move cursor to stay visible
            if (state->table_view.cursor_row >= state->table_view.table_start_row + visible_rows) {
                state->table_view.cursor_row = state->table_view.table_start_row + visible_rows - 1;
            }
            break;
            
        case KEY_NPAGE:
            // Move viewport down by one page
            state->table_view.table_start_row += visible_rows;
            if (state->table_view.table_start_row >= viewer->file_data->num_lines) {
                state->table_view.table_start_row = (viewer->file_data->num_lines > (size_t)visible_rows) 
                    ? viewer->file_data->num_lines - visible_rows : 0;
            }
            // Move cursor to stay visible
            if (state->table_view.cursor_row < state->table_view.table_start_row) {
                state->table_view.cursor_row = state->table_view.table_start_row;
            }
            break;
            
        case KEY_HOME:
            state->table_view.cursor_row = 0;
            state->table_view.cursor_col = 0;
            state->table_view.table_start_row = 0;
            state->table_view.table_start_col = 0;
            break;
            
        case KEY_END:
            state->table_view.cursor_row = (viewer->file_data->num_lines > 0) 
                ? viewer->file_data->num_lines - 1 : 0;
            state->table_view.cursor_col = (viewer->display_state->num_cols > 0)
                ? viewer->display_state->num_cols - 1 : 0;
            // Scroll viewport to show the cursor at the end
            if (viewer->file_data->num_lines > (size_t)visible_rows) {
                state->table_view.table_start_row = viewer->file_data->num_lines - visible_rows;
            } else {
                state->table_view.table_start_row = 0;
            }
            break;
            
        default:
            // Unknown keys are ignored by table view
            return INPUT_IGNORED;
    }

    // Check if state changed (viewport or cursor)
    if (state->table_view.table_start_row != old_row || 
        state->table_view.table_start_col != old_col ||
        state->table_view.cursor_row != old_cursor_row ||
        state->table_view.cursor_col != old_cursor_col) {
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