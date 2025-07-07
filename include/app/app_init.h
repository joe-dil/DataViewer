#ifndef APP_INIT_H
#define APP_INIT_H

#include <stddef.h>
#include "field_desc.h"
#include "display_state.h"
#include "file_data.h"
#include "parsed_data.h"
#include "config.h"
#include "buffer_pool.h"
#include "view_manager.h"
#include "view_state.h"

// Forward declarations for cache components
struct DisplayCache;
struct CacheAllocator;
struct StringInternTable;
struct Cache;
struct ErrorContext;

// Core data structure
typedef struct DSVViewer {
    // Pointers to cache components (forward declared)
    struct DisplayCache *display_cache;
    struct CacheAllocator *mem_pool;
    struct StringInternTable *intern_table;
    
    // Component pointers (full definitions available)
    DisplayState *display_state;
    FileData *file_data;
    ParsedData *parsed_data;
    const DSVConfig *config;
    struct Cache *cache;
    struct ErrorContext *error_context;
    BufferPool *buffer_pool;
    ViewManager *view_manager;
    ViewState view_state;
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
 * @param data CSV data
 * @param length Length of CSV data
 * @param delimiter Character delimiter
 * @param offset Byte offset in file where line starts
 * @param fields Array to store parsed field descriptors
 * @param max_fields Maximum number of fields to parse
 * @return Number of fields actually parsed
 */
size_t parse_line(const char *data, size_t length, char delimiter, size_t offset, FieldDesc *fields, size_t max_fields);

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
 * @param state Current view state including cursor and scroll positions
 */
void display_data(DSVViewer *viewer, const ViewState *state);

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

/**
 * @brief Initialize a new ViewState object.
 * @param state The ViewState to initialize.
 */
void init_view_state(ViewState *state);

#endif // APP_INIT_H 