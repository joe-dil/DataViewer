#include "viewer.h"

void run_viewer(CSVViewer *viewer) {
    int start_row = 0, start_col = 0;
    int ch;
    int rows, cols;
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0); // Hide cursor
    
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
                    start_col--;
                }
                break;
                
            case KEY_RIGHT:
                if (start_col < viewer->num_cols - 1) {
                    start_col++;
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
    curs_set(1); // Restore cursor
    endwin();
} 