#ifndef DISPLAY_STATE_H
#define DISPLAY_STATE_H

#include <stddef.h>
#include <wchar.h>
#include <locale.h>

// General constants needed by structs defined below
#define MAX_FIELD_LEN 1024
#define BUFFER_POOL_SIZE 5

// Constants for default separator strings
#define ASCII_SEPARATOR " | "
#define UNICODE_SEPARATOR " │ "

// Enhanced BufferPool with metadata and validation
typedef struct {
    // Named buffers for specific purposes
    char render_buffer[MAX_FIELD_LEN];      // Primary field rendering
    char pad_buffer[MAX_FIELD_LEN];         // Padding operations
    char cache_buffer[MAX_FIELD_LEN];       // Cache lookups
    char temp_buffer[MAX_FIELD_LEN];        // Temporary operations
    char analysis_buffer[MAX_FIELD_LEN];    // Analysis computations
    wchar_t wide_buffer[MAX_FIELD_LEN];     // Wide character support
    
    // Metadata for buffer management
    int buffer_sizes[BUFFER_POOL_SIZE];
    int is_in_use[BUFFER_POOL_SIZE];
    const char* buffer_names[BUFFER_POOL_SIZE];
} ManagedBufferPool;

// Backward compatibility typedef
typedef ManagedBufferPool BufferPool;

// A component to hold state related to the display and UI.
typedef struct {
    int show_header;
    int supports_unicode;
    const char* separator;
    int *col_widths;
    size_t num_cols;
    ManagedBufferPool buffers;
    int needs_redraw;
} DisplayState;

#endif // DISPLAY_STATE_H 