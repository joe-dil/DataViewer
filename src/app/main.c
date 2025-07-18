#include "app_init.h"
#include "config.h"
#include <string.h>
#include <locale.h>
#include <ncurses.h>
#include <stdbool.h>
#include "logging.h"
#include "error_context.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    // --- Pre-initialization ---
    logging_init();
    
    // Initialize locale before any other operations
    setlocale(LC_ALL, "");

    if (argc < 2) {
        LOG_ERROR("Usage: %s <filename> [--config <config_file>] [-d <delimiter>] [--headerless]", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *config_filename = NULL;
    char delimiter = 0;
    bool show_header = true;
    bool benchmark_mode = false;

    // --- Argument Parsing ---
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_filename = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delimiter = argv[++i][0];
        } else if (strcmp(argv[i], "--headerless") == 0) {
            show_header = false;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            benchmark_mode = true;
        }
    }

    // --- Configuration Loading ---
    DSVConfig config;
    config_init_defaults(&config);
    if (config_filename) {
        if (config_load_from_file(&config, config_filename) != DSV_OK) {
            LOG_WARN("Could not load config from '%s', using defaults.", config_filename);
        }
    }

    if (config_validate(&config) != DSV_OK) {
        LOG_ERROR("Configuration validation failed. Exiting.");
        return 1;
    }

    // --- Viewer Initialization ---
    DSVViewer viewer = {0};
    double start_time = get_time_ms();
    DSVResult result = init_viewer(&viewer, filename, delimiter, &config);
    
    if (result != DSV_OK) {
        LOG_ERROR("Initialization failed: %s", dsv_result_to_string(result));
        cleanup_viewer(&viewer);
        return 1;
    }
    
    // Apply header visibility setting
    viewer.display_state->show_header = show_header;

    if (benchmark_mode) {
        printf("Benchmark mode: init complete in %.2fms\n", 
               get_time_ms() - start_time);
        cleanup_viewer(&viewer);
        return 0;
    }

#ifndef TEST_BUILD
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);    // Error messages
    
    // Set up mouse events if supported
    mouseinterval(0); // Disable click delay
#endif
    
    run_viewer(&viewer);

#ifndef TEST_BUILD    
    endwin();
#endif
    cleanup_viewer(&viewer);
    
    return 0;
} 