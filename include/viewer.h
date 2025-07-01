#ifndef VIEWER_H
#define VIEWER_H

#include <stddef.h>
#include "error_context.h"
#include "field_desc.h"
#include "display_state.h"
#include "file_and_parse_data.h"
#include "config.h"

// Forward declarations for cache components
struct DisplayCache;
struct CacheAllocator;
struct StringInternTable;

// Core data structure
typedef struct DSVViewer {
    // Pointers to cache components (forward declared)
    struct DisplayCache *display_cache;
    struct CacheAllocator *mem_pool;
    struct StringInternTable *intern_table;
    
    // Component pointers (full definitions available)
    DisplayState *display_state;
    FileAndParseData *file_data;
    const DSVConfig *config;
} DSVViewer;

// Core application function declarations

// Core viewer functions (viewer.c)
DSVResult init_viewer(DSVViewer *viewer, const char *filename, char delimiter, const DSVConfig *config);
void cleanup_viewer(DSVViewer *viewer);
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields);
void render_field(const FieldDesc* field, char* buffer, size_t buffer_size);

// Display functions (display.c)
void display_data(DSVViewer *viewer, size_t start_row, size_t start_col);
void show_help(void);

// Navigation functions (navigation.c)
void run_viewer(DSVViewer *viewer);

#endif // VIEWER_H 