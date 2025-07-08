#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "view_state.h"
#include "core/data_source.h"

// A view represents a specific way of looking at a data source.
// This can be a direct view of a file, a filtered view, or a view
// of an in-memory table (like frequency analysis results).

// Represents a contiguous range of visible rows.
typedef struct {
    size_t start; // Inclusive start row index
    size_t end;   // Inclusive end row index
} RowRange;

// Enum for sort direction
typedef enum {
    SORT_NONE,
    SORT_ASC,
    SORT_DESC
} SortDirection;

typedef struct View {
    char name[64];
    DataSource *data_source;      // NEW: Data source for this view
    bool owns_data_source;        // NEW: If true, free on view cleanup

    // Row visibility and filtering.
    // Instead of a giant array of row indices, we store ranges of visible rows.
    RowRange *ranges;             // Array of visible row ranges
    size_t num_ranges;            // Number of ranges in the array
    size_t visible_row_count;     // Total number of rows across all ranges

    // Sorting state
    int sort_column;              // Column index for sorting, -1 if not sorted
    SortDirection sort_direction; // Direction of the sort
    size_t *row_order_map;        // Maps displayed row to an index in the visible set
    
    // Selection state - moved from global ViewState to per-view
    bool *row_selected;           // Bitmap for row selection in this view
    size_t selection_count;       // Number of selected rows in this view
    size_t total_rows;            // Total rows available for selection
    
    // Cursor position - per-view to handle different column structures
    size_t cursor_row;            // Current cursor row in this view
    size_t cursor_col;            // Current cursor column in this view
    size_t start_row;             // First visible row in viewport
    size_t start_col;             // First visible column in viewport
    
    // Deprecated fields, to be removed
    size_t *visible_rows;         // Use ranges instead
    
    struct View *next;
    struct View *prev;
} View;

typedef struct {
    View *views;          // Head of doubly-linked list
    View *current;
    size_t view_count;
    size_t max_views;     // Set to 10
} ViewManager;

ViewManager* init_view_manager(void);
void cleanup_view_manager(ViewManager *manager);
View* create_main_view(DataSource *data_source);
void switch_to_next_view(ViewManager *manager, ViewState *state);
void switch_to_prev_view(ViewManager *manager, ViewState *state);
void close_current_view(ViewManager *manager, ViewState *state);
View* create_view_from_selection(ViewManager *manager, ViewState *state, 
                                 size_t *selected_rows, size_t count,
                                 DataSource *parent_data_source);
void reset_view_state_for_new_view(ViewState *state, View *new_view);
bool add_view_to_manager(ViewManager *manager, View *view);

/**
 * @brief Translates a display row index (i.e., the nth visible row) to the
 *        actual row index in the underlying data source.
 *
 * @param view The view to query.
 * @param display_row The 0-based index of the visible row.
 * @return The corresponding row index in the data source, or SIZE_MAX if not found.
 */
size_t view_get_actual_row_index(const View *view, size_t display_row);

/**
 * @brief Gets the actual data source row index for a given displayed row,
 *        accounting for both filtering (ranges) and sorting.
 *
 * @param view The view to query.
 * @param display_row The 0-based index of the row as shown on screen.
 * @return The corresponding row index in the data source, or SIZE_MAX if not found.
 */
size_t view_get_displayed_row_index(const View *view, size_t display_row);

#endif // VIEW_MANAGER_H 