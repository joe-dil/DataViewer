#include "viewer.h"
#include "display_state.h"

static int handle_input(int ch, DSVViewer *viewer, size_t *start_row, size_t *start_col, int page_size) {
    size_t old_row = *start_row;
    size_t old_col = *start_col;
    int needs_redraw = 0;

    switch (ch) {
        case 'q':
            // This is handled in the main loop, no redraw needed
            break;
        case 'h':
        case 'H':
            show_help();
            needs_redraw = 1; // Redraw after help screen
            break;
        case KEY_UP:
            if (*start_row > 0) (*start_row)--;
            break;
        case KEY_DOWN:
            if (*start_row + 1 < viewer->file_data->num_lines) (*start_row)++;
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
            if (*start_row >= viewer->file_data->num_lines) {
                *start_row = (viewer->file_data->num_lines > 0) ? viewer->file_data->num_lines - 1 : 0;
            }
            break;
        case KEY_HOME:
            *start_row = 0;
            *start_col = 0;
            break;
        case KEY_END:
            *start_row = (viewer->file_data->num_lines > 0) ? viewer->file_data->num_lines - 1 : 0;
            break;
    }

    if (*start_row != old_row || *start_col != old_col) {
        needs_redraw = 1;
    }

    return needs_redraw;
}

void run_viewer(DSVViewer *viewer) {
    size_t start_row = 0, start_col = 0;
    int ch;
    
    // Initial display
    int rows, cols;
    viewer->display_state->needs_redraw = 1; // Force initial draw

    while (1) {
        if (viewer->display_state->needs_redraw) {
            getmaxyx(stdscr, rows, cols); // Get latest dimensions before drawing
            display_data(viewer, start_row, start_col);
            viewer->display_state->needs_redraw = 0;
        }

        ch = getch();
        if (ch == 'q') break;

        getmaxyx(stdscr, rows, cols); // Update dimensions on each loop
        int page_size = rows - 1;
        
        if (handle_input(ch, viewer, &start_row, &start_col, page_size)) {
            viewer->display_state->needs_redraw = 1;
        }
    }
} 