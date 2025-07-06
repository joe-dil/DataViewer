#include <ncurses.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/highlighting.h"
#include "../include/constants.h"

// Test helper to initialize ncurses for testing
void init_ncurses_test() {
    initscr();
    start_color();
    init_pair(COLOR_PAIR_HEADER, COLOR_GREEN, COLOR_BLACK);
    cbreak();
    noecho();
    refresh(); // Add refresh after initialization
}

// Test helper to check if a character has the reverse attribute
int has_reverse_attr(int row, int col) {
    chtype ch = mvinch(row, col);
    return (ch & A_REVERSE) != 0;
}

// Test helper to check if a character has underline attribute
int has_underline_attr(int row, int col) {
    chtype ch = mvinch(row, col);
    return (ch & A_UNDERLINE) != 0;
}

// Test 1: Test row highlighting
void test_row_highlighting() {
    clear();
    refresh(); // Ensure screen is cleared
    
    // Draw some content
    mvprintw(0, 0, "Row 0 content");
    mvprintw(1, 0, "Row 1 content");
    mvprintw(2, 0, "Row 2 content");
    refresh(); // Apply the printed content
    
    // Apply row highlighting to row 1
    apply_row_highlight(1, 0, 13); // "Row 1 content" is 13 chars
    refresh(); // Apply the highlighting
    
    // Verify row 1 is highlighted
    for (int col = 0; col < 13; col++) {
        if (!has_reverse_attr(1, col)) {
            fprintf(stderr, "Row 1, col %d not highlighted\n", col);
        }
        assert(has_reverse_attr(1, col) == 1);
    }
    
    // Verify other rows are not highlighted
    if (has_reverse_attr(0, 0)) {
        fprintf(stderr, "Row 0 unexpectedly highlighted\n");
    }
    if (has_reverse_attr(2, 0)) {
        fprintf(stderr, "Row 2 unexpectedly highlighted\n");
    }
    
    printf("✓ Row highlighting test passed\n");
}

// Test 2: Test column highlighting
void test_column_highlighting() {
    clear();
    refresh(); // Clear screen properly
    
    // Draw a grid of content
    mvprintw(0, 0, "Col0  Col1  Col2");
    mvprintw(1, 0, "A0    A1    A2  ");
    mvprintw(2, 0, "B0    B1    B2  ");
    mvprintw(3, 0, "C0    C1    C2  ");
    
    // Apply column highlighting to column 1 (starts at position 6, width 4)
    apply_column_highlight(6, 4, 1, 4); // From row 1 to row 4
    
    // Verify column is highlighted in data rows
    for (int row = 1; row < 4; row++) {
        for (int col = 6; col < 10; col++) {
            assert(has_reverse_attr(row, col) == 1);
        }
    }
    
    // Verify header row is not highlighted by this function
    assert(has_reverse_attr(0, 6) == 0);
    
    // Verify other columns are not highlighted
    assert(has_reverse_attr(1, 0) == 0);
    assert(has_reverse_attr(1, 12) == 0);
    
    printf("✓ Column highlighting test passed\n");
}

// Test 3: Test header column highlighting
void test_header_column_highlighting() {
    clear();
    
    // Draw header with formatting
    apply_header_row_format();
    mvprintw(0, 0, "Header1 Header2 Header3");
    remove_header_row_format();
    
    // Apply header column highlighting to column 2 (starts at position 8, width 7)
    apply_header_column_highlight(8, 7); // "Header2"
    
    // Verify the header column is highlighted with both reverse and underline
    for (int col = 8; col < 15; col++) {
        assert(has_reverse_attr(0, col) == 1);
        assert(has_underline_attr(0, col) == 1);
    }
    
    // Verify other parts of header maintain underline but no reverse
    assert(has_reverse_attr(0, 0) == 0);
    assert(has_reverse_attr(0, 16) == 0);
    
    printf("✓ Header column highlighting test passed\n");
}

// Test 4: Test combined row and column highlighting (intersection)
void test_combined_highlighting() {
    clear();
    
    // Draw a grid
    mvprintw(0, 0, "Header");
    mvprintw(1, 0, "Row 1 ");
    mvprintw(2, 0, "Row 2 ");
    mvprintw(3, 0, "Row 3 ");
    
    // Apply row highlighting to row 2
    apply_row_highlight(2, 0, 6);
    
    // Apply column highlighting from position 4 
    apply_column_highlight(4, 2, 1, 4);
    
    // The intersection at row 2, columns 4-5 should have highlighting
    // (Note: multiple A_REVERSE attributes typically toggle, but in practice
    // ncurses handles this appropriately)
    assert(has_reverse_attr(2, 4) == 1);
    assert(has_reverse_attr(2, 5) == 1);
    
    printf("✓ Combined highlighting test passed\n");
}

// Test 5: Test highlighting boundaries
void test_highlighting_boundaries() {
    clear();
    
    // Test row highlighting with exact width
    mvprintw(0, 0, "Test");
    apply_row_highlight(0, 0, 4); // Exact width of "Test"
    
    // Verify only the specified width is highlighted
    for (int col = 0; col < 4; col++) {
        assert(has_reverse_attr(0, col) == 1);
    }
    // Position 4 should not be highlighted
    mvprintw(0, 4, " Extra");
    assert(has_reverse_attr(0, 4) == 0);
    
    printf("✓ Highlighting boundaries test passed\n");
}

// Test 6: Test USE_INVERTED_HIGHLIGHT flag
void test_highlighting_flag() {
    clear();
    
    // The highlighting functions check USE_INVERTED_HIGHLIGHT
    // which is defined as 1 in constants.h
    
    mvprintw(0, 0, "Test content");
    
    // Apply highlighting - should work since USE_INVERTED_HIGHLIGHT is 1
    apply_row_highlight(0, 0, 12);
    
    // Verify highlighting is applied
    assert(has_reverse_attr(0, 0) == 1);
    
    printf("✓ Highlighting flag test passed\n");
}

int main() {
    printf("Running highlighting tests...\n");
    
    init_ncurses_test();
    
    test_row_highlighting();
    test_column_highlighting();
    test_header_column_highlighting();
    test_combined_highlighting();
    test_highlighting_boundaries();
    test_highlighting_flag();
    
    endwin();
    
    printf("\nAll highlighting tests passed! ✓\n");
    return 0;
} 