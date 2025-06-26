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
        format_line(viewer, 0, formatted_line, sizeof(formatted_line));
        
        // Display header starting from start_col character position
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        int line_len = strlen(formatted_line);
        if (start_col < line_len) {
            mvprintw(0, 0, "%-.*s", cols, formatted_line + start_col);
        }
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
        format_line(viewer, file_line, formatted_line, sizeof(formatted_line));
        
        // Display line starting from start_col character position
        int line_len = strlen(formatted_line);
        if (start_col < line_len) {
            mvprintw(screen_row, 0, "%-.*s", cols, formatted_line + start_col);
        }
    }
    
    // Status line
    mvprintw(rows - 1, 0, "Lines %d-%d of %d | Row: %d | Col: %d | q: quit | h: help", 
             start_row + 1, 
             start_row + display_rows > viewer->num_lines ? viewer->num_lines : start_row + display_rows,
             viewer->num_lines, start_row + 1, start_col + 1);
    
    refresh();
}

void format_line(CSVViewer *viewer, int line_index, char *output, int max_output_len) {
    output[0] = '\0'; // Start with empty string
    
    int num_fields = parse_line(viewer, viewer->line_offsets[line_index], 
                              viewer->fields, MAX_COLS);
    
    for (int col = 0; col < num_fields; col++) {
        char sanitized_field[MAX_FIELD_LEN];
        sanitize_field(sanitized_field, viewer->fields[col], MAX_FIELD_LEN - 1);
        
        // Use fixed width for simplicity - 16 chars per field
        char formatted_field[32];
        snprintf(formatted_field, sizeof(formatted_field), "%-16.16s", sanitized_field);
        
        // Efficient concatenation
        strncat(output, formatted_field, max_output_len - strlen(output) - 1);
        
        if (col < num_fields - 1) {
            strncat(output, " | ", max_output_len - strlen(output) - 1);
        }
    }
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