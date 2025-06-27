#include "viewer.h"
#include <wchar.h>

void init_cache_memory_pool(CSVViewer *viewer) {
    viewer->mem_pool = malloc(sizeof(CacheMemoryPool));
    if (!viewer->mem_pool) {
        viewer->display_cache = NULL; // Prevent use
        return;
    }

    viewer->mem_pool->entry_pool = malloc(sizeof(DisplayCacheEntry) * CACHE_ENTRY_POOL_SIZE);
    viewer->mem_pool->entry_pool_used = 0;

    viewer->mem_pool->string_pool = malloc(CACHE_STRING_POOL_SIZE);
    viewer->mem_pool->string_pool_used = 0;

    // If any pool allocation fails, disable the cache entirely
    if (!viewer->mem_pool->entry_pool || !viewer->mem_pool->string_pool) {
        free(viewer->mem_pool->entry_pool);
        free(viewer->mem_pool->string_pool);
        free(viewer->mem_pool);
        viewer->mem_pool = NULL;
        viewer->display_cache = NULL;
    }
}

void cleanup_cache_memory_pool(CSVViewer *viewer) {
    if (!viewer->mem_pool) return;
    free(viewer->mem_pool->entry_pool);
    free(viewer->mem_pool->string_pool);
    free(viewer->mem_pool);
}

void init_buffer_pool(CSVViewer *viewer) {
    viewer->buffer_pool = malloc(sizeof(BufferPool));
    if (!viewer->buffer_pool) {
        // This is a critical failure, maybe exit or handle error appropriately
        perror("Failed to allocate buffer pool");
        exit(1);
    }
}

void cleanup_buffer_pool(CSVViewer *viewer) {
    if (viewer->buffer_pool) {
        free(viewer->buffer_pool);
    }
}

void init_display_cache(CSVViewer *viewer) {
    viewer->display_cache = malloc(sizeof(DisplayCache));
    if (viewer->display_cache) {
        for (int i = 0; i < CACHE_SIZE; i++) {
            viewer->display_cache->entries[i] = NULL;
        }
    }
}

void cleanup_display_cache(CSVViewer *viewer) {
    if (!viewer->display_cache) return;
    free(viewer->display_cache);
}

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
    
    // Allocate zero-copy field descriptors instead of field buffers
    viewer->fields = malloc(MAX_COLS * sizeof(FieldDesc));
    
    scan_file(viewer);
    init_buffer_pool(viewer);
    init_cache_memory_pool(viewer);
    init_display_cache(viewer);
    
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
        
        // Track column widths using zero-copy field width calculation
        for (int col = 0; col < num_fields && col < MAX_COLS; col++) {
            int display_width = calculate_field_display_width(viewer, &viewer->fields[col]);
            
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
        free(viewer->fields);
    }
    if (viewer->line_offsets) {
        free(viewer->line_offsets);
    }
    if (viewer->col_widths) {
        free(viewer->col_widths);
    }
    cleanup_display_cache(viewer);
    cleanup_cache_memory_pool(viewer);
    cleanup_buffer_pool(viewer);
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
    viewer->capacity = 8192;
    viewer->line_offsets = malloc(viewer->capacity * sizeof(size_t));
    viewer->num_lines = 0;
    viewer->line_offsets[viewer->num_lines++] = 0;

    // Fast line scanning using memchr - we'll handle multi-line records during parsing
    const size_t buffer_size = 4096;
    size_t temp_offsets[buffer_size];
    size_t temp_count = 0;

    char *p = viewer->data;
    char *end = viewer->data + viewer->length;
    
    while ((p = memchr(p, '\n', end - p))) {
        p++;
        if (p >= end) break;

        temp_offsets[temp_count++] = p - viewer->data;
        
        if (temp_count == buffer_size) {
            if (viewer->num_lines + temp_count > (size_t)viewer->capacity) {
                viewer->capacity = (viewer->num_lines + temp_count) * 1.5;
                viewer->line_offsets = realloc(viewer->line_offsets, viewer->capacity * sizeof(size_t));
            }
            memcpy(viewer->line_offsets + viewer->num_lines, temp_offsets, temp_count * sizeof(size_t));
            viewer->num_lines += temp_count;
            temp_count = 0;
        }
    }

    if (temp_count > 0) {
        if (viewer->num_lines + temp_count > (size_t)viewer->capacity) {
            viewer->capacity = viewer->num_lines + temp_count;
            viewer->line_offsets = realloc(viewer->line_offsets, viewer->capacity * sizeof(size_t));
        }
        memcpy(viewer->line_offsets + viewer->num_lines, temp_offsets, temp_count * sizeof(size_t));
        viewer->num_lines += temp_count;
    }
}

// Zero-copy parser - returns field descriptors pointing into original data
int parse_line(CSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    if (offset >= viewer->length) return 0;
    
    int field_count = 0;
    int in_quotes = 0;
    size_t i = offset;
    size_t field_start = offset;
    int has_escapes = 0;
    
    // Clear all field descriptors
    for (int j = 0; j < max_fields; j++) {
        fields[j].start = NULL;
        fields[j].length = 0;
        fields[j].needs_unescaping = 0;
    }
    
    while (i < viewer->length && field_count < max_fields) {
        char c = viewer->data[i];
        
        if (c == '"') {
            // Check for escaped quote (double quote)
            if (in_quotes && i + 1 < viewer->length && viewer->data[i + 1] == '"') {
                // Escaped quote - mark field as needing unescaping
                has_escapes = 1;
                i += 2; // Skip both quotes
                continue;
            } else {
                // Toggle quote state
                in_quotes = !in_quotes;
            }
        } else if (c == viewer->delimiter && !in_quotes) {
            // End current field - store field descriptor
            fields[field_count].start = viewer->data + field_start;
            fields[field_count].length = i - field_start;
            fields[field_count].needs_unescaping = has_escapes;
            
            field_count++;
            field_start = i + 1; // Start next field after delimiter
            has_escapes = 0;
        } else if (c == '\n') {
            if (!in_quotes) {
                // End of record
                break;
            }
            // Newline within quotes - continue parsing
        }
        i++;
    }
    
    // Finalize last field
    if (field_count < max_fields) {
        fields[field_count].start = viewer->data + field_start;
        fields[field_count].length = i - field_start;
        fields[field_count].needs_unescaping = has_escapes;
        field_count++;
    }
    
    return field_count;
}

// Render a field on-demand, handling quote removal and unescaping
char* render_field(const FieldDesc *field, char *buffer, size_t buffer_size) {
    if (!field->start || field->length == 0) {
        buffer[0] = '\0';
        return buffer;
    }
    
    const char *src = field->start;
    size_t src_len = field->length;
    size_t dst_pos = 0;
    
    // Remove surrounding quotes if present
    if (src_len >= 2 && src[0] == '"' && src[src_len-1] == '"') {
        src++;
        src_len -= 2;
    }
    
    if (field->needs_unescaping) {
        // Handle escaped quotes
        for (size_t i = 0; i < src_len && dst_pos < buffer_size - 1; i++) {
            if (src[i] == '"' && i + 1 < src_len && src[i + 1] == '"') {
                // Escaped quote - add one quote and skip the second
                buffer[dst_pos++] = '"';
                i++; // Skip the second quote
            } else if (src[i] == '\n') {
                // Convert newlines in quoted fields to spaces for display
                buffer[dst_pos++] = ' ';
            } else {
                buffer[dst_pos++] = src[i];
            }
        }
    } else {
        // Simple copy for fields without escapes
        size_t copy_len = src_len < buffer_size - 1 ? src_len : buffer_size - 1;
        memcpy(buffer, src, copy_len);
        dst_pos = copy_len;
    }
    
    buffer[dst_pos] = '\0';
    return buffer;
}

// Calculate display width of a field without fully rendering it
int calculate_field_display_width(CSVViewer *viewer, const FieldDesc *field) {
    if (!field->start || field->length == 0) {
        return 0;
    }
    
    // Quick estimation - for most fields this will be accurate
    size_t raw_len = field->length;
    
    // Remove surrounding quotes from length calculation
    if (raw_len >= 2 && field->start[0] == '"' && field->start[raw_len-1] == '"') {
        raw_len -= 2;
    }
    
    if (field->needs_unescaping) {
        // For fields with escapes, we need to render to get accurate width
        // This is the slow path, but only for fields with escaped quotes
        char *temp_buffer = viewer->buffer_pool->buffer1;
        render_field(field, temp_buffer, MAX_FIELD_LEN);
        
        wchar_t *wcs = viewer->buffer_pool->wide_buffer;
        mbstowcs(wcs, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        
        return display_width < 0 ? strlen(temp_buffer) : display_width;
    } else {
        // Fast path - no escaping needed, just measure raw content
        wchar_t *wcs = viewer->buffer_pool->wide_buffer;
        size_t copy_len = raw_len < MAX_FIELD_LEN - 1 ? raw_len : MAX_FIELD_LEN - 1;
        
        // Create temporary null-terminated string for mbstowcs
        char *temp = viewer->buffer_pool->buffer1;
        memcpy(temp, field->start + (field->length > raw_len ? 1 : 0), copy_len);
        temp[copy_len] = '\0';
        
        mbstowcs(wcs, temp, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        
        return display_width < 0 ? copy_len : display_width;
    }
} 