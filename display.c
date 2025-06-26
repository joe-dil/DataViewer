#include "viewer.h"
#include <wchar.h>
#include <string.h>

// Helper function to truncate a string to a specific display width
static void truncate_to_width(const char* src, char* dest, int width) {
    wchar_t wcs[MAX_FIELD_LEN];
    mbstowcs(wcs, src, MAX_FIELD_LEN);
    
    int current_width = 0;
    int i = 0;
    for (i = 0; wcs[i] != '\0'; ++i) {
        int char_width = wcwidth(wcs[i]);
        if (char_width < 0) char_width = 1; // Fallback for control chars
        
        if (current_width + char_width > width) {
            break;
        }
        current_width += char_width;
    }
    wcs[i] = '\0';
    
    wcstombs(dest, wcs, MAX_FIELD_LEN);
}

void display_data(CSVViewer *viewer, int start_row, int start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    clear();
    
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (show_header) {
        int x = 0;
        int num_fields = parse_line(viewer, viewer->line_offsets[0], viewer->fields, MAX_COLS);
        
        // Draw header with underline attribute
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        
        // First, fill the entire header row with spaces to create continuous underline
        for (int i = 0; i < cols; i++) {
            mvaddch(0, i, ' ');
        }
        
        // Then draw the actual header text over the underlined spaces
        for (int col = start_col; col < num_fields; col++) {
            if (x >= cols) break;
            
            int col_width = (col < viewer->num_cols) ? viewer->col_widths[col] : 12;
            int original_col_width = col_width;
            
            // Check if we have enough space for this column + separator (if not last)
            int separator_space = (col < num_fields - 1) ? 3 : 0;
            int needed_space = col_width + separator_space;
            if (x + needed_space > cols) {
                // If this would overflow, use ALL remaining space for this column
                col_width = cols - x;
                if (col_width <= 0) break; // No space left
            }
            
            char truncated_field[MAX_FIELD_LEN];
            truncate_to_width(viewer->fields[col], truncated_field, col_width);
            
            // If this is a truncated field, pad it with spaces to prevent overlap
            if (col_width < original_col_width) {
                // For truncated columns, we need to clear the remaining space
                int text_len = strlen(truncated_field);
                for (int i = text_len; i < col_width; i++) {
                    truncated_field[i] = ' ';
                }
                truncated_field[col_width] = '\0';
            }
            
            mvaddstr(0, x, truncated_field);
            
            // Always advance by the full column width to prevent overlap
            // The spaces from our initial fill will show through for padding
            x += col_width;
            
            // Only draw separator if:
            // 1. Not the last column AND
            // 2. We have space for the separator AND 
            // 3. The column wasn't truncated (meaning we can fit more content)
            if (col < num_fields - 1 && x + 3 <= cols && col_width == original_col_width) {
                mvaddstr(0, x, separator);
                x += 3; // Use display width (3), not byte length
            }
            // If this column was truncated and uses all remaining space, we're done
            else if (col_width != original_col_width && x >= cols) {
                break;
            }
        }
        
        attroff(COLOR_PAIR(1) | A_UNDERLINE);
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        int file_line = start_row + screen_row - (show_header ? 0 : screen_start_row);
        if (file_line >= viewer->num_lines) break;

        int x = 0;
        int num_fields = parse_line(viewer, viewer->line_offsets[file_line], viewer->fields, MAX_COLS);
        for (int col = start_col; col < num_fields; col++) {
            if (x >= cols) break;
            
            int col_width = (col < viewer->num_cols) ? viewer->col_widths[col] : 12;
            
            char truncated_field[MAX_FIELD_LEN];
            truncate_to_width(viewer->fields[col], truncated_field, col_width);
            mvaddstr(screen_row, x, truncated_field);
            
            x += col_width;
            if (x < cols && col < num_fields - 1) {
                mvaddstr(screen_row, x, separator);
            }
            x += 3; // Use display width (3), not byte length
        }
    }
    
    mvprintw(rows - 1, 0, "Lines %d-%d of %d | Row: %d | Col: %d | q: quit | h: help",
             start_row + 1,
             start_row + display_rows > viewer->num_lines ? viewer->num_lines : start_row + display_rows,
             viewer->num_lines, start_row + 1, start_col + 1);
    
    refresh();
}

void show_help(void) {
    clear();
    mvprintw(1, 2, "CSV/PSV Viewer - Help");
    mvprintw(3, 2, "Navigation:");
    mvprintw(4, 4, "Arrow Keys    - Jump between fields/scroll");
    mvprintw(5, 4, "Page Up/Down  - Scroll pages");
    mvprintw(6, 4, "Home          - Go to beginning");
    mvprintw(7, 4, "End           - Go to end");
    mvprintw(8, 4, "Mouse Scroll  - Fine character movement");
    mvprintw(10, 2, "Commands:");
    mvprintw(11, 4, "q             - Quit");
    mvprintw(12, 4, "h             - Show this help");
    mvprintw(14, 2, "Features:");
    mvprintw(15, 4, "- Fast loading of large files (memory mapped)");
    mvprintw(16, 4, "- Auto-detection of delimiters (, | ; tab)");
    mvprintw(17, 4, "- Arrow keys jump between fields");
    mvprintw(18, 4, "- Mouse wheel for fine scrolling");
    mvprintw(20, 2, "Press any key to return...");
    refresh();
    getch();
} 