#ifndef SORTING_H
#define SORTING_H

#include "ui/view_manager.h"

// Forward declaration
struct DSVViewer;

/**
 * @brief Sorts a view based on its internal sort_column and sort_direction state.
 *
 * This function will allocate and populate the view's row_order_map. If the
 * view's sort direction is SORT_NONE, it will free the map.
 *
 * @param view The view to sort.
 */
void sort_view(View *view);

/**
 * @brief Checks if a column in a view is already sorted.
 *
 * @param view The view to check.
 * @param column_index The index of the column to check.
 * @param direction The direction to check for (SORT_ASC or SORT_DESC).
 * @return True if the column is sorted, false otherwise.
 */
bool is_column_sorted(View *view, int column_index, SortDirection direction);

#endif // SORTING_H