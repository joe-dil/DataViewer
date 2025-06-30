#include "viewer.h"
#include "display_state.h"
#include "file_io.h"
#include "analysis.h"
#include <wchar.h>
#include <pthread.h>
#include <sys/time.h>

// Constants to replace magic numbers
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


// --- Component Initialization ---
static int init_components(DSVViewer *viewer) {
    // -- Initialize new DisplayState component --
    viewer->display_state = malloc(sizeof(DisplayState));
    if (!viewer->display_state) {
        perror("Failed to allocate for DisplayState");
        return -1;
    }
    memset(viewer->display_state, 0, sizeof(DisplayState));

    // -- Initialize new FileAndParseData component --
    viewer->file_data = malloc(sizeof(FileAndParseData));
    if (!viewer->file_data) {
        perror("Failed to allocate for FileAndParseData");
        return -1; // display_state will be cleaned up by the caller
    }
    memset(viewer->file_data, 0, sizeof(FileAndParseData));
    return 0;
}

static void configure_display_settings(DSVViewer *viewer) {
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
}

static void initialize_cache_if_needed(DSVViewer *viewer) {
    if (viewer->file_data->num_lines > CACHE_THRESHOLD_LINES || viewer->display_state->num_cols > CACHE_THRESHOLD_COLS) {
        if (init_cache_system((struct DSVViewer*)viewer) != 0) {
            fprintf(stderr, "Warning: Failed to initialize cache. Continuing without it.\n");
            // The cleanup function will safely handle the partially initialized cache components.
        }
    }
}

// --- Modular Initialization Functions ---

static int init_core_components(DSVViewer *viewer) {
    double start_time = get_time_ms();
    
    // Ensure all pointers are NULL so cleanup is safe
    memset(viewer, 0, sizeof(DSVViewer));
    
    if (init_components(viewer) != 0) {
        return -1;
    }
    
    double phase_time = get_time_ms() - start_time;
    printf("Core components: %.2f ms\n", phase_time);
    return 0;
}

static int init_file_operations(DSVViewer *viewer, const char *filename, char delimiter) {
    double start_time = get_time_ms();
    double phase_time;
    
    // Phase 1: File loading
    double load_start = get_time_ms();
    if (load_file_data(viewer, filename) != 0) return -1;
    phase_time = get_time_ms() - load_start;
    printf("File loading: %.2f ms\n", phase_time);
    
    // Phase 2: Delimiter detection
    double delim_start = get_time_ms();
    viewer->file_data->delimiter = detect_file_delimiter(viewer->file_data->data, viewer->file_data->length, delimiter);
    phase_time = get_time_ms() - delim_start;
    printf("Delimiter detection: %.2f ms\n", phase_time);
    
    double total_time = get_time_ms() - start_time;
    printf("File operations: %.2f ms\n", total_time);
    return 0;
}

static int init_data_structures(DSVViewer *viewer) {
    double start_time = get_time_ms();
    double phase_time;
    
    // Phase 3: Memory allocation
    double alloc_start = get_time_ms();
    viewer->file_data->fields = malloc(MAX_COLS * sizeof(FieldDesc));
    if (!viewer->file_data->fields) {
        fprintf(stderr, "Failed to allocate memory for field descriptors\n");
        return -1;
    }
    phase_time = get_time_ms() - alloc_start;
    printf("Field allocation: %.2f ms\n", phase_time);
    
    // Phase 4: File scanning
    double scan_start = get_time_ms();
    if (scan_file_data(viewer) != 0) return -1;
    phase_time = get_time_ms() - scan_start;
    printf("File scanning: %.2f ms (found %zu lines)\n", phase_time, viewer->file_data->num_lines);
    
    double total_time = get_time_ms() - start_time;
    printf("Data structures: %.2f ms\n", total_time);
    return 0;
}

static int init_display_features(DSVViewer *viewer) {
    double start_time = get_time_ms();
    double phase_time;
    
    // Phase 5: Column analysis
    double analysis_start = get_time_ms();
    if (analyze_columns_legacy(viewer) != 0) return -1;
    phase_time = get_time_ms() - analysis_start;
    printf("Column analysis: %.2f ms\n", phase_time);

    configure_display_settings(viewer);
    
    // Phase 6: Cache initialization
    double cache_start = get_time_ms();
    initialize_cache_if_needed(viewer);
    phase_time = get_time_ms() - cache_start;
    printf("Cache initialization: %.2f ms\n", phase_time);
    
    double total_time = get_time_ms() - start_time;
    printf("Display features: %.2f ms\n", total_time);
    return 0;
}

// --- Main Initialization Functions ---

// Fast, modular initialization - improved testability and maintainability
int init_viewer_fast(DSVViewer *viewer, const char *filename, char delimiter) {
    double start_time = get_time_ms();
    
    if (init_core_components(viewer) != 0) {
        goto cleanup;
    }
    
    if (init_file_operations(viewer, filename, delimiter) != 0) {
        goto cleanup;
    }
    
    if (init_data_structures(viewer) != 0) {
        goto cleanup;
    }
    
    if (init_display_features(viewer) != 0) {
        goto cleanup;
    }
    
    double total_time = get_time_ms() - start_time;
    printf("Total initialization: %.2f ms\n", total_time);
    
    return 0;

cleanup:
    cleanup_viewer(viewer);
    return -1;
}

// Legacy wrapper - maintains compatibility with existing code
int init_viewer(DSVViewer *viewer, const char *filename, char delimiter) {
    return init_viewer_fast(viewer, filename, delimiter);
}

static void cleanup_file_and_parse_data_resources(DSVViewer *viewer) {
    if (!viewer->file_data) return;
    
    if (viewer->file_data->fields) free(viewer->file_data->fields);
    if (viewer->file_data->line_offsets) free(viewer->file_data->line_offsets);
    
    // The main struct pointer is freed in the main cleanup function
}

void cleanup_viewer(DSVViewer *viewer) {
    if (!viewer) return;

    cleanup_file_data(viewer);
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

// A state machine for the parser to improve readability.
typedef struct {
    int in_quotes;
    int needs_unescaping;
    size_t field_start;
    size_t field_count;
} ParseState;

// Helper to record a new field, avoiding duplicate code.
static void record_field(DSVViewer *viewer, FieldDesc *fields, size_t *field_count, int max_fields, size_t field_start, size_t current_pos, int needs_unescaping) {
    if (*field_count < (size_t)max_fields) {
        fields[*field_count] = (FieldDesc){viewer->file_data->data + field_start, current_pos - field_start, needs_unescaping};
        (*field_count)++;
    }
}

size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    // Cache frequently accessed variables from viewer->file_data to local variables.
    // This reduces pointer dereferencing overhead inside the loop.
    const char *data = viewer->file_data->data;
    const size_t length = viewer->file_data->length;
    const char delimiter = viewer->file_data->delimiter;

    if (offset >= length) return 0;

    // Initialize the state machine for the parser.
    ParseState state = {
        .in_quotes = 0,
        .needs_unescaping = 0,
        .field_start = offset,
        .field_count = 0,
    };

    size_t i = offset;
    while (i < length) {
        char c = data[i];

        // State: Inside a quoted field
        if (state.in_quotes) {
            if (c == '"') {
                // Check for an escaped quote ("")
                if (i + 1 < length && data[i + 1] == '"') {
                    state.needs_unescaping = 1;
                    i++; // Skip the second quote; loop will increment over the first
                } else {
                    state.in_quotes = 0; // This is a closing quote
                }
            }
        // State: Not in a quoted field
        } else {
            if (c == '"') {
                state.in_quotes = 1; // Start of a quoted field
            } else if (c == delimiter) {
                // End of a field, record it
                record_field(viewer, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);
                state.field_start = i + 1;
                state.needs_unescaping = 0; // Reset for next field
            } else if (c == '\n') {
                // End of the line, exit the loop
                break;
            }
        }
        i++;
    }
    
    // After the loop, there's always one last field to record.
    // This handles lines that don't end with a delimiter or newline.
    record_field(viewer, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);

    viewer->file_data->num_fields = state.field_count;
    return state.field_count;
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