#include "viewer.h"
#include <ncurses.h>
#include <stdbool.h>
#include "display_state.h"

// Explicit redraw control enum - FIXES ALL REDRAW ISSUES
typedef enum {
    INPUT_NO_CHANGE,        // No redraw needed - CRITICAL FIX
    INPUT_NEEDS_REDRAW,     // State changed, redraw required
    INPUT_QUIT,             // Quit application, no redraw
    INPUT_SHOW_HELP,        // Show help modal, handle separately
    INPUT_ERROR = -1        // Error occurred
} InputResult;

static InputResult handle_input_with_redraw_control(int ch, struct DSVViewer *viewer, size_t *start_row, size_t *start_col, int page_size) {
    size_t old_row = *start_row;
    size_t old_col = *start_col;

    switch (ch) {
        case 'q':
            return INPUT_QUIT;
        case 'h':
        case 'H':
            return INPUT_SHOW_HELP;
        case KEY_UP:
            if (*start_row > 0) {
                (*start_row)--;
            }
            break;
        case KEY_DOWN:
            if (*start_row + 1 < viewer->file_data->num_lines) {
                (*start_row)++;
            }
            break;
        case KEY_LEFT:
            if (*start_col > 0) {
                (*start_col)--;
            }
            break;
        case KEY_RIGHT:
            // Don't scroll if no columns would be visible from next position
            if (*start_col < viewer->display_state->num_cols - 1) {
                // Quick check: would any columns be visible from next position?
                int rows, cols;
                getmaxyx(stdscr, rows, cols);
                (void)rows;
                
                size_t next_start = *start_col + 1;
                int x = 0;
                bool would_show_columns = false;
                
                // Check if any columns from next_start would fit on screen
                for (size_t col = next_start; col < viewer->display_state->num_cols; col++) {
                    int col_width = viewer->display_state->col_widths[col];
                    if (x + col_width <= cols) {
                        would_show_columns = true;
                        break;
                    }
                    x += col_width + 3;
                }
                
                if (would_show_columns) {
                    (*start_col)++;
                }
            }
            break;
        case KEY_PPAGE:
            *start_row = (*start_row > (size_t)page_size) ? *start_row - page_size : 0;
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
        default:
             // CRITICAL: Unknown keys don't trigger redraw
            return INPUT_NO_CHANGE;
    }

    if (*start_row != old_row || *start_col != old_col) {
        return INPUT_NEEDS_REDRAW;
    }
    
    // CRITICAL: No redraw if at boundary or no change
    return INPUT_NO_CHANGE;
}

void run_viewer(DSVViewer *viewer) {
    size_t start_row = 0, start_col = 0;
    int rows, cols;
    
    viewer->display_state->needs_redraw = 1; // Force initial draw

    while (1) {
        // CRITICAL: Only redraw when needed
        if (viewer->display_state->needs_redraw) {
            getmaxyx(stdscr, rows, cols);
            display_data(viewer, start_row, start_col);
            viewer->display_state->needs_redraw = 0;
        }

        int ch = getch();
        getmaxyx(stdscr, rows, cols);
        int page_size = rows - 1;
        
        // CRITICAL: Use new redraw control system
        InputResult result = handle_input_with_redraw_control(ch, viewer, 
                                                            &start_row, &start_col, 
                                                            page_size);
        
        switch (result) {
            case INPUT_QUIT:
                return;  // CRITICAL: No redraw before quit
                
            case INPUT_SHOW_HELP:
                show_help();
                viewer->display_state->needs_redraw = 1;  // Redraw after modal
                break;
                
            case INPUT_NEEDS_REDRAW:
                viewer->display_state->needs_redraw = 1;
                break;
                
            case INPUT_NO_CHANGE:
                // CRITICAL: Do nothing, no redraw
                break;
                
            case INPUT_ERROR:
                // Handle error if needed
                break;
        }
    }
} 