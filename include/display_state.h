#ifndef DISPLAY_STATE_H
#define DISPLAY_STATE_H

#include <stddef.h>
#include <wchar.h>
#include <locale.h>

// General constants needed by structs defined below
#define MAX_FIELD_LEN 1024

// Constants for default separator strings
#define ASCII_SEPARATOR " | "
#define UNICODE_SEPARATOR " │ "

// This was embedded in DSVViewer, now part of the DisplayState
typedef struct {
    char buffer_one[MAX_FIELD_LEN];
    char buffer_two[MAX_FIELD_LEN];
    char buffer_three[MAX_FIELD_LEN];
    wchar_t wide_buffer[MAX_FIELD_LEN];
} BufferPool;

// A component to hold state related to the display and UI.
typedef struct {
    int show_header;
    int supports_unicode;
    const char* separator;
    int *col_widths;
    size_t num_cols;
    BufferPool buffers;
    int needs_redraw;
} DisplayState;

#endif // DISPLAY_STATE_H 