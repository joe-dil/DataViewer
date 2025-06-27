#include "viewer.h"

// Global variable definitions
int show_header = 1; // Default to showing header
int supports_unicode = 0;
const char* separator = " | "; // Default to ASCII

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