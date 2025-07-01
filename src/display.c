#include "viewer.h"
#include "display_state.h"
#include <ncurses.h>
#include <wchar.h>
#include <string.h>

// Simple helper: get column width with fallback
static int get_column_width(DSVViewer *viewer, size_t col) {
    return (col < viewer->display_state->num_cols) ? viewer->display_state->col_widths[col] : 12;
}

// Simple helper: render field content (common to both header and data)
static const char* render_column_content(DSVViewer *viewer, size_t col, int display_width) {
    char *rendered_field = viewer->display_state->buffers.render_buffer;
    render_field(&viewer->file_data->fields[col], rendered_field, viewer->config->max_field_len);
    return get_truncated_string(viewer, rendered_field, display_width);
}

// Simple helper: add separator if conditions are met
static void add_separator_if_needed(DSVViewer *viewer, int y, int x, size_t col, size_t num_fields, int cols) {
    if (col < num_fields - 1 && x + 3 <= cols) {
        mvaddstr(y, x, viewer->display_state->separator);
    }
}

// Draw header row with special formatting and padding
static void draw_header_row(int y, DSVViewer *viewer, size_t start_col, const DSVConfig *config) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    // Clear the line first
    move(y, 0);
    clrtoeol();
    
    // Calculate total width needed for all visible columns
    int total_width = 0;
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[0], viewer->file_data->fields, config->max_cols);
    size_t last_processed_col = start_col;
    bool broke_early = false;
    
    for (size_t col = start_col; col < num_fields; col++) {
        int col_width = get_column_width(viewer, col);
        int separator_space = (col < num_fields - 1) ? 3 : 0;
        int needed_space = col_width + separator_space;
        
        if (total_width + needed_space > cols) {
            // If this column would extend past screen, truncate
            col_width = cols - total_width - separator_space;
            if (col_width <= 0) {
                broke_early = true;
                break;
            }
            total_width += col_width;
            last_processed_col = col;
            broke_early = true;
            break;
        } else {
            total_width += needed_space;
            last_processed_col = col;
        }
    }
    
    // Check if there are more columns to the right:
    // Either we broke early OR we finished the loop but haven't shown all columns
    bool has_more_columns_right = broke_early || (last_processed_col + 1 < num_fields);
    
    // Extend underline to full width if there are more columns to the right
    int underline_width = has_more_columns_right ? cols : total_width;
    
    // Fill spaces for continuous underline
    move(y, 0);
    for (int i = 0; i < underline_width; i++) {
        addch(' ');
    }
    
    // Now render columns over the spaces
    int x = 0;
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        int original_col_width = col_width;
        
        // Header truncation logic (keep as-is, it works!)
        int separator_space = (col < num_fields - 1) ? 3 : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > cols) {
            col_width = cols - x;
            if (col_width <= 0) break;
        }
        
        const char *display_string = render_column_content(viewer, col, col_width);
        
        // Simplified padding logic for headers
        if (col_width < original_col_width) {
            // Only pad when truncated (existing behavior)
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
        if (col < num_fields - 1 && x + 3 <= cols && col_width == original_col_width) {
            mvaddstr(y, x, viewer->display_state->separator);
        }
        // Add final column separator if this is the last column
        else if (col == num_fields - 1 && x + 3 <= cols && col_width == original_col_width) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
        }
        x += 3;
        
        if (col_width != original_col_width && x >= cols) {
            break;
        }
    }
}

// Draw regular data row (keep simple, it works!)
static void draw_data_row(int y, DSVViewer *viewer, size_t file_line, size_t start_col, const DSVConfig *config) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[file_line], viewer->file_data->fields, config->max_cols);
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        
        const char *display_string = render_column_content(viewer, col, col_width);
        mvaddstr(y, x, display_string);
        x += col_width;
        
        // Add separator if not last column and space available  
        add_separator_if_needed(viewer, y, x, col, num_fields, cols);
        // Add final column separator if this is the last column
        if (col == num_fields - 1 && x + 3 <= cols) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
        }
        x += 3;
    }
}

void display_data(DSVViewer *viewer, size_t start_row, size_t start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // ANTI-FLICKER FIX: Replace clear() with line-by-line clearing
    // Only clear lines that will be used to avoid full screen flash
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (viewer->display_state->show_header) {
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        draw_header_row(0, viewer, start_col, viewer->config);
        attroff(COLOR_PAIR(1) | A_UNDERLINE);
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        // Clear each line before drawing to prevent artifacts
        move(screen_row, 0);
        clrtoeol();
        
        size_t file_line = start_row + screen_row - (viewer->display_state->show_header ? 0 : screen_start_row);
        if (file_line >= viewer->file_data->num_lines) {
            // Clear remaining lines if no more data
            continue;
        }

        draw_data_row(screen_row, viewer, file_line, start_col, viewer->config);
    }
    
    // Clear status line before updating
    move(rows - 1, 0);
    clrtoeol();
    
    // Use %zu for size_t types
    mvprintw(rows - 1, 0, "Lines %zu-%zu of %zu | Row: %zu | Col: %zu | q: quit | h: help",
             start_row + 1,
             (start_row + display_rows > viewer->file_data->num_lines) ? viewer->file_data->num_lines : start_row + display_rows,
             viewer->file_data->num_lines, start_row + 1, start_col + 1);
    
    refresh();
}

void show_help(void) {
    clear();
    mvprintw(1, 2, "DSV (Delimiter-Separated Values) Viewer - Help");
    mvprintw(3, 2, "Navigation:");
    mvprintw(4, 4, "Arrow Keys    - Jump between fields/scroll");
    mvprintw(5, 4, "Page Up/Down  - Scroll pages");
    mvprintw(6, 4, "Home          - Go to beginning");
    mvprintw(7, 4, "End           - Go to end");
    mvprintw(8, 4, "Mouse Scroll  - Fine character movement");
    mvprintw(10, 2, "Commands:");
    mvprintw(11, 4, "q             - Quit");
    mvprintw(12, 4, "h             - Show this help");
    mvprintw(14, 2, "Features:");
    mvprintw(15, 4, "- Fast loading of large files (memory mapped)");
    mvprintw(16, 4, "- Auto-detection of delimiters (, | ; tab)");
    mvprintw(17, 4, "- Arrow keys jump between fields");
    mvprintw(18, 4, "- Mouse wheel for fine scrolling");
    mvprintw(20, 2, "Press any key to return...");
    refresh();
    getch();
} 