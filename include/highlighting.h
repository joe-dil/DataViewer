#ifndef HIGHLIGHTING_H
#define HIGHLIGHTING_H

#include <stddef.h>

/**
 * Apply row highlighting to a screen row
 * @param screen_row The screen row to highlight (0-based)
 * @param start_col Starting column position (usually 0)
 * @param width Width to highlight (actual content width, not full terminal)
 */
void apply_row_highlight(int screen_row, int start_col, int width);

/**
 * Apply column highlighting to a screen column
 * @param screen_col The screen column to highlight (0-based)
 * @param start_row Starting row for the highlight
 * @param end_row Ending row for the highlight (exclusive)
 */
void apply_column_highlight(int screen_col, int start_row, int end_row);

/**
 * Apply cell highlighting at the intersection of row and column
 * @param screen_row The screen row of the cell
 * @param screen_col The screen column of the cell
 * @param width Width of the cell to highlight
 */
void apply_cell_highlight(int screen_row, int screen_col, int width);

#endif // HIGHLIGHTING_H 