#include "viewer.h"
#include "file_io.h"
#include "display_state.h"
#include "file_and_parse_data.h"
#include "logging.h"
#include "error_context.h"
#include "utils.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LINE_CAPACITY_GROWTH_FACTOR 1.2

static size_t estimate_line_count(DSVViewer *viewer, const DSVConfig *config);

// --- Validation Functions (Critical Fixes) ---

static DSVResult validate_file_bounds(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->file_data, DSV_ERROR_INVALID_ARGS);
    
    // CRITICAL: Prevent SIZE_MAX assignment when num_lines == 0
    if (viewer->file_data->num_lines == 0) {
        // Create minimal valid state instead of underflow
        viewer->file_data->num_lines = 1;
        viewer->file_data->line_offsets = malloc(sizeof(size_t));
        CHECK_ALLOC(viewer->file_data->line_offsets);
        viewer->file_data->line_offsets[0] = 0;
        return DSV_OK; // Signal empty file handled
    }
    return DSV_OK; // Normal file
}

static DSVResult handle_empty_file(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->file_data, DSV_ERROR_INVALID_ARGS);
    
    if (viewer->file_data->length == 0) {
        // Set up minimal valid state for empty files
        viewer->file_data->num_lines = 1;
        viewer->file_data->line_offsets = malloc(sizeof(size_t));
        CHECK_ALLOC(viewer->file_data->line_offsets);
        viewer->file_data->line_offsets[0] = 0;
        viewer->file_data->delimiter = ','; // Default delimiter
        return DSV_OK;
    }
    return DSV_ERROR; // Not empty - caller should continue processing
}

// Handle single line files - basic implementation
static DSVResult handle_single_line_file(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->file_data, DSV_ERROR_INVALID_ARGS);
    
    // Single-line files are handled by the normal scanning process
    if (viewer->file_data->num_lines == 1) {
        // Single line files work correctly with existing logic
    }
    return DSV_OK;
}


// --- Static Helper Functions ---

static size_t estimate_line_count(DSVViewer *viewer, const DSVConfig *config) {
    if (!viewer || !viewer->file_data || !config) return 1;
    
    const size_t sample_size = (viewer->file_data->length < (size_t)config->line_estimation_sample_size) ? viewer->file_data->length : (size_t)config->line_estimation_sample_size;
    if (sample_size == 0) return 1;

    int sample_lines = 0;
    const char *search_start = viewer->file_data->data;
    const char *search_end = viewer->file_data->data + sample_size;

    while (search_start < search_end) {
        const char *newline = memchr(search_start, '\n', search_end - search_start);
        if (!newline) break;
        sample_lines++;
        search_start = newline + 1;
    }

    if (sample_lines == 0) return (viewer->file_data->length / config->default_chars_per_line) + 1;
    double avg_line_len = (double)sample_size / sample_lines;
    return (size_t)((viewer->file_data->length / avg_line_len) * LINE_CAPACITY_GROWTH_FACTOR) + 1;
}

// --- Public API Functions ---

char detect_file_delimiter(const char *data, size_t length, char override_delimiter, const DSVConfig *config) {
    if (override_delimiter != 0) {
        return override_delimiter;
    }
    if (!data || !config) return ','; // Safe fallback
    
    int comma_count = 0, tab_count = 0, pipe_count = 0;
    size_t scan_len = (length < (size_t)config->delimiter_detection_sample_size) ? length : (size_t)config->delimiter_detection_sample_size;
    for (size_t i = 0; i < scan_len; i++) {
        if (data[i] == ',') comma_count++;
        else if (data[i] == '\t') tab_count++;
        else if (data[i] == '|') pipe_count++;
    }
    if (tab_count > comma_count && tab_count > pipe_count) return '\t';
    if (pipe_count > comma_count && pipe_count > tab_count) return '|';
    return ',';
}

DSVResult load_file_data(struct DSVViewer *viewer, const char *filename) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->file_data, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(filename, DSV_ERROR_INVALID_ARGS);
    
    struct stat st;
    viewer->file_data->fd = open(filename, O_RDONLY);
    if (viewer->file_data->fd == -1) {
        LOG_ERROR("Failed to open file '%s': %s", filename, strerror(errno));
        return DSV_ERROR_FILE_IO;
    }
    if (fstat(viewer->file_data->fd, &st) == -1) {
        LOG_ERROR("Failed to stat file '%s': %s", filename, strerror(errno));
        close(viewer->file_data->fd);
        return DSV_ERROR_FILE_IO;
    }
    viewer->file_data->length = st.st_size;
    if (viewer->file_data->length > 0) {
        viewer->file_data->data = mmap(NULL, viewer->file_data->length, PROT_READ, MAP_PRIVATE, viewer->file_data->fd, 0);
        if (viewer->file_data->data == MAP_FAILED) {
            LOG_ERROR("Failed to mmap file '%s': %s", filename, strerror(errno));
            close(viewer->file_data->fd);
            return DSV_ERROR_FILE_IO;
        }
    } else {
        viewer->file_data->data = NULL;
    }
    return DSV_OK;
}

void cleanup_file_data(struct DSVViewer *viewer) {
    if (!viewer || !viewer->file_data) return;
    
    if (viewer->file_data->data != NULL && viewer->file_data->data != MAP_FAILED) {
        munmap(viewer->file_data->data, viewer->file_data->length);
    }
    if (viewer->file_data->fd != -1) {
        close(viewer->file_data->fd);
    }
}

DSVResult scan_file_data(struct DSVViewer *viewer, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(config, DSV_ERROR_INVALID_ARGS);
    
    DSVResult empty_result = handle_empty_file(viewer);
    if (empty_result == DSV_OK) return DSV_OK;
    if (empty_result != DSV_ERROR) return empty_result; // DSV_ERROR means "continue processing"

    viewer->file_data->capacity = estimate_line_count(viewer, config);
    viewer->file_data->line_offsets = malloc(viewer->file_data->capacity * sizeof(size_t));
    CHECK_ALLOC(viewer->file_data->line_offsets);

    viewer->file_data->num_lines = 0;
    viewer->file_data->line_offsets[viewer->file_data->num_lines++] = 0;

    const char *search_start = viewer->file_data->data;
    const char *file_end = viewer->file_data->data + viewer->file_data->length;

    while (search_start < file_end) {
        const char *newline = memchr(search_start, '\n', file_end - search_start);
        if (!newline) break;

        if (viewer->file_data->num_lines >= viewer->file_data->capacity) {
            size_t new_capacity = viewer->file_data->capacity * 2;
            size_t *new_offsets = realloc(viewer->file_data->line_offsets, new_capacity * sizeof(size_t));
            if (!new_offsets) {
                LOG_ERROR("Failed to expand line offsets array");
                return DSV_ERROR_MEMORY;
            }
            viewer->file_data->line_offsets = new_offsets;
            viewer->file_data->capacity = new_capacity;
        }

        size_t next_line_offset = (newline - viewer->file_data->data) + 1;
        if (next_line_offset < viewer->file_data->length) {
            viewer->file_data->line_offsets[viewer->file_data->num_lines++] = next_line_offset;
        }
        search_start = newline + 1;
    }
    
    // Final memory and bounds checks
    DSVResult validation_result = validate_file_bounds(viewer);
    if (validation_result != DSV_OK) return validation_result;
    
    return handle_single_line_file(viewer);
} 