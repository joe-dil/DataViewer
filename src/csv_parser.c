#include "viewer.h"
#include <wchar.h>

int init_viewer(CSVViewer *viewer, const char *filename, char delimiter) {
    struct stat st;
    
    viewer->fd = open(filename, O_RDONLY);
    if (viewer->fd == -1) {
        perror("Failed to open file");
        return -1;
    }
    
    if (fstat(viewer->fd, &st) == -1) {
        perror("Failed to get file stats");
        close(viewer->fd);
        return -1;
    }
    
    viewer->length = st.st_size;
    
    viewer->data = mmap(NULL, viewer->length, PROT_READ, MAP_PRIVATE, viewer->fd, 0);
    if (viewer->data == MAP_FAILED) {
        perror("Failed to map file");
        close(viewer->fd);
        return -1;
    }
    
    // Auto-detect delimiter if not specified
    if (delimiter == 0) {
        viewer->delimiter = detect_delimiter(viewer->data, viewer->length);
    } else {
        viewer->delimiter = delimiter;
    }
    
    viewer->fields = malloc(MAX_COLS * sizeof(char*));
    for (int i = 0; i < MAX_COLS; i++) {
        viewer->fields[i] = malloc(MAX_FIELD_LEN);
    }
    
    viewer->capacity = 1000;
    viewer->line_offsets = malloc(viewer->capacity * sizeof(size_t));
    viewer->num_lines = 0;
    
    scan_file(viewer);
    
    // Single-pass column detection and width calculation (first 1000 lines)
    viewer->num_cols = 0;
    int sample_size = viewer->num_lines < 1000 ? viewer->num_lines : 1000;
    
    // Temporary width tracking
    int temp_widths[MAX_COLS];
    for (int i = 0; i < MAX_COLS; i++) {
        temp_widths[i] = 8; // Default width
    }
    
    // Single pass through sample lines
    for (int i = 0; i < sample_size; i++) {
        int num_fields = parse_line(viewer, viewer->line_offsets[i], 
                                  viewer->fields, MAX_COLS);
        
        // Track max columns
        if (num_fields > viewer->num_cols) {
            viewer->num_cols = num_fields;
        }
        
        // Track column widths using display width, not byte length
        for (int col = 0; col < num_fields && col < MAX_COLS; col++) {
            wchar_t wcs[MAX_FIELD_LEN];
            mbstowcs(wcs, viewer->fields[col], MAX_FIELD_LEN);
            int display_width = wcswidth(wcs, MAX_FIELD_LEN);
            
            if (display_width < 0) { // Fallback for invalid chars
                display_width = strlen(viewer->fields[col]);
            }

            if (display_width > temp_widths[col]) {
                temp_widths[col] = display_width;
            }
        }
    }
    
    // Copy calculated widths with a cap of 16
    viewer->col_widths = malloc(viewer->num_cols * sizeof(int));
    for (int i = 0; i < viewer->num_cols; i++) {
        viewer->col_widths[i] = temp_widths[i] > 16 ? 16 :  // Cap at 16 chars max
                               (temp_widths[i] < 4 ? 4 : temp_widths[i]);  // Min 4 chars
    }
    
    return 0;
}

void cleanup_viewer(CSVViewer *viewer) {
    if (viewer->data != MAP_FAILED) {
        munmap(viewer->data, viewer->length);
    }
    if (viewer->fd != -1) {
        close(viewer->fd);
    }
    if (viewer->fields) {
        for (int i = 0; i < MAX_COLS; i++) {
            free(viewer->fields[i]);
        }
        free(viewer->fields);
    }
    if (viewer->line_offsets) {
        free(viewer->line_offsets);
    }
    if (viewer->col_widths) {
        free(viewer->col_widths);
    }
}

char detect_delimiter(const char *data, size_t length) {
    int comma_count = 0, pipe_count = 0, tab_count = 0, semicolon_count = 0;
    size_t sample_size = length > 10000 ? 10000 : length;
    
    // Only scan the first line for delimiters
    for (size_t i = 0; i < sample_size && data[i] != '\n'; i++) {
        switch (data[i]) {
            case ',': comma_count++; break;
            case '|': pipe_count++; break;
            case '\t': tab_count++; break;
            case ';': semicolon_count++; break;
        }
    }
    
    if (pipe_count > comma_count && pipe_count > tab_count && pipe_count > semicolon_count) {
        return '|';
    } else if (tab_count > comma_count && tab_count > semicolon_count) {
        return '\t';
    } else if (semicolon_count > comma_count) {
        return ';';
    }
    return ','; // Default to comma
}

void scan_file(CSVViewer *viewer) {
    viewer->num_lines = 0;
    
    // Always include offset 0 (first line)
    viewer->line_offsets[viewer->num_lines++] = 0;
    
    for (size_t i = 0; i < viewer->length; i++) {
        if (viewer->data[i] == '\n') {
            if (i + 1 < viewer->length) { // Don't add if it's the last character
                if (viewer->num_lines >= viewer->capacity) {
                    viewer->capacity *= 2;
                    viewer->line_offsets = realloc(viewer->line_offsets, 
                                                 viewer->capacity * sizeof(size_t));
                }
                viewer->line_offsets[viewer->num_lines++] = i + 1;
            }
        }
    }
}

int parse_line(CSVViewer *viewer, size_t offset, char **fields, int max_fields) {
    if (offset >= viewer->length) return 0;
    
    int field_count = 0;
    int field_pos = 0;
    int in_quotes = 0;
    size_t i = offset;
    
    // Clear ALL fields, not just the first one
    for (int j = 0; j < max_fields; j++) {
        fields[j][0] = '\0';
    }
    
    while (i < viewer->length && viewer->data[i] != '\n' && field_count < max_fields) {
        char c = viewer->data[i];
        
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == viewer->delimiter && !in_quotes) {
            // End current field
            fields[field_count][field_pos] = '\0';
            field_count++;
            field_pos = 0;
        } else if (field_pos < MAX_FIELD_LEN - 1) {
            // Add character to current field
            fields[field_count][field_pos++] = c;
        }
        i++;
    }
    
    // Finalize last field
    if (field_count < max_fields) {
        fields[field_count][field_pos] = '\0';
        field_count++;
    }
    
    return field_count;
} 