#include "constants.h"
#include "input_router.h"
#include "viewer.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for copy functionality
static char* get_field_at_cursor(DSVViewer *viewer, size_t cursor_row, size_t cursor_col);
static void copy_to_clipboard_with_status(DSVViewer *viewer, const char *text);

// Helper function to get the field value at the current cursor position
static char* get_field_at_cursor(DSVViewer *viewer, size_t cursor_row, size_t cursor_col) {
    if (!viewer || !viewer->file_data || !viewer->file_data->data) {
        return NULL;
    }
    
    // Calculate actual file line (accounting for header)
    size_t file_line = cursor_row;
    if (viewer->display_state->show_header) {
        file_line++; // Skip header line in file
    }
    
    // Check bounds
    if (file_line >= viewer->file_data->num_lines) {
        return NULL;
    }
    
    // Parse the line to get fields
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[file_line], 
                                  viewer->file_data->fields, viewer->config->max_cols);
    
    // Check if cursor_col is within bounds
    if (cursor_col >= num_fields) {
        return NULL;
    }
    
    // Render the field to a static buffer
    static char field_buffer[DEFAULT_MAX_FIELD_LEN];
    render_field(&viewer->file_data->fields[cursor_col], field_buffer, sizeof(field_buffer));
    
    return field_buffer;
}

// Copy text to clipboard and update status - enhanced version
static void copy_to_clipboard_with_status(DSVViewer *viewer, const char *text) {
    if (!text || !viewer || !viewer->display_state) return;
    
    const char *cmd = NULL;
    
    #ifdef __APPLE__
        cmd = "pbcopy";
    #elif __linux__
        // Try xclip first, fall back to xsel
        if (system("which xclip > /dev/null 2>&1") == 0) {
            cmd = "xclip -selection clipboard";
        } else if (system("which xsel > /dev/null 2>&1") == 0) {
            cmd = "xsel --clipboard --input";
        }
    #endif
    
    if (!cmd) {
        // No clipboard command available
        snprintf(viewer->display_state->copy_status, sizeof(viewer->display_state->copy_status),
                 "Clipboard not available on this system");
        viewer->display_state->show_copy_status = 1;
        return;
    }
    
    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        snprintf(viewer->display_state->copy_status, sizeof(viewer->display_state->copy_status),
                 "Failed to access clipboard");
        viewer->display_state->show_copy_status = 1;
        return;
    }
    
    fprintf(pipe, "%s", text);
    int result = pclose(pipe);
    
    if (result == 0) {
        // Success - show what was copied (truncated if too long)
        if (strlen(text) > 50) {
            snprintf(viewer->display_state->copy_status, sizeof(viewer->display_state->copy_status),
                     "Copied: %.47s...", text);
        } else {
            snprintf(viewer->display_state->copy_status, sizeof(viewer->display_state->copy_status),
                     "Copied: %s", text);
        }
    } else {
        snprintf(viewer->display_state->copy_status, sizeof(viewer->display_state->copy_status),
                 "Copy failed");
    }
    
    viewer->display_state->show_copy_status = 1;
}

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
    
    // Calculate the actual number of data rows (excluding header if present)
    size_t data_rows = viewer->file_data->num_lines;
    if (viewer->display_state->show_header && viewer->file_data->num_lines > 0) {
        data_rows--; // Subtract header row from total lines
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
            // Use data_rows for boundary check
            if (state->table_view.cursor_row + 1 < data_rows) {
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
            // Use data_rows for boundary check
            if (state->table_view.table_start_row >= data_rows) {
                state->table_view.table_start_row = (data_rows > (size_t)visible_rows) 
                    ? data_rows - visible_rows : 0;
            }
            // Move cursor to stay visible
            if (state->table_view.cursor_row < state->table_view.table_start_row) {
                state->table_view.cursor_row = state->table_view.table_start_row;
            }
            // Ensure cursor doesn't go past the last data row
            if (state->table_view.cursor_row >= data_rows && data_rows > 0) {
                state->table_view.cursor_row = data_rows - 1;
            }
            break;
            
        case KEY_HOME:
            state->table_view.cursor_row = 0;
            state->table_view.cursor_col = 0;
            state->table_view.table_start_row = 0;
            state->table_view.table_start_col = 0;
            break;
            
        case KEY_END:
            // Use data_rows for setting cursor to last row
            state->table_view.cursor_row = (data_rows > 0) ? data_rows - 1 : 0;
            state->table_view.cursor_col = (viewer->display_state->num_cols > 0)
                ? viewer->display_state->num_cols - 1 : 0;
            
            // Scroll viewport to show the cursor at the end
            if (data_rows > (size_t)visible_rows) {
                state->table_view.table_start_row = data_rows - visible_rows;
            } else {
                state->table_view.table_start_row = 0;
            }
            
            // Find optimal horizontal position to show last column
            if (viewer->display_state->num_cols > 0) {
                state->table_view.table_start_col = find_start_col_for_target(
                    viewer, state->table_view.cursor_col, cols);
            }
            break;
            
        case 'y':  // Copy current cell to clipboard
            {
                // Get the field value at cursor position
                char *field_value = get_field_at_cursor(viewer, 
                                                       state->table_view.cursor_row, 
                                                       state->table_view.cursor_col);
                if (field_value) {
                    copy_to_clipboard_with_status(viewer, field_value);
                    // Need to redraw to show the status message
                    state->needs_redraw = true;
                }
                return INPUT_CONSUMED;
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