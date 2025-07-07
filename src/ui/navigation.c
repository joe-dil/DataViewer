#include "navigation.h"
#include "app_init.h"
#include "constants.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

    size_t data_rows = state->current_view->visible_row_count;

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
    if (!state->current_view || !state->current_view->data_source) return;
    DataSource *ds = state->current_view->data_source;
    size_t col_count = ds->ops->get_col_count(ds->context);

    if (state->table_view.cursor_col + 1 < col_count) {
        state->table_view.cursor_col++;
        
        // Super simple: if we have a reasonable number of columns visible (say 5),
        // and cursor goes beyond that, start scrolling
        // This avoids complex width calculations while providing smooth scrolling
        size_t visible_cols = 5; // Reasonable guess for most terminals
        
        if (state->table_view.cursor_col >= state->table_view.table_start_col + visible_cols) {
            // Scroll right to keep cursor in view
            state->table_view.table_start_col = state->table_view.cursor_col - visible_cols + 1;
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

    size_t data_rows = state->current_view->visible_row_count;

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
    if (!state->current_view || !state->current_view->data_source) return;
    DataSource *ds = state->current_view->data_source;
    size_t col_count = ds->ops->get_col_count(ds->context);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int visible_rows = rows - 1;

    if (viewer->display_state->show_header) {
        visible_rows--;
    }

    size_t data_rows = state->current_view->visible_row_count;

    state->table_view.cursor_row = (data_rows > 0) ? data_rows - 1 : 0;
    state->table_view.cursor_col = (col_count > 0) ? col_count - 1 : 0;
    
    if (data_rows > (size_t)visible_rows) {
        state->table_view.table_start_row = data_rows - visible_rows;
    } else {
        state->table_view.table_start_row = 0;
    }
    
    // Let the display logic handle the horizontal scroll position.
    state->table_view.table_start_col = state->table_view.cursor_col;
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