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
#include <pthread.h>

// Include headers for modularized components
// These headers contain the necessary struct definitions and function prototypes.
#include "cache.h"
#include "display_state.h"

// General constants
#define MAX_COLS 256
#define BUFFER_SIZE 8192

// Zero-copy field descriptor
typedef struct {
    const char *start;
    size_t length;
    int needs_unescaping;
} FieldDesc;

// A component to hold file and parsing related data.
typedef struct {
    char *data;
    size_t length;
    int fd;
    char delimiter;
    FieldDesc *fields;
    size_t num_fields;
    size_t *line_offsets;
    size_t num_lines;
    size_t capacity;
} FileAndParseData;

// Core data structure
typedef struct DSVViewer {
    // Pointers to modularized components.
    struct DisplayCache *display_cache;
    struct CacheMemoryPool *mem_pool;
    struct StringInternTable *intern_table;
    
    // New component pointer for our incremental refactor
    DisplayState *display_state;
    FileAndParseData *file_data;

    // Threading
    int num_threads;
} DSVViewer;

// Core application function declarations

// Parser functions (parser.c)
int init_viewer(DSVViewer *viewer, const char *filename, char delimiter);
void cleanup_viewer(DSVViewer *viewer);
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields);
char* render_field(const FieldDesc *field, char *buffer, size_t buffer_size);
int calculate_field_display_width(DSVViewer *viewer, const FieldDesc *field);

// Display functions (display.c)
void display_data(DSVViewer *viewer, size_t start_row, size_t start_col);
void show_help(void);

// Navigation functions (navigation.c)
void run_viewer(DSVViewer *viewer);

#endif // VIEWER_H 