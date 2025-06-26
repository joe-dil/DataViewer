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

// Zero-copy field descriptor
typedef struct {
    const char *start;  // Pointer into mmap'd data
    size_t length;      // Length of field (may include quotes/escapes)
    int needs_unescaping; // Whether field contains escaped quotes
} FieldDesc;

// Core data structure
typedef struct {
    char *data;
    size_t length;
    int fd;
    char delimiter;
    FieldDesc *fields;  // Zero-copy field descriptors
    int num_fields;
    size_t *line_offsets;
    int num_lines;
    int capacity;
    int *col_widths;
    int num_cols;
    char *render_buffer;  // Single buffer for rendering fields when needed
} CSVViewer;

// Function declarations

// CSV Parser functions (csv_parser.c)
int init_viewer(CSVViewer *viewer, const char *filename, char delimiter);
void cleanup_viewer(CSVViewer *viewer);
void scan_file(CSVViewer *viewer);
int parse_line(CSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields);
char detect_delimiter(const char *data, size_t length);
char* render_field(const FieldDesc *field, char *buffer, size_t buffer_size);
int calculate_field_display_width(const FieldDesc *field);

// Display functions (display.c)
void display_data(CSVViewer *viewer, int start_row, int start_col);
void show_help(void);

// Navigation functions (navigation.c)
void run_viewer(CSVViewer *viewer);

#endif // VIEWER_H 