#include "viewer.h"
#include <wchar.h>
#include <string.h>

// FNV-1a hash function for cache lookups
static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 0x811c9dc5;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
}

// Helper function to calculate display width, backing off to strlen for invalid multi-byte chars
static int calculate_display_width(CSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, str, MAX_FIELD_LEN);
    int width = wcswidth(wcs, MAX_FIELD_LEN);
    return (width < 0) ? strlen(str) : width;
}

// Helper to allocate a DisplayCacheEntry from the memory pool
static DisplayCacheEntry* pool_alloc_entry(CSVViewer *viewer) {
    if (!viewer->mem_pool || !viewer->mem_pool->entry_pool) return NULL;

    if (viewer->mem_pool->entry_pool_used >= CACHE_ENTRY_POOL_SIZE) {
        return NULL; // Pool is full
    }
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

// Helper to copy a string into the string memory pool (replaces strdup)
static char* pool_strdup(CSVViewer *viewer, const char* str) {
    if (!viewer->mem_pool || !viewer->mem_pool->string_pool) return NULL;
    
    size_t len = strlen(str) + 1;
    if (viewer->mem_pool->string_pool_used + len > CACHE_STRING_POOL_SIZE) {
        return NULL; // Pool is full
    }

    char* dest = viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used;
    memcpy(dest, str, len);
    viewer->mem_pool->string_pool_used += len;

    return dest;
}

// Truncates a string using wide characters to respect display width
static void truncate_str(CSVViewer* viewer, const char* src, char* dest, int width) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, src, MAX_FIELD_LEN);

    int current_width = 0;
    int i = 0;
    for (i = 0; wcs[i] != '\0'; ++i) {
        int char_width = wcwidth(wcs[i]);
        if (char_width < 0) char_width = 1;

        if (current_width + char_width > width) {
            break;
        }
        current_width += char_width;
    }
    wcs[i] = '\0';

    wcstombs(dest, wcs, MAX_FIELD_LEN);
}

const char* get_truncated_string(CSVViewer *viewer, const char* original, int width) {
    if (!viewer->display_cache) return original;

    // Use a static buffer for non-cacheable results to avoid memory leaks
    static char static_buffer[MAX_FIELD_LEN];

    uint32_t hash = fnv1a_hash(original);
    uint32_t index = hash % CACHE_SIZE;

    // --- Cache Lookup ---
    DisplayCacheEntry *entry = viewer->display_cache->entries[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->original_string, original) == 0) {
            // Found the entry, now find the right truncated version
            for (int i = 0; i < entry->truncated_count; i++) {
                if (entry->truncated[i].width == width) {
                    return entry->truncated[i].str;
                }
            }
            // If we are here, we have the original string but not this specific width
            break; 
        }
        entry = entry->next;
    }

    // --- Cache Miss ---
    char *truncated_buffer = viewer->buffer_pool->buffer3;
    truncate_str(viewer, original, truncated_buffer, width);

    if (entry) { // Entry exists, but not this truncated version
        if (entry->truncated_count < MAX_TRUNCATED_VERSIONS) {
            int i = entry->truncated_count++;
            entry->truncated[i].width = width;
            char* pooled_str = pool_strdup(viewer, truncated_buffer);
            if (!pooled_str) { // Pool is full, return non-pooled buffer
                entry->truncated_count--;
                strncpy(static_buffer, truncated_buffer, MAX_FIELD_LEN - 1);
                static_buffer[MAX_FIELD_LEN - 1] = '\0';
                return static_buffer;
            }
            entry->truncated[i].str = pooled_str;
            return entry->truncated[i].str;
        }
        // Fallback: cannot store more versions. Copy to static buffer and return.
        strncpy(static_buffer, truncated_buffer, MAX_FIELD_LEN - 1);
        static_buffer[MAX_FIELD_LEN - 1] = '\0';
        return static_buffer;
    } else { // No entry for this string, create a new one
        DisplayCacheEntry *new_entry = pool_alloc_entry(viewer);
        if (!new_entry) { // Entry pool is full
            strncpy(static_buffer, truncated_buffer, MAX_FIELD_LEN - 1);
            static_buffer[MAX_FIELD_LEN - 1] = '\0';
            return static_buffer;
        }

        new_entry->hash = hash;
        new_entry->original_string = pool_strdup(viewer, original);
        new_entry->display_width = calculate_display_width(viewer, original);
        new_entry->truncated_count = 1;
        new_entry->truncated[0].width = width;
        new_entry->truncated[0].str = pool_strdup(viewer, truncated_buffer);
        
        // If any string allocation failed, the entry is effectively invalid.
        if (!new_entry->original_string || !new_entry->truncated[0].str) {
            // This is tricky because we can't easily "undo" the pool allocations.
            // For simplicity, we just won't add it to the cache.
            // The viewer will simply have a cache miss next time.
            strncpy(static_buffer, truncated_buffer, MAX_FIELD_LEN - 1);
            static_buffer[MAX_FIELD_LEN - 1] = '\0';
            return static_buffer;
        }
        
        // Add to hash table (handle collision)
        new_entry->next = viewer->display_cache->entries[index];
        viewer->display_cache->entries[index] = new_entry;

        return new_entry->truncated[0].str;
    }
}

// Unified function to draw any CSV line (header or data)
static void draw_csv_line(int y, CSVViewer *viewer, int file_line, int start_col, int is_header) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows; // Suppress unused warning
    
    int x = 0;
    int num_fields = parse_line(viewer, viewer->line_offsets[file_line], viewer->fields, MAX_COLS);
    
    if (is_header) {
        // Fill the entire header row with spaces to create continuous underline
        for (int i = 0; i < cols; i++) {
            mvaddch(y, i, ' ');
        }
    }
    
    // Use the unified logic for both header and data
    for (int col = start_col; col < num_fields; col++) {
        if (x >= cols) break;
        
        int col_width = (col < viewer->num_cols) ? viewer->col_widths[col] : 12;
        int original_col_width = col_width;
        
        if (is_header) {
            // Header truncation logic
            int separator_space = (col < num_fields - 1) ? 3 : 0;
            int needed_space = col_width + separator_space;
            if (x + needed_space > cols) {
                col_width = cols - x;
                if (col_width <= 0) break;
            }
        }
        
        char *rendered_field = viewer->buffer_pool->buffer1;
        render_field(&viewer->fields[col], rendered_field, MAX_FIELD_LEN);
        
        const char *display_string = get_truncated_string(viewer, rendered_field, col_width);
        
        if (is_header && col_width < original_col_width) {
            // Pad truncated header fields with spaces
            int text_len = strlen(display_string);
            char *padded_field = viewer->buffer_pool->buffer2;
            strcpy(padded_field, display_string);
            for (int i = text_len; i < col_width; i++) {
                padded_field[i] = ' ';
            }
            padded_field[col_width] = '\0';
            mvaddstr(y, x, padded_field);
        } else {
            mvaddstr(y, x, display_string);
        }

        x += col_width;
        
        // Unified separator logic - same for both header and data
        if (col < num_fields - 1 && x + 3 <= cols) {
            if (!is_header || col_width == original_col_width) {
                mvaddstr(y, x, separator);
            }
        }
        x += 3;
        
        if (is_header && col_width != original_col_width && x >= cols) {
            break;
        }
    }
}

void display_data(CSVViewer *viewer, int start_row, int start_col) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    clear();
    
    int display_rows = rows - 1;
    int screen_start_row = 0;
    
    if (show_header) {
        attron(COLOR_PAIR(1) | A_UNDERLINE);
        draw_csv_line(0, viewer, 0, start_col, 1); // 1 = is_header
        attroff(COLOR_PAIR(1) | A_UNDERLINE);
        screen_start_row = 1;
    }
    
    for (int screen_row = screen_start_row; screen_row < display_rows; screen_row++) {
        int file_line = start_row + screen_row - (show_header ? 0 : screen_start_row);
        if (file_line >= viewer->num_lines) break;

        draw_csv_line(screen_row, viewer, file_line, start_col, 0); // 0 = not header
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