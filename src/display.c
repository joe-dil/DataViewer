#include "constants.h"
#include "viewer.h"
#include "display_state.h"
#include "utils.h"
#include <ncurses.h>
#include <wchar.h>
#include <string.h>

// Simple helper: get column width with fallback
static int get_column_width(DSVViewer *viewer, size_t col) {
    return (col < viewer->display_state->num_cols) ? viewer->display_state->col_widths[col] : DEFAULT_COL_WIDTH;
}

// Simple helper: render field content (common to both header and data)
static const char* render_column_content(DSVViewer *viewer, size_t col, int display_width) {
    char *rendered_field = viewer->display_state->buffers.render_buffer;
    render_field(&viewer->file_data->fields[col], rendered_field, viewer->config->max_field_len);
    return get_truncated_string(viewer, rendered_field, display_width);
}

// Simple helper: add separator if conditions are met
static void add_separator_if_needed(DSVViewer *viewer, int y, int x, size_t col, size_t num_fields, int cols) {
    if (col < num_fields - 1 && x + SEPARATOR_WIDTH <= cols) {
        mvaddstr(y, x, viewer->display_state->separator);
    }
}

// Phase 4: Calculate header layout and determine what columns fit
static void calculate_header_layout(DSVViewer *viewer, size_t start_col, int cols, HeaderLayout *layout) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(layout);
    
    layout->content_width = 0;
    layout->last_visible_col = start_col;
    layout->has_more_columns_right = false;
    
    layout->num_fields = parse_line(viewer, viewer->file_data->line_offsets[0], viewer->file_data->fields, viewer->config->max_cols);
    bool broke_early = false;
    
    for (size_t col = start_col; col < layout->num_fields; col++) {
        int col_width = get_column_width(viewer, col);
        int separator_space = (col < layout->num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        
        if (layout->content_width + needed_space > cols) {
            // If this column would extend past screen, truncate
            col_width = cols - layout->content_width - separator_space;
            if (col_width <= 0) {
                broke_early = true;
                break;
            }
            layout->content_width += col_width;
            layout->last_visible_col = col;
            broke_early = true;
            break;
        } else {
            layout->content_width += needed_space;
            layout->last_visible_col = col;
        }
    }
    
    // Check if there are more columns to the right
    layout->has_more_columns_right = broke_early || (layout->last_visible_col + 1 < layout->num_fields);
    
    // Extend underline to full width if there are more columns to the right
    layout->underline_width = layout->has_more_columns_right ? cols : layout->content_width;
}

// Phase 4: Render header background with proper underline
static void render_header_background(int y, int underline_width) {
    move(y, 0);
    clrtoeol();
    
    // Fill spaces for continuous underline
    for (int i = 0; i < underline_width; i++) {
        addch(' ');
    }
}

// Phase 4: Render actual header column content
static void render_header_columns(DSVViewer *viewer, int y, size_t start_col, int cols, const HeaderLayout *layout) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(layout);
    
    int x = 0;
    for (size_t col = start_col; col < layout->num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        int original_col_width = col_width;
        
        // Header truncation logic
        int separator_space = (col < layout->num_fields - 1) ? SEPARATOR_WIDTH : 0;
        int needed_space = col_width + separator_space;
        if (x + needed_space > cols) {
            col_width = cols - x;
            if (col_width <= 0) break;
        }
        
        const char *display_string = render_column_content(viewer, col, col_width);
        
        // Simplified padding logic for headers
        if (col_width < original_col_width) {
            // Only pad when truncated
            char *padded_field = viewer->display_state->buffers.pad_buffer;
            int text_len = strlen(display_string);
            int pad_width = (col_width < viewer->config->max_field_len) ? col_width : viewer->config->max_field_len - 1;
            
            strncpy(padded_field, display_string, pad_width);
            if (text_len < pad_width) {
                memset(padded_field + text_len, ' ', pad_width - text_len);
            }
            padded_field[pad_width] = '\0';
            mvaddstr(y, x, padded_field);
        } else {
            mvaddstr(y, x, display_string);
        }

        x += col_width;
        
        // Add separator if not truncated and not last column
        if (col < layout->num_fields - 1 && x + SEPARATOR_WIDTH <= cols && col_width == original_col_width) {
            mvaddstr(y, x, viewer->display_state->separator);
        }
        // Add final column separator if this is the last column
        else if (col == layout->num_fields - 1 && x + SEPARATOR_WIDTH <= cols && col_width == original_col_width) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
        }
        x += SEPARATOR_WIDTH;
        
        if (col_width != original_col_width && x >= cols) {
            break;
        }
    }
}

// Phase 4: Main header drawing function - now much simpler
static void draw_header_row(int y, DSVViewer *viewer, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(config);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    HeaderLayout layout;
    calculate_header_layout(viewer, start_col, cols, &layout);
    render_header_background(y, layout.underline_width);
    render_header_columns(viewer, y, start_col, cols, &layout);
}

// Draw regular data row (keep simple, it works!)
static void draw_data_row(int y, DSVViewer *viewer, size_t file_line, size_t start_col, const DSVConfig *config) {
    CHECK_NULL_RET_VOID(viewer);
    CHECK_NULL_RET_VOID(config);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[file_line], viewer->file_data->fields, config->max_cols);
    
    for (size_t col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = get_column_width(viewer, col);
        
        const char *display_string = render_column_content(viewer, col, col_width);
        mvaddstr(y, x, display_string);
        x += col_width;
        
        // Add separator if not last column and space available  
        add_separator_if_needed(viewer, y, x, col, num_fields, cols);
        // Add final column separator if this is the last column
        if (col == num_fields - 1 && x + SEPARATOR_WIDTH <= cols) {
            const char* final_separator = viewer->display_state->supports_unicode ? "║" : ASCII_SEPARATOR;
            mvaddstr(y, x, final_separator);
        }
        x += SEPARATOR_WIDTH;
    }
}

void display_data(DSVViewer *viewer, size_t start_row, size_t start_col) {
    CHECK_NULL_RET_VOID(viewer);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // ANTI-FLICKER FIX: Replace clear() with line-by-line clearing
    // Only clear lines that will be used to avoid full screen flash
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (viewer->display_state->show_header) {
        attron(COLOR_PAIR(COLOR_PAIR_HEADER) | A_UNDERLINE);
        draw_header_row(0, viewer, start_col, viewer->config);
        attroff(COLOR_PAIR(COLOR_PAIR_HEADER) | A_UNDERLINE);
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        // Clear each line before drawing to prevent artifacts
        move(screen_row, 0);
        clrtoeol();
        
        size_t file_line = start_row + screen_row - (viewer->display_state->show_header ? 0 : screen_start_row);
        if (file_line >= viewer->file_data->num_lines) {
            // Clear remaining lines if no more data
            continue;
        }

        draw_data_row(screen_row, viewer, file_line, start_col, viewer->config);
    }
    
    // Clear status line before updating
    move(rows - 1, 0);
    clrtoeol();
    
    // Use %zu for size_t types
    mvprintw(rows - 1, 0, "Lines %zu-%zu of %zu | Row: %zu | Col: %zu | q: quit | h: help",
             start_row + 1,
             (start_row + display_rows > viewer->file_data->num_lines) ? viewer->file_data->num_lines : start_row + display_rows,
             viewer->file_data->num_lines, start_row + 1, start_col + 1);
    
    refresh();
}

void show_help(void) {
    clear();
    mvprintw(1, HELP_INDENT_COL, "DSV (Delimiter-Separated Values) Viewer - Help");
    mvprintw(3, HELP_INDENT_COL, "Navigation:");
    mvprintw(4, HELP_ITEM_INDENT_COL, "Arrow Keys    - Jump between fields/scroll");
    mvprintw(5, HELP_ITEM_INDENT_COL, "Page Up/Down  - Scroll pages");
    mvprintw(6, HELP_ITEM_INDENT_COL, "Home          - Go to beginning");
    mvprintw(7, HELP_ITEM_INDENT_COL, "End           - Go to end");
    mvprintw(8, HELP_ITEM_INDENT_COL, "Mouse Scroll  - Fine character movement");
    mvprintw(HELP_COMMANDS_ROW, HELP_INDENT_COL, "Commands:");
    mvprintw(11, HELP_ITEM_INDENT_COL, "q             - Quit");
    mvprintw(12, HELP_ITEM_INDENT_COL, "h             - Show this help");
    mvprintw(HELP_FEATURES_ROW, HELP_INDENT_COL, "Features:");
    mvprintw(15, HELP_ITEM_INDENT_COL, "- Fast loading of large files (memory mapped)");
    mvprintw(16, HELP_ITEM_INDENT_COL, "- Auto-detection of delimiters (, | ; tab)");
    mvprintw(17, HELP_ITEM_INDENT_COL, "- Arrow keys jump between fields");
    mvprintw(18, HELP_ITEM_INDENT_COL, "- Mouse wheel for fine scrolling");
    mvprintw(20, HELP_INDENT_COL, "Press any key to return...");
    refresh();
    getch();
} 