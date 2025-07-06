#include "constants.h"
#include "input_router.h"
#include "viewer.h"
#include <ncurses.h>
#include <stdbool.h>

// Helper function to check if a column is fully visible in the current viewport
static bool is_column_fully_visible(DSVViewer *viewer, size_t start_col, size_t target_col, int screen_width) {
    if (target_col < start_col) {
        return false;  // Column is before viewport
    }
    
    int x = 0;
    for (size_t col = start_col; col <= target_col; col++) {
        if (col > start_col) {
            x += SEPARATOR_WIDTH;  // Add separator before this column
        }
        
        int col_width = viewer->display_state->col_widths[col];
        
        if (col == target_col) {
            // Check if the entire column fits
            return (x + col_width <= screen_width);
        }
        
        x += col_width;
        if (x >= screen_width) {
            return false;  // Already past screen edge
        }
    }
    
    return false;
}

// Helper function to find the leftmost start column that shows the target column fully
static size_t find_start_col_for_target(DSVViewer *viewer, size_t target_col, int screen_width) {
    // First, try showing from column 0
    if (is_column_fully_visible(viewer, 0, target_col, screen_width)) {
        return 0;
    }
    
    // Binary search for the optimal start column
    size_t left = 0;
    size_t right = target_col;
    size_t best_start = target_col;  // Worst case: show only the target column
    
    while (left <= right && right < viewer->display_state->num_cols) {
        size_t mid = left + (right - left) / 2;
        
        if (is_column_fully_visible(viewer, mid, target_col, screen_width)) {
            best_start = mid;
            // Try to show more columns by starting earlier
            if (mid > 0) {
                right = mid - 1;
            } else {
                break;
            }
        } else {
            // Need to start later
            left = mid + 1;
        }
    }
    
    return best_start;
}

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
                // Move cursor right
                state->table_view.cursor_col++;
                
                // Check if cursor column is still fully visible
                if (!is_column_fully_visible(viewer, state->table_view.table_start_col, 
                                            state->table_view.cursor_col, cols)) {
                    // Find optimal start column to show cursor
                    state->table_view.table_start_col = find_start_col_for_target(
                        viewer, state->table_view.cursor_col, cols);
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
            
            // Find optimal horizontal position to show last column
            if (viewer->display_state->num_cols > 0) {
                state->table_view.table_start_col = find_start_col_for_target(
                    viewer, state->table_view.cursor_col, cols);
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