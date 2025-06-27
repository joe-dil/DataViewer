#include "viewer.h"
#include <wchar.h>
#include <string.h>

// Draw header row with special formatting and padding
static void draw_header_row(int y, DSVViewer *viewer, size_t start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    // Clear the entire header line
    move(y, 0);
    for (int i = 0; i < cols; i++) {
        addch(' ');
    }
    
    int x = 0;
    size_t num_fields = parse_line(viewer, viewer->line_offsets[0], viewer->fields, MAX_COLS);
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = (col < viewer->num_cols) ? viewer->col_widths[col] : 12;
        int original_col_width = col_width;
        
        // Header truncation logic
        int separator_space = (col < num_fields - 1) ? 3 : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > cols) {
            col_width = cols - x;
            if (col_width <= 0) break;
        }
        
        char *rendered_field = viewer->buffer_pool.buffer_one;
        render_field(&viewer->fields[col], rendered_field, MAX_FIELD_LEN);
        
        const char *display_string = get_truncated_string(viewer, rendered_field, col_width);
        
        if (col_width < original_col_width) {
            // Pad truncated header fields with spaces
            int text_len = strlen(display_string);
            char *padded_field = viewer->buffer_pool.buffer_two;
            strcpy(padded_field, display_string);
            memset(padded_field + text_len, ' ', col_width - text_len);
            padded_field[col_width] = '\0';
            mvaddstr(y, x, padded_field);
        } else {
            mvaddstr(y, x, display_string);
        }

        x += col_width;
        
        // Add separator if not truncated and not last column
        if (col < num_fields - 1 && x + 3 <= cols && col_width == original_col_width) {
            mvaddstr(y, x, viewer->separator);
        }
        x += 3;
        
        if (col_width != original_col_width && x >= cols) {
            break;
        }
    }
}

// Draw regular data row
static void draw_data_row(int y, DSVViewer *viewer, size_t file_line, size_t start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    size_t num_fields = parse_line(viewer, viewer->line_offsets[file_line], viewer->fields, MAX_COLS);
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = (col < viewer->num_cols) ? viewer->col_widths[col] : 12;
        
        char *rendered_field = viewer->buffer_pool.buffer_one;
        render_field(&viewer->fields[col], rendered_field, MAX_FIELD_LEN);
        
        const char *display_string = get_truncated_string(viewer, rendered_field, col_width);
        mvaddstr(y, x, display_string);
        x += col_width;
        
        // Add separator if not last column and space available
        if (col < num_fields - 1 && x + 3 <= cols) {
            mvaddstr(y, x, viewer->separator);
        }
        x += 3;
    }
}

void display_data(DSVViewer *viewer, size_t start_row, size_t start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    clear();
    
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (viewer->display_state->show_header) {
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        draw_header_row(0, viewer, start_col);
        attroff(COLOR_PAIR(1) | A_UNDERLINE);
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        size_t file_line = start_row + screen_row - (viewer->display_state->show_header ? 0 : screen_start_row);
        if (file_line >= viewer->num_lines) break;

        draw_data_row(screen_row, viewer, file_line, start_col);
    }
    
    // Use %zu for size_t types
    mvprintw(rows - 1, 0, "Lines %zu-%zu of %zu | Row: %zu | Col: %zu | q: quit | h: help",
             start_row + 1,
             (start_row + display_rows > viewer->num_lines) ? viewer->num_lines : start_row + display_rows,
             viewer->num_lines, start_row + 1, start_col + 1);
    
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