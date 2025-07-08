#include "constants.h"
#include "app_init.h"
#include "display_state.h"
#include "cache.h"
#include "utils.h"
#include "highlighting.h"
#include "navigation.h"
#include "analysis.h"
#include "core/parser.h"
#include <ncurses.h>
#include <wchar.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define COLOR_PAIR_SELECTED 2
#define COLOR_PAIR_ERROR 3
#define ERROR_MESSAGE_DURATION_MS 3000  // Show error for 3 seconds

#define STATUS_MESSAGE_DURATION_MS 3000 // Show status for 3 seconds

// Helper function to check if error message should still be shown
static bool should_show_error(DSVViewer *viewer) {
    if (!viewer || !viewer->display_state || !viewer->display_state->show_error_message) {
        return false;
    }
    
    double elapsed = get_time_ms() - viewer->display_state->error_message_time;
    if (elapsed > ERROR_MESSAGE_DURATION_MS) {
        viewer->display_state->show_error_message = 0;
        return false;
    }
    
    return true;
}

// Helper function to check if status message should still be shown
static bool should_show_status(DSVViewer *viewer) {
    if (!viewer || !viewer->display_state || !viewer->display_state->show_status_message) {
        return false;
    }
    
    double elapsed = get_time_ms() - viewer->display_state->status_message_time;
    if (elapsed > STATUS_MESSAGE_DURATION_MS) {
        viewer->display_state->show_status_message = 0;
        return false;
    }
    
    return true;
}

// Simple helper: get column width with fallback
static int get_column_width(DSVViewer *viewer, const ViewState *state, size_t col) {
    DataSource *ds = state->current_view->data_source;
    if (ds->type == DATA_SOURCE_MEMORY) {
        return ds->ops->get_column_width(ds->context, col);
    } else { // DATA_SOURCE_FILE
        return analysis_get_column_width(viewer, col);
    }
}

// Simple helper: add separator if conditions are met
static void add_separator_if_needed(DSVViewer *viewer, int y, int x, size_t col, size_t num_fields, int cols) {
    if (col < num_fields - 1 && x + SEPARATOR_WIDTH <= cols) {
        mvaddstr(y, x, viewer->display_state->separator);
    }
}

// Calculate which columns fit on screen and how to lay them out
// Handles column width calculation, truncation, and overflow indicators
static void calculate_header_layout(DSVViewer *viewer, ViewState *state, size_t start_col, int cols, HeaderLayout *layout) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(layout);
    
    layout->content_width = 0;
    layout->last_visible_col = start_col;
    layout->has_more_columns_right = false;
    
    if (state->current_view && state->current_view->data_source) {
        DataSource *ds = state->current_view->data_source;
        layout->num_fields = ds->ops->get_col_count(ds->context);
    } else {
        layout->num_fields = 0;
    }

    bool broke_early = false;
    
    // Try to fit as many columns as possible within screen width
    for (size_t col = start_col; col < layout->num_fields; col++) {
        int col_width = get_column_width(viewer, state, col);
        int separator_space = (col < layout->num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        
        if (layout->content_width + needed_space > cols) {
            // If this column would extend past screen, truncate
            col_width = cols - layout->content_width - separator_space;
            if (col_width <= 0) {
                broke_early = true;
                break;
            }
            layout->content_width += col_width;
            layout->last_visible_col = col;
            broke_early = true;
            break;
        } else {
            layout->content_width += needed_space;
            layout->last_visible_col = col;
        }
    }
    
    // Check if there are more columns to the right
    layout->has_more_columns_right = broke_early || (layout->last_visible_col + 1 < layout->num_fields);
    
    // Extend underline to full width if there are more columns to the right
    layout->underline_width = layout->has_more_columns_right ? cols : layout->content_width;
}

// Phase 4: Render header background with proper underline
static void render_header_background(int y, int underline_width) {
    move(y, 0);
    clrtoeol();
    
    // Fill spaces for continuous underline
    for (int i = 0; i < underline_width; i++) {
        addch(' ');
    }
}

// Phase 4: Render actual header column content
static int render_header_columns(DSVViewer *viewer, const ViewState* state, int y, size_t start_col, int cols, const HeaderLayout *layout) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(layout, 0);
    
    int x = 0;
    for (size_t col = start_col; col < layout->num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, state, col);
        int original_col_width = col_width;
        
        // Header truncation logic
        int separator_space = (col < layout->num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > cols) {
            col_width = cols - x;
            if (col_width <= 0) break;
        }
        
        char header_buffer[DEFAULT_MAX_FIELD_LEN];
        if (state->current_view && state->current_view->data_source) {
            DataSource *ds = state->current_view->data_source;
            FieldDesc fd = ds->ops->get_header(ds->context, col);
            render_field(&fd, header_buffer, sizeof(header_buffer));
        } else {
            header_buffer[0] = '\0';
        }
        const char *display_string = get_truncated_string(viewer, header_buffer, col_width);
        
        // Simplified padding logic for headers
        if (col_width < original_col_width) {
            // Only pad when truncated
            char *padded_field = viewer->display_state->buffers.pad_buffer;
            int text_len = strlen(display_string);
            int pad_width = (col_width < viewer->config->max_field_len) ? col_width : viewer->config->max_field_len - 1;
            
            strncpy(padded_field, display_string, pad_width);
            if (text_len < pad_width) {
                memset(padded_field + text_len, ' ', pad_width - text_len);
            }
            padded_field[pad_width] = '\0';
            mvaddstr(y, x, padded_field);
        } else {
            mvaddstr(y, x, display_string);
        }

        x += col_width;
        
        // Add separator if not truncated and not last column
        if (col < layout->num_fields - 1 && x + SEPARATOR_WIDTH <= cols && col_width == original_col_width) {
            mvaddstr(y, x, viewer->display_state->separator);
            x += SEPARATOR_WIDTH;
        }
        // Add final column separator if this is the last column
        else if (col == layout->num_fields - 1 && x + SEPARATOR_WIDTH <= cols && col_width == original_col_width) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
            x += SEPARATOR_WIDTH;
        }
        // Only add separator width for non-truncated, non-last columns where we didn't draw a separator yet
        else if (col < layout->num_fields - 1 && col_width == original_col_width) {
            x += SEPARATOR_WIDTH;
        }
        
        if (col_width != original_col_width && x >= cols) {
            break;
        }
    }
    
    return x; // Return the actual content width
}

// Phase 4: Main header drawing function - now much simpler
static int draw_header_row(int y, DSVViewer *viewer, const ViewState *state, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(config, 0);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    HeaderLayout layout;
    calculate_header_layout(viewer, (ViewState*)state, start_col, cols, &layout);
    render_header_background(y, layout.underline_width);
    int content_width = render_header_columns(viewer, state, y, start_col, cols, &layout);
    
    return content_width; // Return the actual header content width
}

// Calculate screen position and width for a given column
// Returns true if column is visible, false otherwise
static bool get_column_screen_position(DSVViewer *viewer, const ViewState *state, size_t start_col, size_t target_col, 
                                       int screen_width, int *out_x, int *out_width) {
    if (target_col < start_col) {
        return false;  // Column is scrolled off to the left
    }
    
    // Parse the header to get the actual number of fields
    size_t num_fields = 0;
    if (state->current_view && state->current_view->data_source) {
        DataSource *ds = state->current_view->data_source;
        num_fields = ds->ops->get_col_count(ds->context);
    }
    
    int x = 0;
    for (size_t col = start_col; col <= target_col && col < num_fields; col++) {
        if (x >= screen_width) {
            return false;  // Already past screen edge
        }
        
        int col_width = get_column_width(viewer, state, col);
        int original_col_width = col_width;
        
        // Apply same truncation logic as render_header_columns
        int separator_space = (col < num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > screen_width) {
            col_width = screen_width - x;
            if (col_width <= 0) {
                return false;
            }
        }
        
        if (col == target_col) {
            // This is our target column
            *out_x = x;
            *out_width = col_width;
            return true;
        }
        
        // Advance position using same logic as render_header_columns
        x += col_width;
        
        // Add separator width using exact same conditions as render_header_columns
        if (col < num_fields - 1 && x + SEPARATOR_WIDTH <= screen_width && col_width == original_col_width) {
            x += SEPARATOR_WIDTH;
        }
        else if (col == num_fields - 1 && x + SEPARATOR_WIDTH <= screen_width && col_width == original_col_width) {
            x += SEPARATOR_WIDTH;
        }
        else if (col < num_fields - 1 && col_width == original_col_width) {
            x += SEPARATOR_WIDTH;
        }
        
        if (col_width != original_col_width && x >= screen_width) {
            return false;
        }
    }
    
    return false;  // Shouldn't reach here
}

static void display_table_view(DSVViewer *viewer, const ViewState *state);
static void display_help_panel(void);

// Draw regular data row (keep simple, it works!)
static int draw_data_row(int y, DSVViewer *viewer, const ViewState *state, size_t display_row, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(config, 0);

    bool is_selected = state->current_view ? is_row_selected(state->current_view, display_row) : false;
    if (is_selected) {
        attron(COLOR_PAIR(COLOR_PAIR_SELECTED));
    }
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    
    size_t num_fields = 0;
    if (state->current_view && state->current_view->data_source) {
        DataSource *ds = state->current_view->data_source;
        num_fields = ds->ops->get_col_count(ds->context);
    }
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, state, col);
        
        char cell_buffer[DEFAULT_MAX_FIELD_LEN];
        int result = -1;

        if (state->current_view && state->current_view->data_source) {
             DataSource *ds = state->current_view->data_source;
             size_t actual_row = view_get_displayed_row_index(state->current_view, display_row);

             if (actual_row == SIZE_MAX) {
                 // Should not happen if calling code is correct
                 continue;
             }
             
             FieldDesc fd = ds->ops->get_cell(ds->context, actual_row, col);
             if (fd.start == NULL) {
                 result = -1;
                 cell_buffer[0] = '\0';
             } else {
                render_field(&fd, cell_buffer, sizeof(cell_buffer));
                result = strlen(cell_buffer);
             }
        }
        
        if (result < 0) {
            // Error or out of bounds, maybe draw something? For now, skip.
            continue;
        }

        const char *display_string = get_truncated_string(viewer, 
                                                          cell_buffer, 
                                                          col_width);
        
        // Draw the field content
        mvaddstr(y, x, display_string);
        x += col_width;
        
        // Add separator if not last column and space available  
        add_separator_if_needed(viewer, y, x, col, num_fields, cols);
        // Add final column separator if this is the last column
        if (col == num_fields - 1 && x + SEPARATOR_WIDTH <= cols) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
            // Don't add separator width to x for final separator - we want highlighting to stop before it
        } else if (col < num_fields - 1) {
            x += SEPARATOR_WIDTH;
        }
    }

    if (is_selected) {
        attroff(COLOR_PAIR(COLOR_PAIR_SELECTED));
    }
    
    return x; // Return the actual content width
}

void display_data(DSVViewer *viewer, const ViewState *state) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(state);

    switch (state->current_panel) {
        case PANEL_TABLE_VIEW:
            display_table_view(viewer, state);
            break;
        case PANEL_FREQ_ANALYSIS:
            // This panel is deprecated and will be replaced by the new data view panel.
            // For now, it does nothing.
            break;
        case PANEL_HELP:
            display_help_panel();
            break;
    }

    refresh();
}

static void display_table_view(DSVViewer *viewer, const ViewState *state) {
    View *current_view = state->current_view;
    if (!current_view) return;
    
    // Handle empty views gracefully
    if (current_view->visible_row_count == 0) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        
        // Clear screen
        clear();
        
        // Show message in center of screen
        const char *msg = "No data to display";
        mvprintw(rows / 2, (cols - strlen(msg)) / 2, "%s", msg);
        
        // Still show status line
        move(rows - 1, 0);
        clrtoeol();
        mvprintw(rows - 1, 0, "%s | Empty view", current_view->name);
        
        refresh();
        return;
    }
    
    size_t start_row = current_view->start_row;
    size_t start_col = current_view->start_col;
    size_t cursor_row = current_view->cursor_row;
    size_t cursor_col = current_view->cursor_col;
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // CRITICAL FIX: Adjust viewport if cursor is not visible
    // This should happen BEFORE we start drawing
    int cursor_x, cursor_width;
    bool cursor_visible = get_column_screen_position(viewer, state, start_col, cursor_col, cols, &cursor_x, &cursor_width);
    
    if (!cursor_visible) {
        // Cursor column is not visible with current start_col
        if (cursor_col < start_col) {
            // Cursor is to the left of viewport
            ((View*)current_view)->start_col = cursor_col;
            start_col = cursor_col;
        } else {
            // Cursor is to the right of viewport
            // Check if scrolling by 1 would make it visible
            size_t new_start = start_col + 1;
            if (get_column_screen_position(viewer, state, new_start, cursor_col, cols, &cursor_x, &cursor_width)) {
                // Scrolling by 1 is enough
                ((View*)current_view)->start_col = new_start;
                start_col = new_start;
            } else {
                // Need to scroll more - find the minimum that works
                // Allow new_start to go up to cursor_col (inclusive) to handle last column
                while (new_start <= cursor_col && !get_column_screen_position(viewer, state, new_start, cursor_col, cols, &cursor_x, &cursor_width)) {
                    new_start++;
                }
                ((View*)current_view)->start_col = new_start;
                start_col = new_start;
            }
        }
    }
    
    // ANTI-FLICKER FIX: Replace clear() with line-by-line clearing
    // Only clear lines that will be used to avoid full screen flash
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (viewer->display_state->show_header) {
        apply_header_row_format();
        draw_header_row(0, viewer, state, start_col, viewer->config);
        remove_header_row_format();
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        // Clear each line before drawing to prevent artifacts
        move(screen_row, 0);
        clrtoeol();
        
        // Calculate which data row we're displaying (0-based index into the current view)
        size_t view_data_row = start_row + screen_row - screen_start_row;
        
        if (view_data_row >= current_view->visible_row_count) {
            // Clear remaining lines if no more data in this view
            continue;
        }

        int content_width = draw_data_row(screen_row, viewer, state, view_data_row, start_col, viewer->config);
        
        // Apply row highlighting if this is the cursor row
        if (view_data_row == cursor_row) {
            apply_row_highlight(screen_row, 0, content_width);
        }
    }
    
    // Apply column highlighting after all rows are drawn
    int col_x, col_width;
    if (get_column_screen_position(viewer, state, start_col, cursor_col, cols, &col_x, &col_width)) {
        // Column is visible
        if (viewer->display_state->show_header) {
            // Apply special highlighting to header column that preserves underline
            apply_header_column_highlight(col_x, col_width);
            
            // Calculate the last row that actually has data
            size_t remaining_data_rows = (current_view->visible_row_count > start_row) ? 
                                        current_view->visible_row_count - start_row : 0;
            int data_end_row = screen_start_row + (int)remaining_data_rows;
            if (data_end_row > display_rows) {
                data_end_row = display_rows;
            }
            
            // Apply regular highlighting only to data rows
            apply_column_highlight(col_x, col_width, 1, data_end_row);
        } else {
            // No header - calculate data end row from row 0
            size_t remaining_data_rows = (current_view->visible_row_count > start_row) ? 
                                        current_view->visible_row_count - start_row : 0;
            int data_end_row = (int)remaining_data_rows;
            if (data_end_row > display_rows) {
                data_end_row = display_rows;
            }
            
            // Apply highlighting only to rows with data
            apply_column_highlight(col_x, col_width, 0, data_end_row);
        }
    }
    
    // Clear status line before updating
    move(rows - 1, 0);
    clrtoeol();
    
    // Check for special modes first, as they take priority
    if (state->input_mode == INPUT_MODE_SEARCH) {
        mvprintw(rows - 1, 0, "/%s", state->search_term);
        // Place cursor at the end of the typed term
        move(rows - 1, strlen(state->search_term) + 1);
    }
    else if (should_show_error(viewer)) {
        attron(COLOR_PAIR(COLOR_PAIR_ERROR));
        mvprintw(rows - 1, 0, "Error: %s", viewer->display_state->error_message);
        attroff(COLOR_PAIR(COLOR_PAIR_ERROR));
    }
    else if (should_show_status(viewer)) {
        mvprintw(rows - 1, 0, "%s", viewer->display_state->status_message);
    }
    else if (viewer->display_state->show_copy_status) {
        // Show copy status message
        mvprintw(rows - 1, 0, "%s", viewer->display_state->copy_status);
        
        // After showing, reset flag
        viewer->display_state->show_copy_status = 0;
    }
    else {
        // Build the status line string
        char status_buffer[cols + 1];
        size_t viewing_end = start_row + display_rows > current_view->visible_row_count ? current_view->visible_row_count : start_row + display_rows;
        snprintf(status_buffer, sizeof(status_buffer), 
                 "%s | Cursor: (%zu,%zu) | Viewing: %zu-%zu of %zu | sel: %zu",
                 current_view->name,
                 cursor_row + 1, cursor_col + 1,
                 start_row + 1, viewing_end, current_view->visible_row_count,
                 current_view->selection_count);
        
        // Append sort status if applicable
        if (current_view->sort_direction != SORT_NONE) {
            char sort_col_name[256];
            char temp_buffer[256]; // Buffer for get_column_name
            get_column_name(viewer, current_view->sort_column, temp_buffer, sizeof(temp_buffer));
            snprintf(sort_col_name, sizeof(sort_col_name), " | Sorted by: %s (%s)",
                     temp_buffer,
                     current_view->sort_direction == SORT_ASC ? "ASC" : "DESC");
            strncat(status_buffer, sort_col_name, sizeof(status_buffer) - strlen(status_buffer) - 1);
        }

        // Append search message if it exists
        if (state->search_message[0] != '\0') {
            strncat(status_buffer, state->search_message, sizeof(status_buffer) - strlen(status_buffer) - 1);
        }
        
        mvprintw(rows - 1, 0, "%s", status_buffer);
    }
}

static void display_help_panel(void) {
    show_help();
}

void show_help(void) {
    clear();
    mvprintw(1, HELP_INDENT_COL, "DSV (Delimiter-Separated Values) Viewer - Help");
    mvprintw(3, HELP_INDENT_COL, "Navigation:");
    mvprintw(4, HELP_ITEM_INDENT_COL, "Arrow Keys    - Move cursor");
    mvprintw(5, HELP_ITEM_INDENT_COL, "Page Up/Down  - Scroll by a full page");
    mvprintw(6, HELP_ITEM_INDENT_COL, "Home/End      - Go to start/end of row/file");
    
    mvprintw(8, HELP_INDENT_COL, "Row Selection:");
    mvprintw(9, HELP_ITEM_INDENT_COL, "Space         - Toggle selection for the current row");
    mvprintw(10, HELP_ITEM_INDENT_COL, "A or ESC      - Clear all selections");

    mvprintw(12, HELP_INDENT_COL, "Views:");
    mvprintw(13, HELP_ITEM_INDENT_COL, "v             - Create a new view from selected rows");
    mvprintw(14, HELP_ITEM_INDENT_COL, "Tab/Shift+Tab - Cycle through open views");
    mvprintw(15, HELP_ITEM_INDENT_COL, "x             - Close the current view (except Main)");

    mvprintw(17, HELP_INDENT_COL, "General:");
    mvprintw(18, HELP_ITEM_INDENT_COL, "y             - Copy current cell to clipboard");
    mvprintw(19, HELP_ITEM_INDENT_COL, "h             - Show this help screen");
    mvprintw(20, HELP_ITEM_INDENT_COL, "q             - Quit the application");

    mvprintw(22, HELP_INDENT_COL, "Press any key to return...");
    refresh();
    getch();
}

// Helper function to set an error message
void set_error_message(DSVViewer *viewer, const char *format, ...) {
    if (!viewer || !viewer->display_state) return;
    
    va_list args;
    va_start(args, format);
    vsnprintf(viewer->display_state->error_message, 
              sizeof(viewer->display_state->error_message), format, args);
    va_end(args);
    
    viewer->display_state->show_error_message = 1;
    viewer->display_state->error_message_time = get_time_ms();
    viewer->view_state.needs_redraw = true;
} 

// Helper function to set a status message
void set_status_message(DSVViewer *viewer, const char *format, ...) {
    if (!viewer || !viewer->display_state) return;
    
    va_list args;
    va_start(args, format);
    vsnprintf(viewer->display_state->status_message, 
              sizeof(viewer->display_state->status_message), format, args);
    va_end(args);
    
    viewer->display_state->show_status_message = 1;
    viewer->display_state->status_message_time = get_time_ms();
    viewer->view_state.needs_redraw = true;
} 