#include "viewer.h"
#include "viewer_core.h"
#include "file_io.h"
#include "analysis.h"
#include "utils.h"
#include "logging.h"
#include "error_context.h"
#include "buffer_pool.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CACHE_THRESHOLD_LINES 500
#define CACHE_THRESHOLD_COLS 20

// --- Component Lifecycle Management ---

DSVResult init_viewer_components(struct DSVViewer *viewer) {
    viewer->display_state = malloc(sizeof(DisplayState));
    if (!viewer->display_state) {
        LOG_ERROR("Failed to allocate for DisplayState");
        return DSV_ERROR_MEMORY;
    }
    memset(viewer->display_state, 0, sizeof(DisplayState));
    
    // Initialize buffer pool
    init_buffer_pool(&viewer->display_state->buffers);

    viewer->file_data = malloc(sizeof(FileAndParseData));
    if (!viewer->file_data) {
        LOG_ERROR("Failed to allocate for FileAndParseData");
        free(viewer->display_state);
        return DSV_ERROR_MEMORY;
    }
    memset(viewer->file_data, 0, sizeof(FileAndParseData));
    return DSV_OK;
}

void cleanup_viewer_resources(struct DSVViewer *viewer) {
    if (!viewer || !viewer->file_data) return;

    if (viewer->file_data->fields) {
        free(viewer->file_data->fields);
        viewer->file_data->fields = NULL;
    }
    if (viewer->file_data->line_offsets) {
        free(viewer->file_data->line_offsets);
        viewer->file_data->line_offsets = NULL;
    }
}

// Full cleanup function, moved from parser.c
void cleanup_viewer(DSVViewer *viewer) {
    if (!viewer) return;

    cleanup_file_data(viewer); // from file_io.h
    cleanup_cache_system((struct DSVViewer*)viewer); // from cache.h
    cleanup_viewer_resources(viewer);

    if (viewer->display_state) {
        if (viewer->display_state->col_widths) {
            free(viewer->display_state->col_widths);
            viewer->display_state->col_widths = NULL;
        }
        free(viewer->display_state);
        viewer->display_state = NULL;
    }

    if (viewer->file_data) {
        free(viewer->file_data);
        viewer->file_data = NULL;
    }
}

// The plan specifies this function but cleanup_viewer covers all of it.
// This can be used later to break up the main cleanup function.
void cleanup_viewer_components(struct DSVViewer *viewer) {
    cleanup_viewer(viewer);
}

// --- Configuration Management ---

void configure_viewer_settings(struct DSVViewer *viewer) {
    viewer->display_state->show_header = 1;

    char *locale = setlocale(LC_CTYPE, NULL);
    if (locale && (strstr(locale, "UTF-8") || strstr(locale, "utf8"))) {
        viewer->display_state->supports_unicode = 1;
        viewer->display_state->separator = UNICODE_SEPARATOR;
    } else {
        viewer->display_state->supports_unicode = 0;
        viewer->display_state->separator = ASCII_SEPARATOR;
    }
}

void initialize_viewer_cache(struct DSVViewer *viewer) {
    if (viewer->file_data->num_lines > CACHE_THRESHOLD_LINES || viewer->display_state->num_cols > CACHE_THRESHOLD_COLS) {
        if (init_cache_system((struct DSVViewer*)viewer) != DSV_OK) {
            LOG_WARN("Failed to initialize cache. Continuing without it.");
        }
    }
}

// --- Main Initialization Function ---

DSVResult init_viewer(DSVViewer *viewer, const char *filename, char delimiter) {
    DSVResult res;
    double phase_time, total_time;
    total_time = get_time_ms();

    // Core components
    phase_time = get_time_ms();
    res = init_viewer_components(viewer);
    if (res != DSV_OK) return res;
    printf("Core components: %.2f ms\n", get_time_ms() - phase_time);

    // File operations
    phase_time = get_time_ms();
    if (load_file_data(viewer, filename) != 0) {
        LOG_ERROR("Failed to load file data.");
        return DSV_ERROR_FILE_IO;
    }
    viewer->file_data->delimiter = detect_file_delimiter(viewer->file_data->data, viewer->file_data->length, delimiter);
    printf("File operations: %.2f ms\n", get_time_ms() - phase_time);

    // Data structures
    phase_time = get_time_ms();
    viewer->file_data->fields = malloc(MAX_COLS * sizeof(FieldDesc));
    if (!viewer->file_data->fields) {
        LOG_ERROR("Failed to allocate for fields");
        return DSV_ERROR_MEMORY;
    }
    if (scan_file_data(viewer) != 0) {
        LOG_ERROR("Failed to scan file data.");
        return DSV_ERROR_PARSE;
    }
    printf("Data structures: %.2f ms\n", get_time_ms() - phase_time);

    // Display features
    phase_time = get_time_ms();
    if (analyze_columns_legacy(viewer) != 0) {
        LOG_ERROR("Failed to analyze columns.");
        return DSV_ERROR_DISPLAY;
    }
    configure_viewer_settings(viewer);
    initialize_viewer_cache(viewer);
    printf("Display features: %.2f ms\n", get_time_ms() - phase_time);

    printf("Total initialization: %.2f ms\n", get_time_ms() - total_time);
    LOG_INFO("Viewer initialized successfully.");
    return DSV_OK;
} 