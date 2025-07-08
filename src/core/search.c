#include "core/search.h"
#include "core/data_source.h"
#include "core/parser.h"
#include "util/logging.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

SearchResult search_view(DSVViewer *viewer, View *view, const char* search_term, bool start_from_cursor) {
    if (!viewer || !view || !search_term || *search_term == '\0') {
        return SEARCH_NOT_FOUND;
    }

    DataSource* ds = view->data_source;
    if (!ds || view->visible_row_count == 0) return SEARCH_NOT_FOUND;

    size_t col_count = ds->ops->get_col_count(ds->context);
    if (col_count == 0) return SEARCH_NOT_FOUND;

    size_t start_r = view->cursor_row;
    size_t start_c = view->cursor_col;

    if (!start_from_cursor) {
        // Advance to the next cell for "find next"
        start_c++;
        if (start_c >= col_count) {
            start_c = 0;
            start_r++;
            if (start_r >= view->visible_row_count) {
                start_r = 0; // Wrap around
            }
        }
    }

    char cell_buffer[4096];
    size_t current_r = start_r;
    size_t current_c = start_c;
    bool wrapped = false;

    for (size_t i = 0; i < view->visible_row_count * col_count; i++) {
        size_t actual_row = view_get_displayed_row_index(view, current_r);
        if (actual_row != SIZE_MAX) {
            FieldDesc fd = ds->ops->get_cell(ds->context, actual_row, current_c);
            if (fd.start != NULL && fd.length > 0) {
                render_field(&fd, cell_buffer, sizeof(cell_buffer));
                if (strstr(cell_buffer, search_term) != NULL) {
                    // Match found!
                    view->cursor_row = current_r;
                    view->cursor_col = current_c;
                    LOG_INFO("Found '%s' at display row %zu, col %zu", search_term, current_r, current_c);
                    return wrapped ? SEARCH_WRAPPED_AND_FOUND : SEARCH_FOUND;
                }
            }
        }

        // Advance to the next cell, wrapping around rows and the whole view
        current_c++;
        if (current_c >= col_count) {
            current_c = 0;
            current_r++;
            if (current_r >= view->visible_row_count) {
                current_r = 0;
                wrapped = true;
            }
        }
    }

    set_error_message(viewer, "Search term not found: %s", search_term);
    return SEARCH_NOT_FOUND;
} 