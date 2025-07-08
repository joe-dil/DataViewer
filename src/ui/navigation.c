#include "navigation.h"
#include "app_init.h"
#include "constants.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void navigate_up(ViewState *state) {
    if (state->current_view->cursor_row > 0) {
        state->current_view->cursor_row--;
        // Scroll up if cursor moves above viewport
        if (state->current_view->cursor_row < state->current_view->start_row) {
            state->current_view->start_row = state->current_view->cursor_row;
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

    if (state->current_view->cursor_row + 1 < data_rows) {
        state->current_view->cursor_row++;
        // Safe check to prevent overflow
        if (visible_rows > 0 && state->current_view->cursor_row > state->current_view->start_row && 
            state->current_view->cursor_row - state->current_view->start_row >= (size_t)visible_rows) {
            state->current_view->start_row = state->current_view->cursor_row - visible_rows + 1;
        }
    }
}

void navigate_left(ViewState *state) {
    if (state->current_view->cursor_col > 0) {
        state->current_view->cursor_col--;
        if (state->current_view->cursor_col < state->current_view->start_col) {
            state->current_view->start_col = state->current_view->cursor_col;
        }
    }
}

void navigate_right(ViewState *state, const struct DSVViewer *viewer) {
    (void)viewer; // Currently unused but kept for API consistency
    if (!state->current_view || !state->current_view->data_source) return;
    DataSource *ds = state->current_view->data_source;
    size_t col_count = ds->ops->get_col_count(ds->context);

    if (state->current_view->cursor_col + 1 < col_count) {
        state->current_view->cursor_col++;
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

    // Safe subtraction to prevent underflow
    if (visible_rows > 0 && state->current_view->start_row > (size_t)visible_rows) {
        state->current_view->start_row -= visible_rows;
    } else {
        state->current_view->start_row = 0;
    }
    
    // Ensure cursor stays within viewport
    if (visible_rows > 0 && state->current_view->cursor_row > state->current_view->start_row &&
        state->current_view->cursor_row - state->current_view->start_row >= (size_t)visible_rows) {
        state->current_view->cursor_row = state->current_view->start_row + visible_rows - 1;
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
    
    // Safe addition to prevent overflow
    size_t new_start_row;
    if (visible_rows > 0 && state->current_view->start_row <= SIZE_MAX - (size_t)visible_rows) {
        new_start_row = state->current_view->start_row + visible_rows;
    } else {
        new_start_row = SIZE_MAX;  // Saturate at max
    }
    
    state->current_view->start_row = new_start_row;
    if (state->current_view->start_row >= data_rows) {
        state->current_view->start_row = (data_rows > (size_t)visible_rows) 
            ? data_rows - visible_rows : 0;
    }
    if (state->current_view->cursor_row < state->current_view->start_row) {
        state->current_view->cursor_row = state->current_view->start_row;
    }
    if (state->current_view->cursor_row >= data_rows && data_rows > 0) {
        state->current_view->cursor_row = data_rows - 1;
    }
}

void navigate_home(ViewState *state) {
    state->current_view->cursor_row = 0;
    state->current_view->cursor_col = 0;
    state->current_view->start_row = 0;
    state->current_view->start_col = 0;
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

    state->current_view->cursor_row = (data_rows > 0) ? data_rows - 1 : 0;
    state->current_view->cursor_col = (col_count > 0) ? col_count - 1 : 0;
    
    // Safe calculation for start row
    if (visible_rows > 0 && data_rows > (size_t)visible_rows) {
        state->current_view->start_row = data_rows - visible_rows;
    } else {
        state->current_view->start_row = 0;
    }
    
    // Let the display logic handle the horizontal scroll position.
    state->current_view->start_col = state->current_view->cursor_col;
}

// Row selection functions
void init_row_selection(struct View *view, size_t total_rows) {
    if (!view) return;
    
    view->total_rows = total_rows;
    view->row_selected = calloc(total_rows, sizeof(bool));
    if (!view->row_selected) {
        // In a real app, you'd have better error handling.
        // For now, we'll proceed without selection capabilities.
        view->total_rows = 0;
        return;
    }
    view->selection_count = 0;
}

void cleanup_row_selection(struct View *view) {
    if (!view) return;
    
    if (view->row_selected) {
        free(view->row_selected);
        view->row_selected = NULL;
    }
    view->total_rows = 0;
    view->selection_count = 0;
}

void toggle_row_selection(struct View *view, size_t row) {
    if (!view || row >= view->total_rows) return;

    if (view->row_selected[row]) {
        view->row_selected[row] = false;
        view->selection_count--;
    } else {
        view->row_selected[row] = true;
        view->selection_count++;
    }
}

bool is_row_selected(const struct View *view, size_t row) {
    if (!view || row >= view->total_rows || !view->row_selected) {
        return false;
    }
    return view->row_selected[row];
}

void clear_all_selections(struct View *view) {
    if (!view || !view->row_selected) return;
    memset(view->row_selected, 0, view->total_rows * sizeof(bool));
    view->selection_count = 0;
}

size_t get_selected_rows(const struct View *view, size_t **rows) {
    if (!view || view->selection_count == 0) {
        *rows = NULL;
        return 0;
    }

    *rows = malloc(view->selection_count * sizeof(size_t));
    if (!*rows) {
        return 0; // Allocation failed
    }

    size_t count = 0;
    for (size_t i = 0; i < view->total_rows && count < view->selection_count; i++) {
        if (view->row_selected[i]) {
            (*rows)[count++] = i;
        }
    }
    return count;
} 