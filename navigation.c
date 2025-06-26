#include "viewer.h"

// Simple, robust separator finding functions
int find_next_separator(const char *line, int start_pos) {
    int sep_len = strlen(separator);
    int line_len = strlen(line);
    
    // Search for separator starting from start_pos
    for (int i = start_pos; i <= line_len - sep_len; i++) {
        if (strncmp(line + i, separator, sep_len) == 0) {
            return i + sep_len; // Return position AFTER the separator
        }
    }
    return line_len; // No separator found, return end of line
}

int find_prev_separator(const char *line, int start_pos) {
    int sep_len = strlen(separator);
    
    // Search backwards for separator
    for (int i = start_pos - sep_len; i >= 0; i--) {
        if (strncmp(line + i, separator, sep_len) == 0) {
            return i + sep_len; // Return position AFTER the separator
        }
    }
    return 0; // No separator found, return beginning of line
}

void run_viewer(CSVViewer *viewer) {
    int start_row = 0, start_col = 0;
    int ch;
    int rows, cols;
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Initialize colors
    if (has_colors()) {
        start_color();
        init_pair(1, 5, COLOR_BLACK); // Color 5 (magenta) on black background
    }
    
    getmaxyx(stdscr, rows, cols);
    int page_size = rows - 1; // Adjusted for new layout
    
    while (1) {
        display_data(viewer, start_row, start_col);
        ch = getch();
        
        switch (ch) {
            case 'q':
            case 'Q':
                goto cleanup;
                
            case 'h':
            case 'H':
                show_help();
                break;
                
            case KEY_UP:
                if (start_row > 0) start_row--;
                break;
                
            case KEY_DOWN:
                if (start_row < viewer->num_lines - 1) {
                    start_row++;
                }
                break;
                
            case KEY_LEFT:
                if (start_col > 0) {
                    start_col--; // Move to previous column
                }
                break;
                
            case KEY_RIGHT:
                if (start_col < viewer->num_cols - 1) {
                    start_col++; // Move to next column
                }
                break;
                
            case KEY_PPAGE: // Page Up
                start_row = (start_row - page_size < 0) ? 0 : start_row - page_size;
                break;
                
            case KEY_NPAGE: // Page Down
                start_row = (start_row + page_size >= viewer->num_lines) ? 
                           viewer->num_lines - 1 : start_row + page_size;
                break;
                
            case KEY_HOME:
                start_row = 0;
                start_col = 0;
                break;
                
            case KEY_END:
                start_row = viewer->num_lines - page_size;
                if (start_row < 0) start_row = 0;
                break;
                
            case 27: // ESC key
                goto cleanup;
        }
    }
    
cleanup:
    endwin();
} 