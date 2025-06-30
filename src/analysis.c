#include "analysis.h"
#include "viewer.h"
#include "file_and_parse_data.h"
#include "display_state.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_COLS 256
#define COLUMN_ANALYSIS_SAMPLE_LINES 1000
#define MAX_COLUMN_WIDTH 16
#define MIN_COLUMN_WIDTH 4
#define MAX_FIELD_LEN 1024

// --- Forward declarations for static helpers ---
static size_t parse_line_pure(const char *line_start, const char* file_end, char delimiter, FieldDesc *fields, int max_fields);
static int calculate_field_display_width_pure(const FieldDesc* field);
static char* render_field_pure(const FieldDesc *field, char *buffer, size_t buffer_size);

// --- Public API Functions ---

void cleanup_column_analysis(ColumnAnalysis *analysis) {
    if (analysis && analysis->col_widths) {
        free(analysis->col_widths);
        analysis->col_widths = NULL;
    }
}

int analyze_column_widths(const char *data, size_t length,
                         const size_t *line_offsets, size_t num_lines,
                         char delimiter, ColumnAnalysis *result) {
    if (!data || !line_offsets || !result) return -1;

    size_t sample_lines = num_lines > COLUMN_ANALYSIS_SAMPLE_LINES ? COLUMN_ANALYSIS_SAMPLE_LINES : num_lines;
    if (sample_lines == 0) {
        result->num_cols = 0;
        result->col_widths = NULL;
        return 0;
    }

    int* col_widths_tmp = calloc(MAX_COLS, sizeof(int));
    if (!col_widths_tmp) return -1;

    size_t max_cols_found = 0;
    FieldDesc fields[MAX_COLS];
    const char *file_end = data + length;

    for (size_t i = 0; i < sample_lines; i++) {
        const char *line_start = data + line_offsets[i];
        size_t num_fields = parse_line_pure(line_start, file_end, delimiter, fields, MAX_COLS);

        if (num_fields > max_cols_found) {
            max_cols_found = num_fields;
        }

        for (size_t col = 0; col < num_fields && col < MAX_COLS; col++) {
            if (col_widths_tmp[col] >= MAX_COLUMN_WIDTH) continue;
            int width = calculate_field_display_width_pure(&fields[col]);
            if (width > col_widths_tmp[col]) {
                col_widths_tmp[col] = width;
            }
        }
    }

    result->num_cols = max_cols_found > MAX_COLS ? MAX_COLS : max_cols_found;
    if (result->num_cols > 0) {
        result->col_widths = malloc(result->num_cols * sizeof(int));
        if (!result->col_widths) {
            free(col_widths_tmp);
            return -1;
        }

        for (size_t i = 0; i < result->num_cols; i++) {
            result->col_widths[i] = col_widths_tmp[i] > MAX_COLUMN_WIDTH ? MAX_COLUMN_WIDTH :
                                   (col_widths_tmp[i] < MIN_COLUMN_WIDTH ? MIN_COLUMN_WIDTH : col_widths_tmp[i]);
        }
    } else {
        result->col_widths = NULL;
    }

    free(col_widths_tmp);
    return 0;
}

int analyze_columns_legacy(struct DSVViewer *viewer) {
    ColumnAnalysis analysis_result = {0};

    int ret = analyze_column_widths(
        viewer->file_data->data,
        viewer->file_data->length,
        viewer->file_data->line_offsets,
        viewer->file_data->num_lines,
        viewer->file_data->delimiter,
        &analysis_result
    );

    if (ret != 0) {
        return -1;
    }

    viewer->display_state->num_cols = analysis_result.num_cols;
    viewer->display_state->col_widths = analysis_result.col_widths;

    analysis_result.col_widths = NULL;

    return 0;
}

// --- Pure Helper Implementations (Re-implemented from parser.c) ---

int calculate_field_width(const char *field_start, size_t field_length, int needs_unescaping) {
    (void)needs_unescaping;
    return field_length;
}

typedef struct {
    int in_quotes;
    int needs_unescaping;
    size_t field_start_offset;
    size_t field_count;
} ParseStatePure;

static void record_field_pure(const char *line_start, FieldDesc *fields, size_t *field_count, int max_fields, size_t field_start_offset, size_t current_offset, int needs_unescaping) {
    if (*field_count < (size_t)max_fields) {
        fields[*field_count] = (FieldDesc){
            .start = line_start + field_start_offset,
            .length = current_offset - field_start_offset,
            .needs_unescaping = needs_unescaping
        };
        (*field_count)++;
    }
}

static size_t parse_line_pure(const char *line_start, const char* file_end, char delimiter, FieldDesc *fields, int max_fields) {
    if (!line_start) return 0;

    ParseStatePure state = {0};
    const char *p = line_start;

    while (p < file_end && *p != '\n' && *p != '\r') {
        char c = *p;
        if (state.in_quotes) {
            if (c == '"') {
                if (p + 1 < file_end && *(p + 1) == '"') {
                    state.needs_unescaping = 1;
                    p++;
                } else {
                    state.in_quotes = 0;
                }
            }
        } else {
            if (c == '"') {
                state.in_quotes = 1;
            } else if (c == delimiter) {
                record_field_pure(line_start, fields, &state.field_count, max_fields, state.field_start_offset, p - line_start, state.needs_unescaping);
                state.field_start_offset = (p - line_start) + 1;
                state.needs_unescaping = 0;
            }
        }
        p++;
    }
    record_field_pure(line_start, fields, &state.field_count, max_fields, state.field_start_offset, p - line_start, state.needs_unescaping);
    return state.field_count;
}

static char* render_field_pure(const FieldDesc *field, char *buffer, size_t buffer_size) {
    if (!field || !field->start || !buffer || buffer_size == 0) {
        if (buffer && buffer_size > 0) buffer[0] = '\0';
        return buffer;
    }
    const char *src = field->start;
    size_t src_len = field->length;
    size_t dst_pos = 0;
    if (src_len >= 2 && src[0] == '"' && src[src_len-1] == '"') { src++; src_len -= 2; }
    if (field->needs_unescaping) {
        for (size_t i = 0; i < src_len && dst_pos < buffer_size - 1; i++) {
            if (src[i] == '"' && i + 1 < src_len && src[i + 1] == '"') {
                buffer[dst_pos++] = '"';
                i++;
            } else if (src[i] == '\n') {
                buffer[dst_pos++] = ' ';
            } else {
                buffer[dst_pos++] = src[i];
            }
        }
    } else {
        size_t copy_len = src_len < buffer_size - 1 ? src_len : buffer_size - 1;
        memcpy(buffer, src, copy_len);
        dst_pos = copy_len;
    }
    buffer[dst_pos] = '\0';
    return buffer;
}

static int calculate_field_display_width_pure(const FieldDesc *field) {
    if (!field || !field->start || field->length == 0) return 0;

    char temp_buffer[MAX_FIELD_LEN];
    wchar_t wide_buffer[MAX_FIELD_LEN];

    const char* start = field->start;
    size_t raw_len = field->length;
    if (raw_len >= 2 && start[0] == '"' && start[raw_len - 1] == '"') {
        raw_len -= 2;
        start++;
    }

    if (field->needs_unescaping) {
        render_field_pure(field, temp_buffer, MAX_FIELD_LEN);
        mbstowcs(wide_buffer, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wide_buffer, MAX_FIELD_LEN);
        return display_width < 0 ? strlen(temp_buffer) : display_width;
    } else {
        size_t copy_len = raw_len < MAX_FIELD_LEN - 1 ? raw_len : MAX_FIELD_LEN - 1;
        memcpy(temp_buffer, start, copy_len);
        temp_buffer[copy_len] = '\0';
        mbstowcs(wide_buffer, temp_buffer, MAX_FIELD_LEN);
        int display_width = wcswidth(wide_buffer, MAX_FIELD_LEN);
        return display_width < 0 ? copy_len : display_width;
    }
} 