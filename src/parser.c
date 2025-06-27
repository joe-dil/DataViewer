#include "viewer.h"
#include <wchar.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/time.h>
#include <locale.h>

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
    viewer->fd = open(filename, O_RDONLY);
    if (viewer->fd == -1) { perror("Failed to open file"); return -1; }
    if (fstat(viewer->fd, &st) == -1) { perror("Failed to get file stats"); close(viewer->fd); return -1; }
    viewer->length = st.st_size;
    viewer->data = mmap(NULL, viewer->length, PROT_READ, MAP_PRIVATE, viewer->fd, 0);
    if (viewer->data == MAP_FAILED) { perror("Failed to map file"); close(viewer->fd); return -1; }
    return 0;
}

static void unload_file(struct DSVViewer *viewer) {
    if (viewer->data != MAP_FAILED) munmap(viewer->data, viewer->length);
    if (viewer->fd != -1) close(viewer->fd);
}

// --- File Scanning ---
static size_t estimate_line_count(DSVViewer *viewer) {
    const size_t sample_size = (viewer->length < LINE_ESTIMATION_SAMPLE_SIZE) ? viewer->length : LINE_ESTIMATION_SAMPLE_SIZE;
    if (sample_size == 0) return 1;
    
    // Count newlines in sample using memchr (much faster than byte-by-byte)
    int sample_lines = 0;
    const char *search_start = viewer->data;
    const char *search_end = viewer->data + sample_size;
    
    while (search_start < search_end) {
        const char *newline = memchr(search_start, '\n', search_end - search_start);
        if (!newline) break;
        sample_lines++;
        search_start = newline + 1;
    }
    
    if (sample_lines == 0) return (viewer->length / DEFAULT_CHARS_PER_LINE) + 1;
    double avg_line_len = (double)sample_size / sample_lines;
    return (size_t)((viewer->length / avg_line_len) * LINE_CAPACITY_GROWTH_FACTOR) + 1;
}

static int scan_file(DSVViewer *viewer) {
    viewer->capacity = estimate_line_count(viewer);
    viewer->line_offsets = malloc(viewer->capacity * sizeof(size_t));
    if (!viewer->line_offsets) {
        fprintf(stderr, "Failed to allocate memory for line offsets\n");
        return -1;
    }
    
    viewer->num_lines = 0;
    viewer->line_offsets[viewer->num_lines++] = 0;
    
    // Fast newline scanning using memchr for large chunks
    const char *search_start = viewer->data;
    const char *file_end = viewer->data + viewer->length;
    
    while (search_start < file_end) {
        const char *newline = memchr(search_start, '\n', file_end - search_start);
        if (!newline) break;
        
        // Check if we need to expand capacity
        if (viewer->num_lines >= viewer->capacity) {
            viewer->capacity *= 2;
            size_t *new_offsets = realloc(viewer->line_offsets, viewer->capacity * sizeof(size_t));
            if (!new_offsets) {
                fprintf(stderr, "Failed to expand line offsets array\n");
                return -1;
            }
            viewer->line_offsets = new_offsets;
        }
        
        // Store offset to character after newline
        size_t next_line_offset = (newline - viewer->data) + 1;
        if (next_line_offset < viewer->length) {
            viewer->line_offsets[viewer->num_lines++] = next_line_offset;
        }
        
        search_start = newline + 1;
    }
    
    // Trim capacity to actual size
    if (viewer->capacity > viewer->num_lines) {
        size_t *trimmed_offsets = realloc(viewer->line_offsets, viewer->num_lines * sizeof(size_t));
        if (trimmed_offsets) {
            viewer->line_offsets = trimmed_offsets;
        }
        // If realloc fails for trimming, we just keep the larger array - not a critical error
    }
    
    return 0;
}

// --- Column Analysis ---
static int analyze_columns(struct DSVViewer *viewer) {
    // Simple and fast: analyze first N lines max, single threaded
    size_t sample_lines = viewer->num_lines > COLUMN_ANALYSIS_SAMPLE_LINES ? COLUMN_ANALYSIS_SAMPLE_LINES : viewer->num_lines;
    
    int col_widths[MAX_COLS] = {0};
    size_t max_cols = 0;
    FieldDesc fields[MAX_COLS];
    
    for (size_t i = 0; i < sample_lines; i++) {
        size_t num_fields = parse_line(viewer, viewer->line_offsets[i], fields, MAX_COLS);
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
    
    // -- Initialize new DisplayState component --
    // Do this first, so it's available to other init functions.
    viewer->display_state = malloc(sizeof(DisplayState));
    if (!viewer->display_state) {
        perror("Failed to allocate for DisplayState");
        return -1;
    }
    
    // Phase 1: File loading
    double load_start = get_time_ms();
    if (load_file(viewer, filename) != 0) return -1;
    phase_time = get_time_ms() - load_start;
    printf("File loading: %.2f ms\n", phase_time);
    
    // Phase 2: Delimiter detection
    double delim_start = get_time_ms();
    viewer->delimiter = (delimiter == 0) ? detect_delimiter(viewer->data, viewer->length) : delimiter;
    phase_time = get_time_ms() - delim_start;
    printf("Delimiter detection: %.2f ms\n", phase_time);
    
    // Phase 3: Memory allocation
    double alloc_start = get_time_ms();
    viewer->fields = malloc(MAX_COLS * sizeof(FieldDesc));
    if (!viewer->fields) {
        fprintf(stderr, "Failed to allocate memory for field descriptors\n");
        return -1;
    }
    phase_time = get_time_ms() - alloc_start;
    printf("Field allocation: %.2f ms\n", phase_time);
    
    // Phase 4: File scanning
    double scan_start = get_time_ms();
    if (scan_file(viewer) != 0) return -1;
    phase_time = get_time_ms() - scan_start;
    printf("File scanning: %.2f ms (found %zu lines)\n", phase_time, viewer->num_lines);
    
    // Phase 5: Column analysis
    double analysis_start = get_time_ms();
    if (analyze_columns(viewer) != 0) return -1;
    phase_time = get_time_ms() - analysis_start;
    printf("Column analysis: %.2f ms\n", phase_time);

    // Set the values in the new struct from the old ones
    viewer->display_state->show_header = 1; // Default is 1 (true)
    
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
    if (viewer->num_lines > CACHE_THRESHOLD_LINES || viewer->display_state->num_cols > CACHE_THRESHOLD_COLS) {
        init_cache_system((struct DSVViewer*)viewer);
    } else { 
        viewer->mem_pool = NULL; 
        viewer->display_cache = NULL; 
        viewer->intern_table = NULL; 
    }
    phase_time = get_time_ms() - cache_start;
    printf("Cache initialization: %.2f ms\n", phase_time);
    
    double total_time = get_time_ms() - start_time;
    printf("Total initialization: %.2f ms\n", total_time);
    
    return 0;
}

static void cleanup_parser_resources(DSVViewer *viewer) {
    if (viewer->fields) free(viewer->fields);
    if (viewer->line_offsets) free(viewer->line_offsets);
    // The col_widths pointer is now owned by DisplayState, so we no longer free it here.
    // if (viewer->col_widths) free(viewer->col_widths);
}

void cleanup_viewer(DSVViewer *viewer) {
    unload_file(viewer);
    cleanup_parser_resources(viewer);
    cleanup_cache_system((struct DSVViewer*)viewer);

    // The DisplayState component should be freed last, after all other
    // systems that might have used its data are cleaned up.
    if (viewer->display_state) {
        // This is the sole owner of the col_widths pointer.
        free(viewer->display_state->col_widths);
        free(viewer->display_state);
    }
}

// --- Core Parsing Logic ---
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    if (offset >= viewer->length) return 0;
    size_t field_count = 0;
    int in_quotes = 0;
    size_t i = offset;
    size_t field_start = offset;
    int has_escapes = 0;
    while (i < viewer->length && field_count < (size_t)max_fields) {
        char c = viewer->data[i];
        if (c == '"') {
            if (in_quotes && i + 1 < viewer->length && viewer->data[i + 1] == '"') {
                has_escapes = 1;
                i += 2; 
                continue;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == viewer->delimiter && !in_quotes) {
            fields[field_count] = (FieldDesc){viewer->data + field_start, i - field_start, has_escapes};
            field_count++;
            field_start = i + 1;
            has_escapes = 0;
        } else if (c == '\n' && !in_quotes) {
            break;
        }
        i++;
    }
    if (field_count < (size_t)max_fields) {
        fields[field_count] = (FieldDesc){viewer->data + field_start, i - field_start, has_escapes};
        field_count++;
    }
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