#include "viewer.h"
#include "viewer_core.h"
#include "file_io.h"

#include <string.h>
#include <stdio.h>

#define CACHE_THRESHOLD_LINES 500
#define CACHE_THRESHOLD_COLS 20

// --- Component Lifecycle Management ---

int init_viewer_components(struct DSVViewer *viewer) {
    viewer->display_state = malloc(sizeof(DisplayState));
    if (!viewer->display_state) {
        perror("Failed to allocate for DisplayState");
        return -1;
    }
    memset(viewer->display_state, 0, sizeof(DisplayState));

    viewer->file_data = malloc(sizeof(FileAndParseData));
    if (!viewer->file_data) {
        perror("Failed to allocate for FileAndParseData");
        free(viewer->display_state); // Clean up already allocated part
        return -1;
    }
    memset(viewer->file_data, 0, sizeof(FileAndParseData));
    return 0;
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
        }
        free(viewer->display_state);
    }

    if (viewer->file_data) {
        free(viewer->file_data);
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
        if (init_cache_system((struct DSVViewer*)viewer) != 0) {
            fprintf(stderr, "Warning: Failed to initialize cache. Continuing without it.\n");
        }
    }
} 