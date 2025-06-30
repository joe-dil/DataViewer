#include "viewer.h"
#include "display_state.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// --- Core Parsing Logic ---

// A state machine for the parser to improve readability.
typedef struct {
    int in_quotes;
    int needs_unescaping;
    size_t field_start;
    size_t field_count;
} ParseState;

// Helper to record a new field, avoiding duplicate code.
static void record_field(DSVViewer *viewer, FieldDesc *fields, size_t *field_count, int max_fields, size_t field_start, size_t current_pos, int needs_unescaping) {
    if (*field_count < (size_t)max_fields) {
        fields[*field_count] = (FieldDesc){viewer->file_data->data + field_start, current_pos - field_start, needs_unescaping};
        (*field_count)++;
    }
}

size_t parse_line(DSVViewer *viewer, size_t offset, FieldDesc *fields, int max_fields) {
    if (!viewer || !viewer->file_data || !viewer->file_data->data || offset >= viewer->file_data->length) {
        return 0;
    }

    // Cache frequently accessed variables from viewer->file_data to local variables.
    // This reduces pointer dereferencing overhead inside the loop.
    const char *data = viewer->file_data->data;
    const size_t length = viewer->file_data->length;
    const char delimiter = viewer->file_data->delimiter;

    // Initialize the state machine for the parser.
    ParseState state = {
        .in_quotes = 0,
        .needs_unescaping = 0,
        .field_start = offset,
        .field_count = 0,
    };

    size_t i = offset;
    while (i < length) {
        char c = data[i];

        // State: Inside a quoted field
        if (state.in_quotes) {
            if (c == '"') {
                // Check for an escaped quote ("")
                if (i + 1 < length && data[i + 1] == '"') {
                    state.needs_unescaping = 1;
                    i++; // Skip the second quote; loop will increment over the first
                } else {
                    state.in_quotes = 0; // This is a closing quote
                }
            }
        // State: Not in a quoted field
        } else {
            if (c == '"') {
                state.in_quotes = 1; // Start of a quoted field
            } else if (c == delimiter) {
                // End of a field, record it
                record_field(viewer, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);
                state.field_start = i + 1;
                state.needs_unescaping = 0; // Reset for next field
            } else if (c == '\n') {
                // End of the line, exit the loop
                break;
            }
        }
        i++;
    }
    
    // After the loop, there's always one last field to record.
    // This handles lines that don't end with a delimiter or newline.
    record_field(viewer, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);

    viewer->file_data->num_fields = state.field_count;
    return state.field_count;
}

// Helper to handle unquoting and unescaping logic shared by render and width calculation
static void unquote_field(const FieldDesc *field, char *buffer, size_t buffer_size) {
    if (!field->start || field->length == 0) {
        if (buffer_size > 0) buffer[0] = '\0';
        return;
    }

    const char *src = field->start;
    size_t src_len = field->length;
    size_t dst_pos = 0;

    // Remove surrounding quotes if they exist
    if (src_len >= 2 && src[0] == '"' && src[src_len-1] == '"') {
        src++;
        src_len -= 2;
    }

    // Handle escaped quotes ("")
    if (field->needs_unescaping) {
        for (size_t i = 0; i < src_len && dst_pos < buffer_size - 1; i++) {
            if (src[i] == '"' && i + 1 < src_len && src[i + 1] == '"') {
                buffer[dst_pos++] = '"';
                i++; // Skip the second quote
            } else {
                buffer[dst_pos++] = src[i];
            }
        }
    } else {
        // Simple copy if no unescaping is needed
        size_t copy_len = src_len < buffer_size - 1 ? src_len : buffer_size - 1;
        memcpy(buffer, src, copy_len);
        dst_pos = copy_len;
    }
    buffer[dst_pos] = '\0';
}

void render_field(const FieldDesc *field, char *buffer, size_t buffer_size) {
    if (!field || !field->start || !buffer || buffer_size == 0) {
        if (buffer && buffer_size > 0) buffer[0] = '\0';
        return;
    }

    unquote_field(field, buffer, buffer_size);
    // Replace newlines with spaces for single-line display
    for(size_t i = 0; buffer[i] != '\0'; ++i) {
        if (buffer[i] == '\n') {
            buffer[i] = ' ';
        }
    }
}

// This function is currently not used but might be useful for future optimizations
// where we only need the width without the full rendered string.
int get_field_display_width(const FieldDesc *field, const DSVConfig *config) {
    if (!field || !field->start || field->length == 0) {
        return 0;
    }

    // This is a simplified version. A more robust implementation would handle this better.
    // For now, assume a temporary buffer is sufficient.
    char temp_buffer[1024]; // Simplified, should use a managed buffer
    wchar_t wcs[1024];

    unquote_field(field, temp_buffer, 1024);

    mbstowcs(wcs, temp_buffer, config->max_field_len);
    int display_width = wcswidth(wcs, config->max_field_len);
    return (display_width < 0) ? strlen(temp_buffer) : display_width;
} 