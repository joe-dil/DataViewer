#include "viewer.h"
#include "file_handler.h"
#include "file_scanner.h"
#include "column_analyzer.h"
#include <wchar.h>
#include <unistd.h> // For sysconf

// SIMD includes for vectorized parsing
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif

// Forward declaration for the SIMD helper function
static size_t simd_find_delimiter(const char *data, size_t length, char delimiter);

// Re-integrated from the now-deleted delimiter.c
static char detect_delimiter(const char *data, size_t length) {
    int comma_count = 0;
    int tab_count = 0;
    int pipe_count = 0;
    size_t scan_len = (length < 1024) ? length : 1024;

    for (size_t i = 0; i < scan_len; i++) {
        switch (data[i]) {
            case ',': comma_count++; break;
            case '\t': tab_count++; break;
            case '|': pipe_count++; break;
        }
    }

    if (tab_count > comma_count && tab_count > pipe_count) return '\t';
    if (pipe_count > comma_count && pipe_count > tab_count) return '|';
    return ',';
}

int init_viewer(DSVViewer *viewer, const char *filename, char delimiter) {
    if (load_file(viewer, filename) != 0) {
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
    // Buffer pool is now embedded, no init needed.
    
    // Determine the number of threads to use
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    viewer->num_threads = (num_cores > 0) ? (int)num_cores : 1;

    analyze_columns(viewer);
    
    // Lazy Allocation: Only initialize the display cache and its memory pool for larger files.
    if (viewer->num_lines > 500 || viewer->num_cols > 20) {
        init_cache_memory_pool((struct DSVViewer*)viewer);
        init_display_cache((struct DSVViewer*)viewer);
        init_string_intern_table((struct DSVViewer*)viewer);
    } else {
        viewer->mem_pool = NULL;
        viewer->display_cache = NULL;
        viewer->intern_table = NULL;
    }
    
    return 0;
}

static void cleanup_parser_resources(DSVViewer *viewer) {
    if (viewer->fields) {
        free(viewer->fields);
    }
    if (viewer->line_offsets) {
        free(viewer->line_offsets);
    }
    if (viewer->col_widths) {
        free(viewer->col_widths);
    }
}

void cleanup_viewer(DSVViewer *viewer) {
    unload_file(viewer);
    cleanup_parser_resources(viewer);
    cleanup_display_cache((struct DSVViewer*)viewer);
    cleanup_cache_memory_pool((struct DSVViewer*)viewer);
    // Buffer pool is now embedded, no cleanup needed.
    cleanup_string_intern_table((struct DSVViewer*)viewer);
}

// Zero-copy parser - returns field descriptors pointing into original data
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    if (offset >= viewer->length) return 0;
    
    size_t field_count = 0;
    int in_quotes = 0;
    size_t i = offset;
    size_t field_start = offset;
    int has_escapes = 0;
    
    // Memory prefetching for better cache performance
    const char *data_ptr = viewer->data + offset;
    size_t remaining = viewer->length - offset;
    if (remaining > 128) {
        __builtin_prefetch(data_ptr + 64, 0, 1); // Prefetch next cache line
        __builtin_prefetch(data_ptr + 128, 0, 1); // Prefetch another cache line
    }
    
    while (i < viewer->length && field_count < (size_t)max_fields) {
        // --- SIMD Accelerated Search ---
        // Only use SIMD when not inside quotes, as it simplifies the logic.
        if (!in_quotes) {
            size_t search_len = viewer->length - i;
            size_t next_special_char = simd_find_delimiter(viewer->data + i, search_len, viewer->delimiter);
            
            // If SIMD found a special character, jump to it.
            if (next_special_char > 0 && (i + next_special_char < viewer->length)) {
                i += next_special_char;
            }
        }

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
    if (field_count < (size_t)max_fields) {
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
int calculate_field_display_width(DSVViewer *viewer, const FieldDesc *field) {
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
        char *temp_buffer = viewer->buffer_pool.buffer_one;
        render_field(field, temp_buffer, MAX_FIELD_LEN);
        
        wchar_t *wcs = viewer->buffer_pool.wide_buffer;
        mbstowcs(wcs, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        
        return display_width < 0 ? strlen(temp_buffer) : display_width;
    } else {
        // Fast path - no escaping needed, just measure raw content
        wchar_t *wcs = viewer->buffer_pool.wide_buffer;
        size_t copy_len = raw_len < MAX_FIELD_LEN - 1 ? raw_len : MAX_FIELD_LEN - 1;
        
        // Create temporary null-terminated string for mbstowcs
        char *temp = viewer->buffer_pool.buffer_one;
        memcpy(temp, field->start + (field->length > raw_len ? 1 : 0), copy_len);
        temp[copy_len] = '\0';
        
        mbstowcs(wcs, temp, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        
        return display_width < 0 ? copy_len : display_width;
    }
}

// SIMD-optimized delimiter detection for long lines
static size_t simd_find_delimiter(const char *data, size_t length, char delimiter) {
#ifdef __AVX2__
    if (length >= 32) {
        __m256i delim_vec = _mm256_set1_epi8(delimiter);
        __m256i quote_vec = _mm256_set1_epi8('"');
        
        for (size_t i = 0; i <= length - 32; i += 32) {
            __m256i data_vec = _mm256_loadu_si256((__m256i*)(data + i));
            __m256i delim_cmp = _mm256_cmpeq_epi8(data_vec, delim_vec);
            __m256i quote_cmp = _mm256_cmpeq_epi8(data_vec, quote_vec);
            
            uint32_t delim_mask = _mm256_movemask_epi8(delim_cmp);
            uint32_t quote_mask = _mm256_movemask_epi8(quote_cmp);
            
            if (delim_mask || quote_mask) {
                // Found a character of interest, fall back to scalar processing from here
                return i + __builtin_ctz(delim_mask | quote_mask);
            }
        }
        return (length / 32) * 32; // Continue scalar processing from where SIMD left off
    }
#elif defined(__SSE2__)
    if (length >= 16) {
        __m128i delim_vec = _mm_set1_epi8(delimiter);
        __m128i quote_vec = _mm_set1_epi8('"');
        
        for (size_t i = 0; i <= length - 16; i += 16) {
            __m128i data_vec = _mm_loadu_si128((__m128i*)(data + i));
            __m128i delim_cmp = _mm_cmpeq_epi8(data_vec, delim_vec);
            __m128i quote_cmp = _mm_cmpeq_epi8(data_vec, quote_vec);
            
            uint16_t delim_mask = _mm_movemask_epi8(delim_cmp);
            uint16_t quote_mask = _mm_movemask_epi8(quote_cmp);
            
            if (delim_mask || quote_mask) {
                // Found a character of interest, fall back to scalar processing from here
                return i + __builtin_ctz(delim_mask | quote_mask);
            }
        }
        return (length / 16) * 16; // Continue scalar processing from where SIMD left off
    }
#endif
    // Suppress unused parameter warnings for non-SIMD builds or short strings.
    (void)data;
    (void)length;
    (void)delimiter;
    return 0; // No SIMD available or data too short
} 