#include "constants.h"
#include "viewer.h"
#include "display_state.h"
#include "cache.h"
#include "utils.h"
#include "highlighting.h"
#include <ncurses.h>
#include <wchar.h>
#include <string.h>
#include <stdbool.h>

// Simple helper: get column width with fallback
static int get_column_width(DSVViewer *viewer, size_t col) {
    return (col < viewer->display_state->num_cols) ? viewer->display_state->col_widths[col] : DEFAULT_COL_WIDTH;
}

// Simple helper: render field content (common to both header and data)
static const char* render_column_content(DSVViewer *viewer, size_t col, int display_width) {
    char *rendered_field = viewer->display_state->buffers.render_buffer;
    render_field(&viewer->file_data->fields[col], rendered_field, viewer->config->max_field_len);
    return get_truncated_string(viewer, rendered_field, display_width);
}

// Simple helper: add separator if conditions are met
static void add_separator_if_needed(DSVViewer *viewer, int y, int x, size_t col, size_t num_fields, int cols) {
    if (col < num_fields - 1 && x + SEPARATOR_WIDTH <= cols) {
        mvaddstr(y, x, viewer->display_state->separator);
    }
}

// Calculate which columns fit on screen and how to lay them out
// Handles column width calculation, truncation, and overflow indicators
static void calculate_header_layout(DSVViewer *viewer, size_t start_col, int cols, HeaderLayout *layout) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(layout);
    
    layout->content_width = 0;
    layout->last_visible_col = start_col;
    layout->has_more_columns_right = false;
    
    layout->num_fields = parse_line(viewer, viewer->file_data->line_offsets[0], viewer->file_data->fields, viewer->config->max_cols);
    bool broke_early = false;
    
    // Try to fit as many columns as possible within screen width
    for (size_t col = start_col; col < layout->num_fields; col++) {
        int col_width = get_column_width(viewer, col);
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
static int render_header_columns(DSVViewer *viewer, int y, size_t start_col, int cols, const HeaderLayout *layout) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(layout, 0);
    
    int x = 0;
    for (size_t col = start_col; col < layout->num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        int original_col_width = col_width;
        
        // Header truncation logic
        int separator_space = (col < layout->num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > cols) {
            col_width = cols - x;
            if (col_width <= 0) break;
        }
        
        const char *display_string = render_column_content(viewer, col, col_width);
        
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
static int draw_header_row(int y, DSVViewer *viewer, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(config, 0);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    HeaderLayout layout;
    calculate_header_layout(viewer, start_col, cols, &layout);
    render_header_background(y, layout.underline_width);
    int content_width = render_header_columns(viewer, y, start_col, cols, &layout);
    
    return content_width; // Return the actual header content width
}

// Calculate screen position and width for a given column
// Returns true if column is visible, false otherwise
static bool get_column_screen_position(DSVViewer *viewer, size_t start_col, size_t target_col, 
                                       int screen_width, int *out_x, int *out_width) {
    if (target_col < start_col) {
        return false;  // Column is scrolled off to the left
    }
    
    // Parse the header to get the actual number of fields
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[0], 
                                  viewer->file_data->fields, viewer->config->max_cols);
    
    int x = 0;
    for (size_t col = start_col; col <= target_col && col < num_fields; col++) {
        if (x >= screen_width) {
            return false;  // Already past screen edge
        }
        
        int col_width = get_column_width(viewer, col);
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

// Draw regular data row (keep simple, it works!)
static int draw_data_row(int y, DSVViewer *viewer, size_t file_line, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(config, 0);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[file_line], viewer->file_data->fields, config->max_cols);
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        
        const char *display_string = render_column_content(viewer, col, col_width);
        
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
    
    return x; // Return the actual content width
}

void display_data(DSVViewer *viewer, size_t start_row, size_t start_col, size_t cursor_row, size_t cursor_col) {
    CHECK_NULL_RET_VOID(viewer);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // ANTI-FLICKER FIX: Replace clear() with line-by-line clearing
    // Only clear lines that will be used to avoid full screen flash
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (viewer->display_state->show_header) {
        apply_header_row_format();
        draw_header_row(0, viewer, start_col, viewer->config);
        remove_header_row_format();
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        // Clear each line before drawing to prevent artifacts
        move(screen_row, 0);
        clrtoeol();
        
        // Calculate which data row we're displaying (0-based index, excluding header)
        size_t data_row_index = start_row + screen_row - screen_start_row;
        
        // Calculate the actual file line to display
        size_t file_line = data_row_index;
        if (viewer->display_state->show_header) {
            file_line++;  // Skip header line in file
        }
        
        if (file_line >= viewer->file_data->num_lines) {
            // Clear remaining lines if no more data
            continue;
        }

        int content_width = draw_data_row(screen_row, viewer, file_line, start_col, viewer->config);
        
        // Apply row highlighting if this is the cursor row
        if (data_row_index == cursor_row) {
            apply_row_highlight(screen_row, 0, content_width);
        }
    }
    
    // Apply column highlighting after all rows are drawn
    int col_x, col_width;
    if (get_column_screen_position(viewer, start_col, cursor_col, cols, &col_x, &col_width)) {
        // Column is visible
        if (viewer->display_state->show_header) {
            // Apply special highlighting to header column that preserves underline
            apply_header_column_highlight(col_x, col_width);
            
            // Apply regular highlighting to data rows
            apply_column_highlight(col_x, col_width, 1, display_rows);
        } else {
            // No header, highlight all rows
            apply_column_highlight(col_x, col_width, 0, display_rows);
        }
    }
    
    // Clear status line before updating
    move(rows - 1, 0);
    clrtoeol();
    
    // Updated status line to show cursor position
    size_t display_cursor_row = cursor_row + 1;  // Convert to 1-based
    if (viewer->display_state->show_header) {
        display_cursor_row++;  // Account for header being row 1
    }
    
    size_t first_visible = start_row + 1;
    size_t last_visible = start_row + display_rows - (viewer->display_state->show_header ? 1 : 0);
    if (viewer->display_state->show_header) {
        first_visible++;  // Account for header being row 1
        last_visible++;   // Account for header being row 1
    }
    if (last_visible > viewer->file_data->num_lines) {
        last_visible = viewer->file_data->num_lines;
    }
    
    mvprintw(rows - 1, 0, "Cursor: (%zu,%zu) | Viewing: %zu-%zu of %zu lines | q: quit | h: help",
             display_cursor_row, cursor_col + 1,
             first_visible,
             last_visible,
             viewer->file_data->num_lines);
    
    refresh();
}

void show_help(void) {
    clear();
    mvprintw(1, HELP_INDENT_COL, "DSV (Delimiter-Separated Values) Viewer - Help");
    mvprintw(3, HELP_INDENT_COL, "Navigation:");
    mvprintw(4, HELP_ITEM_INDENT_COL, "Arrow Keys    - Jump between fields/scroll");
    mvprintw(5, HELP_ITEM_INDENT_COL, "Page Up/Down  - Scroll pages");
    mvprintw(6, HELP_ITEM_INDENT_COL, "Home          - Go to beginning");
    mvprintw(7, HELP_ITEM_INDENT_COL, "End           - Go to end");
    mvprintw(8, HELP_ITEM_INDENT_COL, "Mouse Scroll  - Fine character movement");
    mvprintw(HELP_COMMANDS_ROW, HELP_INDENT_COL, "Commands:");
    mvprintw(11, HELP_ITEM_INDENT_COL, "q             - Quit");
    mvprintw(12, HELP_ITEM_INDENT_COL, "h             - Show this help");
    mvprintw(HELP_FEATURES_ROW, HELP_INDENT_COL, "Features:");
    mvprintw(15, HELP_ITEM_INDENT_COL, "- Fast loading of large files (memory mapped)");
    mvprintw(16, HELP_ITEM_INDENT_COL, "- Auto-detection of delimiters (, | ; tab)");
    mvprintw(17, HELP_ITEM_INDENT_COL, "- Arrow keys jump between fields");
    mvprintw(18, HELP_ITEM_INDENT_COL, "- Mouse wheel for fine scrolling");
    mvprintw(20, HELP_INDENT_COL, "Press any key to return...");
    refresh();
    getch();
} 