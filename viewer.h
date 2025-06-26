#ifndef VIEWER_H
#define VIEWER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ncurses.h>
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>

// Constants
#define MAX_COLS 256
#define MAX_FIELD_LEN 1024
#define BUFFER_SIZE 8192

// Global variables
extern int show_header;
extern int supports_unicode;
extern const char* separator;

// Core data structure
typedef struct {
    char *data;
    size_t length;
    int fd;
    char delimiter;
    char **fields;
    int num_fields;
    size_t *line_offsets;
    int num_lines;
    int capacity;
    int *col_widths;
    int num_cols;
} CSVViewer;

// Function declarations

// CSV Parser functions (csv_parser.c)
int init_viewer(CSVViewer *viewer, const char *filename, char delimiter);
void cleanup_viewer(CSVViewer *viewer);
void scan_file(CSVViewer *viewer);
int parse_line(CSVViewer *viewer, size_t offset, char **fields, int max_fields);
char detect_delimiter(const char *data, size_t length);

// Display functions (display.c)
void display_data(CSVViewer *viewer, int start_row, int start_col);
void show_help(void);

// Navigation functions (navigation.c)
void run_viewer(CSVViewer *viewer);

#endif // VIEWER_H 