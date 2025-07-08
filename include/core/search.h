#ifndef SEARCH_H
#define SEARCH_H

#include "ui/view_state.h"
#include "app/app_init.h"
#include <stdbool.h>

// Enum to indicate the result of a search operation
typedef enum {
    SEARCH_NOT_FOUND,
    SEARCH_FOUND,
    SEARCH_WRAPPED_AND_FOUND
} SearchResult;

/**
 * @brief Searches the dataset for the given term, starting from the current
 *        cursor position.
 *
 * If a match is found, the view's cursor will be updated to the location
 * of the match.
 *
 * @param viewer The main application viewer instance.
 * @param view The view to search within.
 * @param search_term The string to search for.
 * @param start_from_cursor If true, starts from the current cell. If false,
 *                          starts from the cell after the cursor (for "find next").
 * @return A SearchResult indicating the outcome.
 */
SearchResult search_view(DSVViewer *viewer, View *view, const char* search_term, bool start_from_cursor);

#endif // SEARCH_H 