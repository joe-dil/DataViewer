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

/**
 * @brief Initialize a CSV viewer with file and configuration.
 * @param viewer Viewer instance to initialize
 * @param filename Path to CSV file to load
 * @param delimiter Character delimiter override (0 for auto-detection)
 * @param config Configuration settings
 * @return DSV_OK on success, error code on failure
 */
DSVResult init_viewer(DSVViewer *viewer, const char *filename, char delimiter, const DSVConfig *config);

/**
 * @brief Clean up all viewer resources and memory.
 * @param viewer Viewer instance to cleanup (safe to call with NULL)
 */
void cleanup_viewer(DSVViewer *viewer);

/**
 * @brief Parse a single line into field descriptors.
 * @param viewer Viewer instance containing file data
 * @param offset Byte offset in file where line starts
 * @param fields Array to store parsed field descriptors
 * @param max_fields Maximum number of fields to parse
 * @return Number of fields actually parsed
 */
size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields);

/**
 * @brief Render field content into a string buffer.
 * @param field Field descriptor to render
 * @param buffer Output buffer for rendered text
 * @param buffer_size Size of output buffer
 */
void render_field(const FieldDesc* field, char* buffer, size_t buffer_size);

// Display functions (display.c)

/**
 * @brief Display CSV data starting from specified position.
 * @param viewer Viewer instance
 * @param start_row First row to display (0-based)
 * @param start_col First column to display (0-based)
 */
void display_data(DSVViewer *viewer, size_t start_row, size_t start_col);

/**
 * @brief Show help screen with navigation commands.
 */
void show_help(void);

// Navigation functions (navigation.c)

/**
 * @brief Run the interactive viewer main loop.
 * @param viewer Viewer instance to run
 */
void run_viewer(DSVViewer *viewer);

#endif // VIEWER_H 