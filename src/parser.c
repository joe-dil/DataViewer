#include "viewer.h"
#include "display_state.h"
#include <wchar.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/time.h>

// Constants to replace magic numbers
#define DELIMITER_DETECTION_SAMPLE_SIZE 1024
#define LINE_ESTIMATION_SAMPLE_SIZE 65536
#define LINE_CAPACITY_GROWTH_FACTOR 1.2
#define DEFAULT_CHARS_PER_LINE 80
#define COLUMN_ANALYSIS_SAMPLE_LINES 1000
#define MAX_COLUMN_WIDTH 16
#define MIN_COLUMN_WIDTH 4
#define CACHE_THRESHOLD_LINES 500
#define CACHE_THRESHOLD_COLS 20

// Simple timing utility for profiling
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// --- Static Helper Declarations ---
static char detect_delimiter(const char *data, size_t length);
static int load_file(struct DSVViewer *viewer, const char *filename);
static void unload_file(struct DSVViewer *viewer);
static int scan_file(DSVViewer *viewer);
static int analyze_columns(struct DSVViewer *viewer);


// --- Delimiter Detection ---
static char detect_delimiter(const char *data, size_t length) {
    int comma_count = 0, tab_count = 0, pipe_count = 0;
    size_t scan_len = (length < DELIMITER_DETECTION_SAMPLE_SIZE) ? length : DELIMITER_DETECTION_SAMPLE_SIZE;
    for (size_t i = 0; i < scan_len; i++) {
        if (data[i] == ',') comma_count++;
        else if (data[i] == '\t') tab_count++;
        else if (data[i] == '|') pipe_count++;
    }
    if (tab_count > comma_count && tab_count > pipe_count) return '\t';
    if (pipe_count > comma_count && pipe_count > tab_count) return '|';
    return ',';
}

// --- File Handling ---
static int load_file(struct DSVViewer *viewer, const char *filename) {
    struct stat st;
    viewer->file_data->fd = open(filename, O_RDONLY);
    if (viewer->file_data->fd == -1) { perror("Failed to open file"); return -1; }
    if (fstat(viewer->file_data->fd, &st) == -1) { perror("Failed to get file stats"); close(viewer->file_data->fd); return -1; }
    viewer->file_data->length = st.st_size;
    viewer->file_data->data = mmap(NULL, viewer->file_data->length, PROT_READ, MAP_PRIVATE, viewer->file_data->fd, 0);
    if (viewer->file_data->data == MAP_FAILED) { perror("Failed to map file"); close(viewer->file_data->fd); return -1; }
    return 0;
}

static void unload_file(struct DSVViewer *viewer) {
    if (viewer->file_data->data != MAP_FAILED) munmap(viewer->file_data->data, viewer->file_data->length);
    if (viewer->file_data->fd != -1) close(viewer->file_data->fd);
}

// --- File Scanning ---
static size_t estimate_line_count(DSVViewer *viewer) {
    const size_t sample_size = (viewer->file_data->length < LINE_ESTIMATION_SAMPLE_SIZE) ? viewer->file_data->length : LINE_ESTIMATION_SAMPLE_SIZE;
    if (sample_size == 0) return 1;
    
    // Count newlines in sample using memchr (much faster than byte-by-byte)
    int sample_lines = 0;
    const char *search_start = viewer->file_data->data;
    const char *search_end = viewer->file_data->data + sample_size;
    
    while (search_start < search_end) {
        const char *newline = memchr(search_start, '\n', search_end - search_start);
        if (!newline) break;
        sample_lines++;
        search_start = newline + 1;
    }
    
    if (sample_lines == 0) return (viewer->file_data->length / DEFAULT_CHARS_PER_LINE) + 1;
    double avg_line_len = (double)sample_size / sample_lines;
    return (size_t)((viewer->file_data->length / avg_line_len) * LINE_CAPACITY_GROWTH_FACTOR) + 1;
}

static int scan_file(DSVViewer *viewer) {
    viewer->file_data->capacity = estimate_line_count(viewer);
    viewer->file_data->line_offsets = malloc(viewer->file_data->capacity * sizeof(size_t));
    if (!viewer->file_data->line_offsets) {
        fprintf(stderr, "Failed to allocate memory for line offsets\n");
        return -1;
    }
    
    viewer->file_data->num_lines = 0;
    viewer->file_data->line_offsets[viewer->file_data->num_lines++] = 0;
    
    // Fast newline scanning using memchr for large chunks
    const char *search_start = viewer->file_data->data;
    const char *file_end = viewer->file_data->data + viewer->file_data->length;
    
    while (search_start < file_end) {
        const char *newline = memchr(search_start, '\n', file_end - search_start);
        if (!newline) break;
        
        // Check if we need to expand capacity
        if (viewer->file_data->num_lines >= viewer->file_data->capacity) {
            viewer->file_data->capacity *= 2;
            size_t *new_offsets = realloc(viewer->file_data->line_offsets, viewer->file_data->capacity * sizeof(size_t));
            if (!new_offsets) {
                fprintf(stderr, "Failed to expand line offsets array\n");
                return -1;
            }
            viewer->file_data->line_offsets = new_offsets;
        }
        
        // Store offset to character after newline
        size_t next_line_offset = (newline - viewer->file_data->data) + 1;
        if (next_line_offset < viewer->file_data->length) {
            viewer->file_data->line_offsets[viewer->file_data->num_lines++] = next_line_offset;
        }
        
        search_start = newline + 1;
    }
    
    // Trim capacity to actual size
    if (viewer->file_data->capacity > viewer->file_data->num_lines) {
        size_t *trimmed_offsets = realloc(viewer->file_data->line_offsets, viewer->file_data->num_lines * sizeof(size_t));
        if (trimmed_offsets) {
            viewer->file_data->line_offsets = trimmed_offsets;
        }
        // If realloc fails for trimming, we just keep the larger array - not a critical error
    }
    
    return 0;
}

// --- Column Analysis ---
static int analyze_columns(struct DSVViewer *viewer) {
    // Simple and fast: analyze first N lines max, single threaded
    size_t sample_lines = viewer->file_data->num_lines > COLUMN_ANALYSIS_SAMPLE_LINES ? COLUMN_ANALYSIS_SAMPLE_LINES : viewer->file_data->num_lines;
    
    int col_widths[MAX_COLS] = {0};
    size_t max_cols = 0;
    FieldDesc fields[MAX_COLS];
    
    for (size_t i = 0; i < sample_lines; i++) {
        size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[i], fields, MAX_COLS);
        if (num_fields > max_cols) max_cols = num_fields;
        
        for (size_t col = 0; col < num_fields && col < MAX_COLS; col++) {
            if (col_widths[col] >= MAX_COLUMN_WIDTH) continue; // Cap at max chars
            int width = calculate_field_display_width(viewer, &fields[col]);
            if (width > col_widths[col]) col_widths[col] = width;
        }
    }
    
    // Set final results
    viewer->display_state->num_cols = max_cols > MAX_COLS ? MAX_COLS : max_cols;
    viewer->display_state->col_widths = malloc(viewer->display_state->num_cols * sizeof(int));
    if (!viewer->display_state->col_widths) {
        fprintf(stderr, "Failed to allocate memory for column widths\n");
        return -1;
    }
    
    for (size_t i = 0; i < viewer->display_state->num_cols; i++) {
        viewer->display_state->col_widths[i] = col_widths[i] > MAX_COLUMN_WIDTH ? MAX_COLUMN_WIDTH : 
                               (col_widths[i] < MIN_COLUMN_WIDTH ? MIN_COLUMN_WIDTH : col_widths[i]);
    }
    
    return 0;
}

// --- Main Initialization Orchestrator ---
int init_viewer(DSVViewer *viewer, const char *filename, char delimiter) {
    double start_time = get_time_ms();
    double phase_time;
    int ret = -1; // Default to failure

    // Ensure all pointers are NULL so cleanup is safe
    memset(viewer, 0, sizeof(DSVViewer));
    
    // -- Initialize new DisplayState component --
    viewer->display_state = malloc(sizeof(DisplayState));
    if (!viewer->display_state) {
        perror("Failed to allocate for DisplayState");
        goto cleanup;
    }
    memset(viewer->display_state, 0, sizeof(DisplayState));
    
    // -- Initialize new FileAndParseData component --
    viewer->file_data = malloc(sizeof(FileAndParseData));
    if (!viewer->file_data) {
        perror("Failed to allocate for FileAndParseData");
        goto cleanup;
    }
    memset(viewer->file_data, 0, sizeof(FileAndParseData));
    
    // Phase 1: File loading
    double load_start = get_time_ms();
    if (load_file(viewer, filename) != 0) goto cleanup;
    phase_time = get_time_ms() - load_start;
    printf("File loading: %.2f ms\n", phase_time);
    
    // Phase 2: Delimiter detection
    double delim_start = get_time_ms();
    viewer->file_data->delimiter = (delimiter == 0) ? detect_delimiter(viewer->file_data->data, viewer->file_data->length) : delimiter;
    phase_time = get_time_ms() - delim_start;
    printf("Delimiter detection: %.2f ms\n", phase_time);
    
    // Phase 3: Memory allocation
    double alloc_start = get_time_ms();
    viewer->file_data->fields = malloc(MAX_COLS * sizeof(FieldDesc));
    if (!viewer->file_data->fields) {
        fprintf(stderr, "Failed to allocate memory for field descriptors\n");
        goto cleanup;
    }
    phase_time = get_time_ms() - alloc_start;
    printf("Field allocation: %.2f ms\n", phase_time);
    
    // Phase 4: File scanning
    double scan_start = get_time_ms();
    if (scan_file(viewer) != 0) goto cleanup;
    phase_time = get_time_ms() - scan_start;
    printf("File scanning: %.2f ms (found %zu lines)\n", phase_time, viewer->file_data->num_lines);
    
    // Phase 5: Column analysis
    double analysis_start = get_time_ms();
    if (analyze_columns(viewer) != 0) goto cleanup;
    phase_time = get_time_ms() - analysis_start;
    printf("Column analysis: %.2f ms\n", phase_time);

    // Set the values directly.
    viewer->display_state->show_header = 1; // Default is 1 (true)
    
    // The locale logic is now coupled with the DisplayState.
    char *locale = setlocale(LC_CTYPE, NULL);
    if (locale && (strstr(locale, "UTF-8") || strstr(locale, "utf8"))) {
        viewer->display_state->supports_unicode = 1;
        viewer->display_state->separator = UNICODE_SEPARATOR;
    } else {
        viewer->display_state->supports_unicode = 0;
        viewer->display_state->separator = ASCII_SEPARATOR;
    }
    
    // Phase 6: Cache initialization
    double cache_start = get_time_ms();
    if (viewer->file_data->num_lines > CACHE_THRESHOLD_LINES || viewer->display_state->num_cols > CACHE_THRESHOLD_COLS) {
        if (init_cache_system((struct DSVViewer*)viewer) != 0) {
            fprintf(stderr, "Warning: Failed to initialize cache. Continuing without it.\n");
            // The cleanup function will safely handle the partially initialized cache components.
        }
    }
    phase_time = get_time_ms() - cache_start;
    printf("Cache initialization: %.2f ms\n", phase_time);
    
    double total_time = get_time_ms() - start_time;
    printf("Total initialization: %.2f ms\n", total_time);
    
    ret = 0; // Success

cleanup:
    if (ret != 0) {
        // If we failed, clean up everything.
        // On success, cleanup happens later in main().
        cleanup_viewer(viewer);
    }
    return ret;
}

static void cleanup_file_and_parse_data_resources(DSVViewer *viewer) {
    if (!viewer->file_data) return;
    
    if (viewer->file_data->fields) free(viewer->file_data->fields);
    if (viewer->file_data->line_offsets) free(viewer->file_data->line_offsets);
    
    // The main struct pointer is freed in the main cleanup function
}

void cleanup_viewer(DSVViewer *viewer) {
    if (!viewer) return;

    unload_file(viewer);
    cleanup_cache_system((struct DSVViewer*)viewer);
    cleanup_file_and_parse_data_resources(viewer);

    if (viewer->display_state) {
        if (viewer->display_state->col_widths) free(viewer->display_state->col_widths);
        free(viewer->display_state);
    }

    if (viewer->file_data) {
        free(viewer->file_data);
    }
}

// --- Core Parsing Logic ---
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    if (offset >= viewer->file_data->length) return 0;
    size_t field_count = 0;
    int in_quotes = 0;
    size_t i = offset;
    size_t field_start = offset;
    int has_escapes = 0;
    while (i < viewer->file_data->length && field_count < (size_t)max_fields) {
        char c = viewer->file_data->data[i];
        if (c == '"') {
            if (in_quotes && i + 1 < viewer->file_data->length && viewer->file_data->data[i + 1] == '"') {
                has_escapes = 1;
                i += 2; 
                continue;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == viewer->file_data->delimiter && !in_quotes) {
            fields[field_count] = (FieldDesc){viewer->file_data->data + field_start, i - field_start, has_escapes};
            field_count++;
            field_start = i + 1;
            has_escapes = 0;
        } else if (c == '\n' && !in_quotes) {
            break;
        }
        i++;
    }
    if (field_count < (size_t)max_fields) {
        fields[field_count] = (FieldDesc){viewer->file_data->data + field_start, i - field_start, has_escapes};
        field_count++;
    }
    viewer->file_data->num_fields = field_count;
    return field_count;
}

char* render_field(const FieldDesc *field, char *buffer, size_t buffer_size) {
    if (!field->start || field->length == 0) { buffer[0] = '\0'; return buffer; }
    const char *src = field->start;
    size_t src_len = field->length;
    size_t dst_pos = 0;
    if (src_len >= 2 && src[0] == '"' && src[src_len-1] == '"') { src++; src_len -= 2; }
    if (field->needs_unescaping) {
        for (size_t i = 0; i < src_len && dst_pos < buffer_size - 1; i++) {
            if (src[i] == '"' && i + 1 < src_len && src[i + 1] == '"') {
                buffer[dst_pos++] = '"';
                i++;
            } else if (src[i] == '\n') {
                buffer[dst_pos++] = ' ';
            } else {
                buffer[dst_pos++] = src[i];
            }
        }
    } else {
        size_t copy_len = src_len < buffer_size - 1 ? src_len : buffer_size - 1;
        memcpy(buffer, src, copy_len);
        dst_pos = copy_len;
    }
    buffer[dst_pos] = '\0';
    return buffer;
}

int calculate_field_display_width(DSVViewer *viewer, const FieldDesc *field) {
    if (!field->start || field->length == 0) return 0;
    size_t raw_len = field->length;
    if (raw_len >= 2 && field->start[0] == '"' && field->start[raw_len-1] == '"') raw_len -= 2;
    if (field->needs_unescaping) {
        char *temp_buffer = viewer->display_state->buffers.buffer_one;
        render_field(field, temp_buffer, MAX_FIELD_LEN);
        wchar_t *wcs = viewer->display_state->buffers.wide_buffer;
        mbstowcs(wcs, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        return display_width < 0 ? strlen(temp_buffer) : display_width;
    } else {
        char *temp = viewer->display_state->buffers.buffer_one;
        size_t copy_len = raw_len < MAX_FIELD_LEN - 1 ? raw_len : MAX_FIELD_LEN - 1;
        memcpy(temp, field->start + (field->length > raw_len ? 1 : 0), copy_len);
        temp[copy_len] = '\0';
        wchar_t *wcs = viewer->display_state->buffers.wide_buffer;
        mbstowcs(wcs, temp, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        return display_width < 0 ? copy_len : display_width;
    }
} 