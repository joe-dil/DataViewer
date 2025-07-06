#include "constants.h"
#include "input_router.h"
#include "app_init.h"
#include "navigation.h"
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
    if (file_line >= viewer->parsed_data->num_lines) {
        return NULL;
    }
    
    // Parse the line to get fields
    size_t num_fields = parse_line(viewer->file_data->data, viewer->file_data->length, viewer->parsed_data->delimiter, viewer->parsed_data->line_offsets[file_line], 
                                  viewer->parsed_data->fields, viewer->config->max_cols);
    
    // Check if cursor_col is within bounds
    if (cursor_col >= num_fields) {
        return NULL;
    }
    
    // Render the field to a static buffer
    static char field_buffer[DEFAULT_MAX_FIELD_LEN];
    render_field(&viewer->parsed_data->fields[cursor_col], field_buffer, sizeof(field_buffer));
    
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
    
    switch (ch) {
        case KEY_UP:
            navigate_up(state);
            break;
        case KEY_DOWN:
            navigate_down(state, viewer);
            break;
        case KEY_LEFT:
            navigate_left(state);
            break;
        case KEY_RIGHT:
            navigate_right(state, viewer);
            break;
        case KEY_PPAGE:
            navigate_page_up(state, viewer);
            break;
        case KEY_NPAGE:
            navigate_page_down(state, viewer);
            break;
        case KEY_HOME:
            navigate_home(state);
            break;
        case KEY_END:
            navigate_end(state, viewer);
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