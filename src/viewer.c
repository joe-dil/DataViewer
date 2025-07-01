#include "viewer.h"
#include "config.h"
#include "file_io.h"
#include "analysis.h"
#include "utils.h"
#include "logging.h"
#include "error_context.h"
#include "buffer_pool.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

// --- Internal Component Functions (static) ---

static DSVResult init_viewer_components(struct DSVViewer *viewer, const DSVConfig *config) {
    viewer->config = config;

    viewer->display_state = calloc(1, sizeof(DisplayState));
    if (!viewer->display_state) {
        LOG_ERROR("Failed to allocate for DisplayState");
        return DSV_ERROR_MEMORY;
    }
    
    // Initialize buffer pool
    if (init_buffer_pool(&viewer->display_state->buffers, config) != DSV_OK) {
        LOG_ERROR("Failed to initialize buffer pool");
        free(viewer->display_state);
        viewer->display_state = NULL;
        return DSV_ERROR_MEMORY;
    }

    viewer->file_data = calloc(1, sizeof(FileAndParseData));
    if (!viewer->file_data) {
        LOG_ERROR("Failed to allocate for FileAndParseData");
        cleanup_buffer_pool(&viewer->display_state->buffers);
        free(viewer->display_state);
        viewer->display_state = NULL;
        return DSV_ERROR_MEMORY;
    }
    return DSV_OK;
}

static void cleanup_viewer_resources(struct DSVViewer *viewer) {
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
        cleanup_buffer_pool(&viewer->display_state->buffers);
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



// --- Configuration Management ---

static void configure_viewer_settings(struct DSVViewer *viewer, const DSVConfig *config) {
    (void)config; // Suppress unused parameter warning
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

static void initialize_viewer_cache(struct DSVViewer *viewer, const DSVConfig *config) {
    if (viewer->file_data->num_lines > (size_t)config->cache_threshold_lines || viewer->display_state->num_cols > (size_t)config->cache_threshold_cols) {
        if (init_cache_system((struct DSVViewer*)viewer, config) != DSV_OK) {
            LOG_WARN("Failed to initialize cache. Continuing without it.");
        }
    }
}

// --- Main Initialization Function ---

DSVResult init_viewer(DSVViewer *viewer, const char *filename, char delimiter, const DSVConfig *config) {
    DSVResult res;
    double phase_time, total_time;
    total_time = get_time_ms();

    // Core components
    phase_time = get_time_ms();
    res = init_viewer_components(viewer, config);
    if (res != DSV_OK) return res;
    printf("Core components: %.2f ms\n", get_time_ms() - phase_time);

    // File operations
    phase_time = get_time_ms();
    if (load_file_data(viewer, filename) != DSV_OK) {
        LOG_ERROR("Failed to load file data.");
        return DSV_ERROR_FILE_IO;
    }
    viewer->file_data->delimiter = detect_file_delimiter(viewer->file_data->data, viewer->file_data->length, delimiter, config);
    printf("File operations: %.2f ms\n", get_time_ms() - phase_time);

    // Data structures
    phase_time = get_time_ms();
    viewer->file_data->fields = malloc(config->max_cols * sizeof(FieldDesc));
    if (!viewer->file_data->fields) {
        LOG_ERROR("Failed to allocate for fields");
        return DSV_ERROR_MEMORY;
    }
    if (scan_file_data(viewer, config) != DSV_OK) {
        LOG_ERROR("Failed to scan file data.");
        return DSV_ERROR_PARSE;
    }
    printf("Data structures: %.2f ms\n", get_time_ms() - phase_time);

    // Display features
    phase_time = get_time_ms();
    if (analyze_column_widths(viewer, config) != 0) {
        LOG_ERROR("Failed to analyze columns.");
        return DSV_ERROR_DISPLAY;
    }
    configure_viewer_settings(viewer, config);
    initialize_viewer_cache(viewer, config);
    printf("Display features: %.2f ms\n", get_time_ms() - phase_time);

    printf("Total initialization: %.2f ms\n", get_time_ms() - total_time);
    LOG_INFO("Viewer initialized successfully.");
    return DSV_OK;
} 