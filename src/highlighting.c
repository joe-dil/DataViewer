#include "highlighting.h"
#include "constants.h"
#include <ncurses.h>

void apply_header_row_format(void) {
    attron(COLOR_PAIR(COLOR_PAIR_HEADER) | A_UNDERLINE);
}

void remove_header_row_format(void) {
    attroff(COLOR_PAIR(COLOR_PAIR_HEADER) | A_UNDERLINE);
}

void apply_row_highlight(int screen_row, int start_col, int width) {
    if (USE_INVERTED_HIGHLIGHT) {
        // Change attributes of the row up to the actual content width
        mvchgat(screen_row, start_col, width, A_REVERSE, 0, NULL);
    }
}

void apply_column_highlight(int screen_col, int col_width, int start_row, int end_row) {
    if (USE_INVERTED_HIGHLIGHT) {
        // Highlight the column from start_row to end_row
        for (int row = start_row; row < end_row; row++) {
            // Change attributes for the full column width
            mvchgat(row, screen_col, col_width, A_REVERSE, 0, NULL);
        }
    }
}

void apply_header_column_highlight(int screen_col, int col_width) {
    if (USE_INVERTED_HIGHLIGHT) {
        // For header columns, we need to maintain the underline while changing colors
        // We need to preserve the color pair and invert it
        for (int i = 0; i < col_width; i++) {
            // Get the character and its attributes at this position
            chtype ch = mvinch(0, screen_col + i);
            // Extract just the character
            chtype char_only = ch & A_CHARTEXT;
            // Extract the color pair
            chtype color_pair = ch & A_COLOR;
            // Apply inverted colors with underline, preserving the color pair
            mvaddch(0, screen_col + i, char_only | color_pair | A_REVERSE | A_UNDERLINE);
        }
    }
}

void apply_cell_highlight(int screen_row, int screen_col, int width) {
    if (USE_INVERTED_HIGHLIGHT) {
        // We'll implement this in Phase 8
        // For now, just a placeholder
        (void)screen_row;
        (void)screen_col;
        (void)width;
    }
} 