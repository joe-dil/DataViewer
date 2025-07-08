#ifndef SORTING_H
#define SORTING_H

#include "ui/view_manager.h"

/**
 * @brief Sorts a view based on its internal sort_column and sort_direction state.
 *
 * This function will allocate and populate the view's row_order_map. If the
 * view's sort direction is SORT_NONE, it will free the map.
 *
 * @param view The view to sort.
 */
void sort_view(View *view);

#endif // SORTING_H 