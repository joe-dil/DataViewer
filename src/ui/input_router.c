#include "constants.h"
#include "input_router.h"
#include "app_init.h"
#include "navigation.h"
#include "analysis.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory/in_memory_table.h"
#include "core/sorting.h"
#include "core/search.h"

// Forward declarations for copy functionality
static char* get_field_at_cursor(const ViewState *state);
static void copy_to_clipboard_with_status(DSVViewer *viewer, const char *text);
static InputResult handle_search_input(int ch, struct DSVViewer *viewer, ViewState *state);

// Helper function to get the field value at the current cursor position
static char* get_field_at_cursor(const ViewState *state) {
    if (!state || !state->current_view || !state->current_view->data_source) {
        return NULL;
    }

    DataSource *ds = state->current_view->data_source;
    size_t row = state->current_view->cursor_row;
    size_t col = state->current_view->cursor_col;

    // Handle filtered and sorted views by translating display row to actual data source row
    size_t actual_row = view_get_displayed_row_index(state->current_view, row);
    if (actual_row == SIZE_MAX) {
        return NULL; // Invalid display row
    }

    FieldDesc fd = ds->ops->get_cell(ds->context, actual_row, col);

    if (fd.start == NULL) {
        return NULL;
    }

    static char field_buffer[DEFAULT_MAX_FIELD_LEN];
    render_field(&fd, field_buffer, sizeof(field_buffer));

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
    size_t old_row = state->current_view->start_row;
    size_t old_col = state->current_view->start_col;
    size_t old_cursor_row = state->current_view->cursor_row;
    size_t old_cursor_col = state->current_view->cursor_col;
    
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
        case 'F': // Shift+F
            if (state->current_view) {
                char col_name[256];
                DataSource *current_ds = state->current_view->data_source;
                FieldDesc header_fd = current_ds->ops->get_header(current_ds->context, state->current_view->cursor_col);

                if (header_fd.start) {
                    render_field(&header_fd, col_name, sizeof(col_name));
                } else {
                    snprintf(col_name, sizeof(col_name), "Column %zu", state->current_view->cursor_col + 1);
                }
                
                // Perform analysis
                InMemoryTable* table = perform_frequency_analysis(viewer, 
                                                                 state->current_view, 
                                                                 state->current_view->cursor_col);
                if (table) {
                    // Create data source for the table
                    DataSource *ds = create_memory_data_source(table);
                    if (!ds) {
                        free_in_memory_table(table);
                        set_error_message(viewer, "Failed to create data source for frequency analysis");
                        return INPUT_CONSUMED;
                    }
                    
                    // Create new view
                    View *freq_view = calloc(1, sizeof(View));
                    if (!freq_view) {
                        destroy_data_source(ds);
                        set_error_message(viewer, "Failed to allocate memory for frequency view");
                        return INPUT_CONSUMED;
                    }
                    
                    snprintf(freq_view->name, sizeof(freq_view->name), 
                             "Freq: %s", col_name);
                    freq_view->data_source = ds;
                    freq_view->owns_data_source = true;
                    freq_view->visible_rows = NULL;
                    freq_view->visible_row_count = ds->ops->get_row_count(ds->context);
                    
                    // Initialize cursor position
                    freq_view->cursor_row = 0;
                    freq_view->cursor_col = 0;
                    freq_view->start_row = 0;
                    freq_view->start_col = 0;
                    
                    // Initialize selection state for frequency view
                    init_row_selection(freq_view, freq_view->visible_row_count);
                    
                    // Add to view manager
                    if (!add_view_to_manager(viewer->view_manager, freq_view)) {
                        free(freq_view);
                        destroy_data_source(ds);
                        set_error_message(viewer, "Maximum number of views reached (%zu)", 
                                        viewer->view_manager->max_views);
                        return INPUT_CONSUMED;
                    }
                    
                    // Switch to new view
                    viewer->view_manager->current = freq_view;
                    reset_view_state_for_new_view(state, freq_view);
                    state->current_view = freq_view;
                    state->needs_redraw = true;
                } else {
                    set_error_message(viewer, "Frequency analysis failed - column may be empty");
                }
            }
            return INPUT_CONSUMED;
        case ' ':
            if (state->current_view) {
                toggle_row_selection(state->current_view, state->current_view->cursor_row);
                state->needs_redraw = true;
            }
            return INPUT_CONSUMED;
        case 'A':
        case 27: // ESC key
            if (state->current_view && state->current_view->selection_count > 0) {
                clear_all_selections(state->current_view);
                state->needs_redraw = true;
            }
            // If no selection, ESC does nothing here, might be handled globally later.
            return INPUT_CONSUMED;
        case 'v':
            if (state->current_view && state->current_view->selection_count > 0) {
                size_t *rows = NULL;
                size_t count = get_selected_rows(state->current_view, &rows);
                if (count > 0 && state->current_view) {
                    View *new_view = create_view_from_selection(viewer->view_manager, state, rows, count, state->current_view->data_source);
                    if (!new_view) {
                        set_error_message(viewer, "Failed to create view - maximum views reached or out of memory");
                    } else {
                        // Per instructions, clear selection after creating a view
                        clear_all_selections(state->current_view);
                        state->needs_redraw = true;
                    }
                    // The create function copies the rows, so we must free them here.
                    free(rows);
                }
            } else {
                set_error_message(viewer, "No rows selected - use Space to select rows");
            }
            return INPUT_CONSUMED;
        case '\t': // Tab
            switch_to_next_view(viewer->view_manager, state);
            state->needs_redraw = true;
            return INPUT_CONSUMED;
        case KEY_BTAB: // Shift+Tab
            switch_to_prev_view(viewer->view_manager, state);
            state->needs_redraw = true;
            return INPUT_CONSUMED;
        case 'x':
            if (viewer->view_manager->view_count > 1) {
                close_current_view(viewer->view_manager, state);
                // The state pointer is now invalid, but we are just setting a flag
                // on the new current view. The state is fetched again at the
                // start of the loop in app_loop.c
                state->needs_redraw = true;
            } else {
                set_error_message(viewer, "Cannot close the main view");
            }
            return INPUT_CONSUMED;
        case ']': // Cycle sort for the current column
            if (state->current_view) {
                View *view = state->current_view;
                view->sort_column = view->cursor_col;
                sort_view(view);
                state->needs_redraw = true;
            }
            return INPUT_CONSUMED;
        case 'n': // Find next search result
            if (state->search_term[0] != '\0') {
                SearchResult result = search_view(viewer, state->current_view, state->search_term, false);
                if (result == SEARCH_WRAPPED_AND_FOUND) {
                    snprintf(state->search_message, sizeof(state->search_message), "| Found: %s - search wrapped", state->search_term);
                } else if (result == SEARCH_FOUND) {
                    snprintf(state->search_message, sizeof(state->search_message), "| Found: %s", state->search_term);
                }
                state->needs_redraw = true;
            } else {
                set_error_message(viewer, "No active search term");
            }
            return INPUT_CONSUMED;
        case '/':
            state->input_mode = INPUT_MODE_SEARCH;
            // Don't clear the search term, so user can edit the previous one
            state->needs_redraw = true;
            return INPUT_CONSUMED;
        case 'y':  // Copy current cell to clipboard
            {
                // Get the field value at cursor position
                char *field_value = get_field_at_cursor(state);
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
    if (state->current_view->start_row != old_row || 
        state->current_view->start_col != old_col ||
        state->current_view->cursor_row != old_cursor_row ||
        state->current_view->cursor_col != old_cursor_col) {
        state->needs_redraw = true;
        state->search_message[0] = '\0'; // Clear search message on any navigation
        return INPUT_CONSUMED;
    }
    
    // No change, but we recognized the key
    return INPUT_CONSUMED;
}

// --- Input Handlers per Panel ---

InputResult route_input(int ch, struct DSVViewer *viewer, ViewState *state) {
    // Check for special input modes first
    if (state->input_mode == INPUT_MODE_SEARCH) {
        return handle_search_input(ch, viewer, state);
    }

    // First, check for global commands
    GlobalResult global_result = handle_global_input(ch, state);
    if (global_result != GLOBAL_CONTINUE) {
        return INPUT_GLOBAL;
    }
    
    // Route to current panel handler
    switch (state->current_panel) {
        case PANEL_TABLE_VIEW:
            return handle_table_input(ch, viewer, state);
            
        case PANEL_FREQ_ANALYSIS:
            // This panel is being deprecated and will be removed.
            // For now, any input will just switch back to the table view.
            state->current_panel = PANEL_TABLE_VIEW;
            state->needs_redraw = true;
            return INPUT_CONSUMED;

        case PANEL_HELP:
            // Help panel would handle its own input here
            // For now, just return ignored
            return INPUT_IGNORED;
            
        default:
            return INPUT_IGNORED;
    }
}

// --- Search Input Handler ---

static InputResult handle_search_input(int ch, struct DSVViewer *viewer, ViewState *state) {
    (void)viewer; // viewer might be used later for search execution

    switch(ch) {
        case 27: // ESC key
            state->input_mode = INPUT_MODE_NORMAL;
            state->search_term[0] = '\0';
            state->needs_redraw = true;
            return INPUT_CONSUMED;

        case KEY_ENTER:
        case '\n':
        case '\r':
            state->input_mode = INPUT_MODE_NORMAL;
            SearchResult result = search_view(viewer, state->current_view, state->search_term, true);
            if (result == SEARCH_WRAPPED_AND_FOUND) {
                snprintf(state->search_message, sizeof(state->search_message), "| Found: %s - search wrapped", state->search_term);
            } else if (result == SEARCH_FOUND) {
                snprintf(state->search_message, sizeof(state->search_message), "| Found: %s", state->search_term);
            }
            state->needs_redraw = true;
            return INPUT_CONSUMED;

        case KEY_BACKSPACE:
        case 127: // Handle backspace variations
        case 8:   // Handle backspace variations
            {
                size_t len = strlen(state->search_term);
                if (len > 0) {
                    state->search_term[len - 1] = '\0';
                }
            }
            state->needs_redraw = true;
            return INPUT_CONSUMED;

        default:
            // Append printable characters to the search term
            if (ch >= 32 && ch <= 126) {
                size_t len = strlen(state->search_term);
                if (len < (sizeof(state->search_term) - 1)) {
                    state->search_term[len] = ch;
                    state->search_term[len + 1] = '\0';
                }
            }
            state->needs_redraw = true;
            return INPUT_CONSUMED;
    }
} 