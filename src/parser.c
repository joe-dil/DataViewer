#include "viewer.h"
#include <wchar.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/time.h>

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
static void scan_file(DSVViewer *viewer);
static void analyze_columns(struct DSVViewer *viewer);
static void* analyze_columns_worker(void *arg);

// --- Delimiter Detection ---
static char detect_delimiter(const char *data, size_t length) {
    int comma_count = 0, tab_count = 0, pipe_count = 0;
    size_t scan_len = (length < 1024) ? length : 1024;
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
    const size_t sample_size = (viewer->length < 65536) ? viewer->length : 65536;
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
    
    if (sample_lines == 0) return (viewer->length / 80) + 1;
    double avg_line_len = (double)sample_size / sample_lines;
    return (size_t)((viewer->length / avg_line_len) * 1.2) + 1;
}

static void scan_file(DSVViewer *viewer) {
    viewer->capacity = estimate_line_count(viewer);
    viewer->line_offsets = malloc(viewer->capacity * sizeof(size_t));
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
            viewer->line_offsets = realloc(viewer->line_offsets, viewer->capacity * sizeof(size_t));
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
        viewer->line_offsets = realloc(viewer->line_offsets, viewer->num_lines * sizeof(size_t));
    }
}

// --- Column Analysis ---
typedef struct {
    DSVViewer *viewer;
    size_t start_line, end_line;
    int *thread_widths;
    size_t thread_max_cols;
} ThreadData;

static void* analyze_columns_worker(void *arg) {
    ThreadData *data = (ThreadData*)arg;
    FieldDesc thread_fields[MAX_COLS];
    for (size_t i = data->start_line; i < data->end_line; i++) {
        size_t num_fields = parse_line(data->viewer, data->viewer->line_offsets[i], thread_fields, MAX_COLS);
        if (num_fields > data->thread_max_cols) data->thread_max_cols = num_fields;
        for (size_t col = 0; col < num_fields; col++) {
            if (data->thread_widths[col] >= 16) continue;
            int width = calculate_field_display_width(data->viewer, &thread_fields[col]);
            if (width > data->thread_widths[col]) data->thread_widths[col] = width;
        }
    }
    return NULL;
}

static void analyze_columns(struct DSVViewer *viewer) {
    long num_cores = sysconf(_SC_NPROCESSORS_CONF); // Get physical cores
    int max_threads = 4; // Sensible cap on threads
    viewer->num_threads = (num_cores > 1) ? (int)num_cores : 1;
    if (viewer->num_threads > max_threads) viewer->num_threads = max_threads;
    
    if (viewer->num_lines < 2000) viewer->num_threads = 1; // No benefit for small files

    pthread_t threads[viewer->num_threads];
    ThreadData thread_data[viewer->num_threads];
    size_t lines_per_thread = viewer->num_lines / viewer->num_threads;

    for (int i = 0; i < viewer->num_threads; i++) {
        thread_data[i] = (ThreadData){
            .viewer = viewer, 
            .start_line = i * lines_per_thread, 
            .end_line = (i == viewer->num_threads - 1) ? viewer->num_lines : (i + 1) * lines_per_thread,
            .thread_widths = calloc(MAX_COLS, sizeof(int)),
            .thread_max_cols = 0
        };

        if (!thread_data[i].thread_widths) {
            fprintf(stderr, "Failed to allocate memory for thread %d\n", i);
            // In a real app, we'd handle this more gracefully (e.g., fallback to single-threaded)
            // For now, we'll just exit to avoid a crash.
            exit(1); 
        }

        if (pthread_create(&threads[i], NULL, analyze_columns_worker, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            exit(1); // Abort if a thread can't be created
        }
    }

    size_t max_cols = 0;
    int *final_widths = calloc(MAX_COLS, sizeof(int));
    for (int i = 0; i < viewer->num_threads; i++) {
        pthread_join(threads[i], NULL);
        if (thread_data[i].thread_max_cols > max_cols) max_cols = thread_data[i].thread_max_cols;
        for (size_t j = 0; j < thread_data[i].thread_max_cols; j++) {
            if (thread_data[i].thread_widths[j] > final_widths[j]) final_widths[j] = thread_data[i].thread_widths[j];
        }
        free(thread_data[i].thread_widths);
    }
    viewer->num_cols = max_cols > MAX_COLS ? MAX_COLS : max_cols;
    viewer->col_widths = malloc(viewer->num_cols * sizeof(int));
    for (size_t i = 0; i < viewer->num_cols; i++) {
        viewer->col_widths[i] = final_widths[i] > 16 ? 16 : (final_widths[i] < 4 ? 4 : final_widths[i]);
    }
    free(final_widths);
}

// --- Main Initialization Orchestrator ---
int init_viewer(DSVViewer *viewer, const char *filename, char delimiter) {
    double start_time = get_time_ms();
    double phase_time;
    
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
    phase_time = get_time_ms() - alloc_start;
    printf("Field allocation: %.2f ms\n", phase_time);
    
    // Phase 4: File scanning
    double scan_start = get_time_ms();
    scan_file(viewer);
    phase_time = get_time_ms() - scan_start;
    printf("File scanning: %.2f ms (found %zu lines)\n", phase_time, viewer->num_lines);
    
    // Phase 5: Column analysis
    double analysis_start = get_time_ms();
    analyze_columns(viewer);
    phase_time = get_time_ms() - analysis_start;
    printf("Column analysis: %.2f ms (using %d threads)\n", phase_time, viewer->num_threads);
    
    // Phase 6: Cache initialization
    double cache_start = get_time_ms();
    if (viewer->num_lines > 500 || viewer->num_cols > 20) init_cache_system((struct DSVViewer*)viewer);
    else { viewer->mem_pool = NULL; viewer->display_cache = NULL; viewer->intern_table = NULL; }
    phase_time = get_time_ms() - cache_start;
    printf("Cache initialization: %.2f ms\n", phase_time);
    
    double total_time = get_time_ms() - start_time;
    printf("Total initialization: %.2f ms\n", total_time);
    
    return 0;
}

static void cleanup_parser_resources(DSVViewer *viewer) {
    if (viewer->fields) free(viewer->fields);
    if (viewer->line_offsets) free(viewer->line_offsets);
    if (viewer->col_widths) free(viewer->col_widths);
}

void cleanup_viewer(DSVViewer *viewer) {
    unload_file(viewer);
    cleanup_parser_resources(viewer);
    cleanup_cache_system((struct DSVViewer*)viewer);
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
        char *temp_buffer = viewer->buffer_pool.buffer_one;
        render_field(field, temp_buffer, MAX_FIELD_LEN);
        wchar_t *wcs = viewer->buffer_pool.wide_buffer;
        mbstowcs(wcs, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        return display_width < 0 ? strlen(temp_buffer) : display_width;
    } else {
        char *temp = viewer->buffer_pool.buffer_one;
        size_t copy_len = raw_len < MAX_FIELD_LEN - 1 ? raw_len : MAX_FIELD_LEN - 1;
        memcpy(temp, field->start + (field->length > raw_len ? 1 : 0), copy_len);
        temp[copy_len] = '\0';
        wchar_t *wcs = viewer->buffer_pool.wide_buffer;
        mbstowcs(wcs, temp, MAX_FIELD_LEN);
        int display_width = wcswidth(wcs, MAX_FIELD_LEN);
        return display_width < 0 ? copy_len : display_width;
    }
} 