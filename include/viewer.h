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

// Constants
#define MAX_COLS 256
#define MAX_FIELD_LEN 1024
#define BUFFER_SIZE 8192

// Cache for display widths and truncated strings
#define CACHE_SIZE 16384 // Power of 2 for efficient modulo
#define MAX_TRUNCATED_VERSIONS 8

// Memory Pool definitions for the display cache
#define CACHE_STRING_POOL_SIZE (1024 * 1024 * 4) // 4MB pool for strings
#define CACHE_ENTRY_POOL_SIZE (CACHE_SIZE * 2)   // Pool for cache entries, allows for collisions

// Global variables
extern int show_header;
extern int supports_unicode;
extern const char* separator;

typedef struct {
    int width;
    char *str;
} TruncatedString;

typedef struct DisplayCacheEntry {
    uint32_t hash; // FNV-1a hash of the field content
    int display_width;
    TruncatedString truncated[MAX_TRUNCATED_VERSIONS];
    int truncated_count;
    char *original_string; // Store original string to resolve hash collisions
    struct DisplayCacheEntry *next; // For chaining in case of collision
} DisplayCacheEntry;

typedef struct {
    DisplayCacheEntry *entries[CACHE_SIZE];
} DisplayCache;

// Manages pre-allocated memory for the display cache to reduce malloc calls
typedef struct {
    // Pool for DisplayCacheEntry structs
    DisplayCacheEntry* entry_pool;
    int entry_pool_used;

    // Pool for strings (original and truncated)
    char* string_pool;
    size_t string_pool_used;
} CacheMemoryPool;

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
    DisplayCache *display_cache;
    CacheMemoryPool *mem_pool;
} CSVViewer;

// Function declarations

// Cache and Memory Pool functions
void init_cache_memory_pool(CSVViewer *viewer);
void cleanup_cache_memory_pool(CSVViewer *viewer);
void init_display_cache(CSVViewer *viewer);
void cleanup_display_cache(CSVViewer *viewer);
const char* get_truncated_string(CSVViewer *viewer, const char* original, int width);

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