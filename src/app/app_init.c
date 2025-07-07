#include "app_init.h"
#include "config.h"
#include "file_io.h"
#include "analysis.h"
#include "cache.h"
#include "utils.h"
#include "logging.h"
#include "error_context.h"
#include "buffer_pool.h"
#include "view_manager.h"
#include "core/data_source.h"

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

    viewer->file_data = calloc(1, sizeof(FileData));
    if (!viewer->file_data) {
        LOG_ERROR("Failed to allocate for FileData");
        cleanup_buffer_pool(&viewer->display_state->buffers);
        free(viewer->display_state);
        viewer->display_state = NULL;
        return DSV_ERROR_MEMORY;
    }

    viewer->parsed_data = calloc(1, sizeof(ParsedData));
    if (!viewer->parsed_data) {
        LOG_ERROR("Failed to allocate for ParsedData");
        free(viewer->file_data);
        cleanup_buffer_pool(&viewer->display_state->buffers);
        free(viewer->display_state);
        viewer->display_state = NULL;
        return DSV_ERROR_MEMORY;
    }

    viewer->view_manager = init_view_manager();
    if (!viewer->view_manager) {
        LOG_ERROR("Failed to initialize ViewManager");
        free(viewer->parsed_data);
        free(viewer->file_data);
        cleanup_buffer_pool(&viewer->display_state->buffers);
        free(viewer->display_state);
        return DSV_ERROR_MEMORY;
    }

    return DSV_OK;
}

static void cleanup_viewer_resources(struct DSVViewer *viewer) {
    if (!viewer || !viewer->parsed_data) return;

    SAFE_FREE(viewer->parsed_data->fields);
    SAFE_FREE(viewer->parsed_data->line_offsets);
}

// Full cleanup function, moved from parser.c
void cleanup_viewer(DSVViewer *viewer) {
    if (!viewer) return;

    destroy_data_source(viewer->main_data_source);
    cleanup_view_manager(viewer->view_manager);
    cleanup_file_data(viewer); // from file_io.h
    cleanup_cache_system(viewer); // from cache.h
    cleanup_viewer_resources(viewer);

    if (viewer->display_state) {
        cleanup_buffer_pool(&viewer->display_state->buffers);
        SAFE_FREE(viewer->display_state->col_widths);
        SAFE_FREE(viewer->display_state);
    }

    SAFE_FREE(viewer->file_data);
    SAFE_FREE(viewer->parsed_data);
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
    if (viewer->parsed_data->num_lines > (size_t)config->cache_threshold_lines || viewer->display_state->num_cols > (size_t)config->cache_threshold_cols) {
        if (init_cache_system(viewer, config) != DSV_OK) {
            LOG_WARN("Failed to initialize cache. Continuing without it.");
        }
    }
}

// --- Phase 4.2: Split Initialization Phases ---

// Phase 4.2: Initialize file system components
static DSVResult init_file_system(DSVViewer *viewer, const char *filename) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(filename, DSV_ERROR_INVALID_ARGS);
    
    double phase_time = get_time_ms();
    
    if (load_file_data(viewer, filename) != DSV_OK) {
        LOG_ERROR("Failed to load file data.");
        return DSV_ERROR_FILE_IO;
    }
    
    printf("File operations: %.2f ms\n", get_time_ms() - phase_time);
    return DSV_OK;
}

// Phase 4.2: Initialize analysis and parsing systems
static DSVResult init_analysis_system(DSVViewer *viewer, char delimiter) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->config, DSV_ERROR_INVALID_ARGS);
    
    double phase_time = get_time_ms();

    viewer->parsed_data->delimiter = detect_file_delimiter(viewer->file_data->data, viewer->file_data->length, delimiter, viewer->config);
    
    viewer->parsed_data->fields = malloc(viewer->config->max_cols * sizeof(FieldDesc));
    CHECK_ALLOC(viewer->parsed_data->fields);
    
    if (scan_file_data(viewer, viewer->config) != DSV_OK) {
        LOG_ERROR("Failed to scan file data.");
        return DSV_ERROR_PARSE;
    }
    
    printf("Data structures: %.2f ms\n", get_time_ms() - phase_time);
    return DSV_OK;
}

// Phase 4.2: Initialize display and analysis systems
static DSVResult init_display_system(DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->config, DSV_ERROR_INVALID_ARGS);
    
    double phase_time = get_time_ms();
    
    if (analyze_column_widths(viewer->file_data, viewer->parsed_data, viewer->display_state, viewer->config) != DSV_OK) {
        LOG_ERROR("Failed to analyze columns.");
        return DSV_ERROR_DISPLAY;
    }
    
    configure_viewer_settings(viewer, viewer->config);
    initialize_viewer_cache(viewer, viewer->config);
    
    printf("Display features: %.2f ms\n", get_time_ms() - phase_time);
    return DSV_OK;
}

// --- Main Initialization Function ---

DSVResult init_viewer(DSVViewer *viewer, const char *filename, char delimiter, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(filename, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(config, DSV_ERROR_INVALID_ARGS);
    
    DSVResult res;
    double phase_time, total_time;
    total_time = get_time_ms();

    // Core components
    phase_time = get_time_ms();
    res = init_viewer_components(viewer, config);
    if (res != DSV_OK) return res;
    init_view_state(&viewer->view_state); // Initialize the global view state
    printf("Core components: %.2f ms\n", get_time_ms() - phase_time);

    // File operations
    res = init_file_system(viewer, filename);
    if (res != DSV_OK) return res;

    // Data structures
    res = init_analysis_system(viewer, delimiter);
    if (res != DSV_OK) return res;

    // Display features
    res = init_display_system(viewer);
    if (res != DSV_OK) return res;

    printf("Total initialization: %.2f ms\n", get_time_ms() - total_time);
    LOG_INFO("Viewer initialized successfully.");
    return DSV_OK;
}

void init_view_state(ViewState *state) {
    state->current_panel = PANEL_TABLE_VIEW;
    state->needs_redraw = true;
    state->table_view.table_start_row = 0;
    state->table_view.table_start_col = 0;
    state->table_view.cursor_row = 0;
    state->table_view.cursor_col = 0;
    
    // Make sure selection state is zeroed out
    state->table_view.row_selected = NULL;
    state->table_view.selection_count = 0;
    state->table_view.total_rows = 0;
    state->current_view = NULL;
} 