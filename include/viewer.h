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
#include <stdint.h>

// Include headers for modularized components
// These headers contain the necessary struct definitions and function prototypes.
#include "utils.h"
#include "cache.h"

// General constants
#define MAX_COLS 256
#define MAX_FIELD_LEN 1024 // Now used by non-module code, so stays here.
#define BUFFER_SIZE 8192

// Global variables
extern int show_header;
extern int supports_unicode;
extern const char* separator;

// Zero-copy field descriptor
typedef struct {
    const char *start;
    size_t length;
    int needs_unescaping;
} FieldDesc;

// Core data structure
typedef struct CSVViewer {
    char *data;
    size_t length;
    int fd;
    char delimiter;
    FieldDesc *fields;
    int num_fields;
    size_t *line_offsets;
    int num_lines;
    int capacity;
    int *col_widths;
    int num_cols;
    
    // Pointers to modularized components.
    // The struct definitions are in the included headers.
    struct DisplayCache *display_cache;
    struct CacheMemoryPool *mem_pool;
    struct BufferPool *buffer_pool;
    struct StringInternTable *intern_table;
} CSVViewer;

// Core application function declarations

// CSV Parser functions (csv_parser.c)
int init_viewer(CSVViewer *viewer, const char *filename, char delimiter);
void cleanup_viewer(CSVViewer *viewer);
void scan_file(CSVViewer *viewer);
int parse_line(CSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields);
char detect_delimiter(const char *data, size_t length);
char* render_field(const FieldDesc *field, char *buffer, size_t buffer_size);
int calculate_field_display_width(CSVViewer *viewer, const FieldDesc *field);

// Display functions (display.c)
void display_data(CSVViewer *viewer, int start_row, int start_col);
void show_help(void);

// Navigation functions (navigation.c)
void run_viewer(CSVViewer *viewer);

#endif // VIEWER_H 