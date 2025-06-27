#include "viewer.h"

int main(int argc, char *argv[]) {
    char delimiter = 0; // Default to auto-detect
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [-d <delimiter>]\n", argv[0]);
        return 1;
    }

    if (argc > 3 && strcmp(argv[2], "-d") == 0) {
        delimiter = argv[3][0];
    }

    setlocale(LC_ALL, "");

    DSVViewer viewer = {0};
    
    // The initial values for display are now set inside init_viewer.
    
    if (init_viewer(&viewer, argv[1], delimiter) != 0) {
        return 1;
    }

    initscr();
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    run_viewer(&viewer);

    endwin();
    cleanup_viewer(&viewer);

    return 0;
} 