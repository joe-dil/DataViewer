#include "viewer.h"

void display_data(CSVViewer *viewer, int start_row, int start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    clear();
    
    int display_rows = rows - 1; // Leave space for status line
    int screen_start_row = 0;
    
    // Show persistent header if enabled
    if (show_header) {
        char formatted_line[BUFFER_SIZE * 2] = "";
        format_line_for_width(viewer, 0, formatted_line, sizeof(formatted_line), start_col, cols);
        
        // Display header
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        mvprintw(0, 0, "%s", formatted_line);
        attroff(COLOR_PAIR(1) | A_UNDERLINE);
        screen_start_row = 1; // Data starts from row 1
    }
    
    // Display data rows
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        int file_line;
        if (show_header) {
            // With persistent header: start_row=0 shows file line 1, etc.
            file_line = start_row + screen_row;
        } else {
            // Without header: start_row=0 shows file line 0, etc.
            file_line = start_row + screen_row - screen_start_row;
        }
        
        if (file_line >= viewer->num_lines) break;
        
        char formatted_line[BUFFER_SIZE * 2] = "";
        format_line_for_width(viewer, file_line, formatted_line, sizeof(formatted_line), start_col, cols);
        
        // Display the formatted line
        mvprintw(screen_row, 0, "%s", formatted_line);
    }
    
    // Status line with debug info
    char status_line[BUFFER_SIZE];
    char temp_line[BUFFER_SIZE * 2] = "";
    format_line_for_width(viewer, 0, temp_line, sizeof(temp_line), start_col, cols);
    int actual_len = strlen(temp_line);
    int wasted = cols - actual_len;
    float waste_percent = (cols > 0) ? ((float)wasted / cols * 100) : 0;

    snprintf(status_line, sizeof(status_line), 
            "Lines %d-%d | Col: %d | Screen: %d, Used: %d, Waste: %d (%.1f%%) | q: quit", 
            start_row + 1, 
            start_row + display_rows > viewer->num_lines ? viewer->num_lines : start_row + display_rows,
            start_col + 1, cols, actual_len, wasted, waste_percent);
    
    mvprintw(rows - 1, 0, "%-.*s", cols, status_line);
    
    refresh();
}

void format_line_for_width(CSVViewer *viewer, int line_index, char *output, int max_output_len, int start_col, int target_width) {
    output[0] = '\0';
    
    int num_fields = parse_line(viewer, viewer->line_offsets[line_index], 
                              viewer->fields, MAX_COLS);
    
    int used_width = 0;
    // The display width of the separator is always 3 characters on screen,
    // even though its byte-length might be different (e.g., 5 for " │ ").
    int sep_display_width = 3;
    
    for (int col = start_col; col < num_fields; col++) {
        char sanitized_field[MAX_FIELD_LEN];
        sanitize_field(sanitized_field, viewer->fields[col], MAX_FIELD_LEN - 1);
        
        int col_width = (col < viewer->num_cols && viewer->col_widths) ? 
                       viewer->col_widths[col] : 12;
        
        // Check if full column + separator fits, using DISPLAY widths
        int space_needed = col_width + (col < num_fields - 1 ? sep_display_width : 0);
        
        if (used_width + space_needed <= target_width) {
            char formatted_field[64];
            snprintf(formatted_field, sizeof(formatted_field), "%-*.*s", 
                    col_width, col_width, sanitized_field);
            strncat(output, formatted_field, max_output_len - strlen(output) - 1);
            used_width += col_width;
            
            if (col < num_fields - 1) {
                strncat(output, separator, max_output_len - strlen(output) - 1);
                used_width += sep_display_width;
            }
        } else {
            int remaining = target_width - used_width;
            if (remaining >= 4) { 
                char formatted_field[64];
                snprintf(formatted_field, sizeof(formatted_field), "%-*.*s", 
                        remaining, remaining, sanitized_field);
                strncat(output, formatted_field, max_output_len - strlen(output) - 1);
            }
            break; 
        }
    }
}

void format_line(CSVViewer *viewer, int line_index, char *output, int max_output_len) {
    format_line_for_width(viewer, line_index, output, max_output_len, 0, 120);
}

void sanitize_field(char *dest, const char *src, int max_len) {
    int j = 0;
    for (int i = 0; src[i] != '\0' && j < max_len; i++) {
        unsigned char c = src[i];
        
        // Only filter out control characters that mess up terminal display
        if (c < 32 || c == 127) {
            // Replace control chars with space
            dest[j++] = ' ';
        } else {
            // Keep everything else (printable ASCII + Unicode)
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}

void show_help(void) {
    clear();
    mvprintw(1, 2, "CSV/PSV Viewer - Help");
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