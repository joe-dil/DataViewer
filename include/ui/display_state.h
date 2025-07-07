#ifndef DISPLAY_STATE_H
#define DISPLAY_STATE_H

#include <stddef.h>
#include <wchar.h>
#include <locale.h>
#include <stdbool.h>

// Forward declaration
struct DSVViewer;

// Constants for default separator strings
#define ASCII_SEPARATOR " | "
#define UNICODE_SEPARATOR " │ "

// Phase 4: Helper structure for complex header calculations
typedef struct {
    int content_width;
    int underline_width;
    size_t last_visible_col;
    bool has_more_columns_right;
    size_t num_fields;
} HeaderLayout;

// Simple work buffers for direct access
typedef struct {
    // Named buffers for specific purposes  
    char *render_buffer;      // Primary field rendering
    char *pad_buffer;         // Padding operations
    char *cache_buffer;       // Cache lookups
    char *temp_buffer;        // Temporary operations
    char *analysis_buffer;    // Analysis computations
    wchar_t *wide_buffer;     // Wide character support
    
    // Simplified metadata (mostly unused now)
    int *buffer_sizes;
    int *is_in_use;
    const char** buffer_names;
    int pool_size;
} WorkBuffers;

// Backward compatibility typedef
typedef WorkBuffers BufferPool;

// A component to hold state related to the display and UI.
typedef struct {
    int show_header;
    int supports_unicode;
    const char* separator;
    int *col_widths;
    size_t num_cols;
    WorkBuffers buffers;
    int needs_redraw;
    char copy_status[256];
    int show_copy_status;
    
    // Error message display
    char error_message[256];
    int show_error_message;
    double error_message_time;  // Time when error was set (for auto-clear)
} DisplayState;

// Error message display function
void set_error_message(struct DSVViewer *viewer, const char *format, ...);

#endif // DISPLAY_STATE_H 