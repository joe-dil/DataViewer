#include "app_init.h"
#include "display_state.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// --- Core Parsing Logic ---

// CSV parsing state machine for handling quotes and delimiters
typedef struct {
    int in_quotes;         // True if currently inside a double-quoted field
    int needs_unescaping;  // True if the field contains escaped quotes ("")
    size_t field_start;    // Byte offset where current field starts
    size_t field_count;    // Number of fields parsed so far
} ParseState;

// Helper to record a new field, avoiding duplicate code.
static void record_field(const char *data, FieldDesc *fields, size_t *field_count, size_t max_fields, size_t field_start, size_t current_pos, int needs_unescaping) {
    if (*field_count < max_fields) {
        fields[*field_count] = (FieldDesc){data + field_start, current_pos - field_start, needs_unescaping};
        (*field_count)++;
    }
}

size_t parse_line(const char *data, size_t length, char delimiter, size_t offset, FieldDesc *fields, size_t max_fields) {
    if (!data || offset >= length) {
        return 0;
    }

    // Initialize parsing state machine
    ParseState state = {
        .in_quotes = 0,
        .needs_unescaping = 0,
        .field_start = offset,
        .field_count = 0,
    };

    size_t i = offset;
    while (i < length) {
        char c = data[i];

        if (state.in_quotes) {
            // Inside quoted field - only quotes can change state
            if (c == '"') {
                // Handle escaped quotes: "" becomes "
                if (i + 1 < length && data[i + 1] == '"') {
                    state.needs_unescaping = 1;
                    i++; // Skip the second quote; loop will increment over the first
                } else {
                    state.in_quotes = 0; // This is a closing quote
                }
            }
        } else {
            // Outside quoted field - delimiters and quotes matter
            if (c == '"') {
                state.in_quotes = 1; // Start of a quoted field
            } else if (c == delimiter) {
                // End of field - record it and start next
                record_field(data, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);
                state.field_start = i + 1;
                state.needs_unescaping = 0; // Reset for next field
            } else if (c == '\n') {
                break; // End of line
            }
        }
        i++;
    }
    
    // Record the final field (lines may not end with delimiter)
    record_field(data, fields, &state.field_count, max_fields, state.field_start, i, state.needs_unescaping);

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

 