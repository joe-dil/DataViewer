#include "viewer.h"
#include <string.h>
#include <locale.h>
#include "logging.h"
#include "error_context.h"

int main(int argc, char *argv[]) {
    init_logging(LOG_LEVEL_INFO, "-"); // Log to stderr
    
    if (argc < 2) {
        LOG_ERROR("Usage: %s <filename> [-d <delimiter>]", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    char delimiter = 0;

    if (argc > 3 && strcmp(argv[2], "-d") == 0) {
        delimiter = argv[3][0];
    }

    // Initialize locale before viewer initialization for proper Unicode detection
    setlocale(LC_ALL, "");
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, filename, delimiter);
    
    if (result != DSV_OK) {
        LOG_ERROR("Initialization failed: %s", dsv_result_to_string(result));
        cleanup_viewer(&viewer);
        return 1;
    }

#ifndef TEST_BUILD
    initscr();
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
#endif
    
    run_viewer(&viewer);

#ifndef TEST_BUILD    
    endwin();
#endif
    cleanup_viewer(&viewer);
    
    return 0;
} 