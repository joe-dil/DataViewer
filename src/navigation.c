#include "viewer.h"

static void handle_input(int ch, DSVViewer *viewer, size_t *start_row, size_t *start_col, int page_size) {
    switch (ch) {
        case 'q':
            // This is handled in the main loop
            break;
        case 'h':
        case 'H':
            show_help();
            break;
        case KEY_UP:
            if (*start_row > 0) (*start_row)--;
            break;
        case KEY_DOWN:
            if (*start_row < viewer->file_data->num_lines - 1) (*start_row)++;
            break;
        case KEY_LEFT:
            if (*start_col > 0) (*start_col)--;
            break;
        case KEY_RIGHT:
            if (*start_col < viewer->display_state->num_cols - 1) (*start_col)++;
            break;
        case KEY_PPAGE:
            if (*start_row > (size_t)page_size) {
                *start_row -= page_size;
            } else {
                *start_row = 0;
            }
            break;
        case KEY_NPAGE:
            *start_row += page_size;
            if (*start_row >= viewer->file_data->num_lines) *start_row = viewer->file_data->num_lines - 1;
            break;
        case KEY_HOME:
            *start_row = 0;
            *start_col = 0;
            break;
        case KEY_END:
            *start_row = viewer->file_data->num_lines - 1;
            break;
    }
}

void run_viewer(DSVViewer *viewer) {
    size_t start_row = 0, start_col = 0;
    int ch;
    
    // Initial display
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    display_data(viewer, start_row, start_col);

    while ((ch = getch()) != 'q') {
        getmaxyx(stdscr, rows, cols); // Update dimensions on each loop
        int page_size = rows - 1;
        
        handle_input(ch, viewer, &start_row, &start_col, page_size);
        display_data(viewer, start_row, start_col);
    }
} 