#include "navigation.h"
#include "app_init.h"
#include "constants.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Helper function to check if a column is fully visible in the current viewport
static bool is_column_fully_visible(const DSVViewer *viewer, size_t start_col, size_t target_col, int screen_width) {
    if (target_col < start_col) {
        return false;  // Column is before viewport
    }
    
    int x = 0;
    for (size_t col = start_col; col <= target_col; col++) {
        if (col > start_col) {
            x += SEPARATOR_WIDTH;  // Add separator before this column
        }
        
        int col_width = viewer->display_state->col_widths[col];
        
        if (col == target_col) {
            // Check if the entire column fits
            return (x + col_width <= screen_width);
        }
        
        x += col_width;
        if (x >= screen_width) {
            return false;  // Already past screen edge
        }
    }
    
    return false;
}

// Helper function to find the leftmost start column that shows the target column fully
static size_t find_start_col_for_target(const DSVViewer *viewer, size_t target_col, int screen_width) {
    // First, try showing from column 0
    if (is_column_fully_visible(viewer, 0, target_col, screen_width)) {
        return 0;
    }
    
    // Binary search for the optimal start column
    size_t left = 0;
    size_t right = target_col;
    size_t best_start = target_col;  // Worst case: show only the target column
    
    while (left <= right && right < viewer->display_state->num_cols) {
        size_t mid = left + (right - left) / 2;
        
        if (is_column_fully_visible(viewer, mid, target_col, screen_width)) {
            best_start = mid;
            // Try to show more columns by starting earlier
            if (mid > 0) {
                right = mid - 1;
            } else {
                break;
            }
        } else {
            // Need to start later
            left = mid + 1;
        }
    }
    
    return best_start;
}

void navigate_up(ViewState *state) {
    if (state->table_view.cursor_row > 0) {
        state->table_view.cursor_row--;
        // Scroll up if cursor moves above viewport
        if (state->table_view.cursor_row < state->table_view.table_start_row) {
            state->table_view.table_start_row = state->table_view.cursor_row;
        }
    }
}

void navigate_down(ViewState *state, const struct DSVViewer *viewer) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int visible_rows = rows - 1; // Minus status line

    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    size_t data_rows = viewer->view_manager->current->visible_row_count;

    if (state->table_view.cursor_row + 1 < data_rows) {
        state->table_view.cursor_row++;
        if (state->table_view.cursor_row >= state->table_view.table_start_row + visible_rows) {
            state->table_view.table_start_row = state->table_view.cursor_row - visible_rows + 1;
        }
    }
}

void navigate_left(ViewState *state) {
    if (state->table_view.cursor_col > 0) {
        state->table_view.cursor_col--;
        if (state->table_view.cursor_col < state->table_view.table_start_col) {
            state->table_view.table_start_col = state->table_view.cursor_col;
        }
    }
}

void navigate_right(ViewState *state, const struct DSVViewer *viewer) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    if (state->table_view.cursor_col + 1 < viewer->display_state->num_cols) {
        state->table_view.cursor_col++;
        if (!is_column_fully_visible(viewer, state->table_view.table_start_col, 
                                    state->table_view.cursor_col, cols)) {
            state->table_view.table_start_col = find_start_col_for_target(
                viewer, state->table_view.cursor_col, cols);
        }
    }
}

void navigate_page_up(ViewState *state, const struct DSVViewer *viewer) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int visible_rows = rows - 1;

    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    if (state->table_view.table_start_row > (size_t)visible_rows) {
        state->table_view.table_start_row -= visible_rows;
    } else {
        state->table_view.table_start_row = 0;
    }
    if (state->table_view.cursor_row >= state->table_view.table_start_row + visible_rows) {
        state->table_view.cursor_row = state->table_view.table_start_row + visible_rows - 1;
    }
}

void navigate_page_down(ViewState *state, const struct DSVViewer *viewer) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int visible_rows = rows - 1;

    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    size_t data_rows = viewer->view_manager->current->visible_row_count;

    state->table_view.table_start_row += visible_rows;
    if (state->table_view.table_start_row >= data_rows) {
        state->table_view.table_start_row = (data_rows > (size_t)visible_rows) 
            ? data_rows - visible_rows : 0;
    }
    if (state->table_view.cursor_row < state->table_view.table_start_row) {
        state->table_view.cursor_row = state->table_view.table_start_row;
    }
    if (state->table_view.cursor_row >= data_rows && data_rows > 0) {
        state->table_view.cursor_row = data_rows - 1;
    }
}

void navigate_home(ViewState *state) {
    state->table_view.cursor_row = 0;
    state->table_view.cursor_col = 0;
    state->table_view.table_start_row = 0;
    state->table_view.table_start_col = 0;
}

void navigate_end(ViewState *state, const struct DSVViewer *viewer) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int visible_rows = rows - 1;

    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    size_t data_rows = viewer->view_manager->current->visible_row_count;

    state->table_view.cursor_row = (data_rows > 0) ? data_rows - 1 : 0;
    state->table_view.cursor_col = (viewer->display_state->num_cols > 0)
        ? viewer->display_state->num_cols - 1 : 0;
    
    if (data_rows > (size_t)visible_rows) {
        state->table_view.table_start_row = data_rows - visible_rows;
    } else {
        state->table_view.table_start_row = 0;
    }
    
    if (viewer->display_state->num_cols > 0) {
        state->table_view.table_start_col = find_start_col_for_target(
            viewer, state->table_view.cursor_col, cols);
    }
}

// Row selection functions
void init_row_selection(ViewState *state, size_t total_rows) {
    state->table_view.total_rows = total_rows;
    state->table_view.row_selected = calloc(total_rows, sizeof(bool));
    if (!state->table_view.row_selected) {
        // In a real app, you'd have better error handling.
        // For now, we'll proceed without selection capabilities.
        state->table_view.total_rows = 0;
        return;
    }
    state->table_view.selection_count = 0;
}

void cleanup_row_selection(ViewState *state) {
    if (state->table_view.row_selected) {
        free(state->table_view.row_selected);
        state->table_view.row_selected = NULL;
    }
    state->table_view.total_rows = 0;
    state->table_view.selection_count = 0;
}

void toggle_row_selection(ViewState *state, size_t row) {
    if (row >= state->table_view.total_rows) return;

    if (state->table_view.row_selected[row]) {
        state->table_view.row_selected[row] = false;
        state->table_view.selection_count--;
    } else {
        state->table_view.row_selected[row] = true;
        state->table_view.selection_count++;
    }
}

bool is_row_selected(const ViewState *state, size_t row) {
    if (row >= state->table_view.total_rows || !state->table_view.row_selected) {
        return false;
    }
    return state->table_view.row_selected[row];
}

void clear_all_selections(ViewState *state) {
    if (!state->table_view.row_selected) return;
    memset(state->table_view.row_selected, 0, state->table_view.total_rows * sizeof(bool));
    state->table_view.selection_count = 0;
}

size_t get_selected_rows(const ViewState *state, size_t **rows) {
    if (state->table_view.selection_count == 0) {
        *rows = NULL;
        return 0;
    }

    *rows = malloc(state->table_view.selection_count * sizeof(size_t));
    if (!*rows) {
        return 0; // Allocation failed
    }

    size_t count = 0;
    for (size_t i = 0; i < state->table_view.total_rows && count < state->table_view.selection_count; i++) {
        if (state->table_view.row_selected[i]) {
            (*rows)[count++] = i;
        }
    }
    return count;
} 