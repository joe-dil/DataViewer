#include "field_desc.h"
#include "display_state.h"
#include "file_data.h"
#include "parsed_data.h"
#include "config.h"
#include "analysis.h"
#include "app_init.h"
#include "constants.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <sys/stat.h>

// --- Public API Functions ---

void cleanup_column_analysis(ColumnAnalysis *analysis) {
    if (!analysis) return;
    SAFE_FREE(analysis->col_widths);
}

// Analyze column widths by sampling the file data
DSVResult analyze_column_widths(const FileData *file_data, const ParsedData *parsed_data, DisplayState *display_state, const DSVConfig *config) {
    CHECK_NULL_RET(file_data, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(parsed_data, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(display_state, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(config, DSV_ERROR_INVALID_ARGS);

    size_t sample_lines = parsed_data->num_lines > (size_t)config->column_analysis_sample_lines ? 
                         (size_t)config->column_analysis_sample_lines : parsed_data->num_lines;
    if (sample_lines == 0) {
        display_state->num_cols = 0;
        display_state->col_widths = NULL;
        return DSV_OK;
    }

    int* col_widths_tmp = calloc(config->max_cols, sizeof(int));
    CHECK_ALLOC(col_widths_tmp);

    size_t max_cols_found = 0;
    
    // Reuse the existing fields buffer instead of allocating new one
    FieldDesc* analysis_fields = parsed_data->fields;
    if (!analysis_fields) {
        SAFE_FREE(col_widths_tmp);
        return DSV_ERROR_INVALID_ARGS;
    }

    for (size_t i = 0; i < sample_lines; i++) {
        // Use the main parse_line function instead of duplicate parsing
        size_t num_fields = parse_line(file_data->data, file_data->length, parsed_data->delimiter, parsed_data->line_offsets[i], analysis_fields, config->max_cols);

        if (num_fields > max_cols_found) {
            max_cols_found = num_fields;
        }

        for (size_t col = 0; col < num_fields && col < (size_t)config->max_cols; col++) {
            if (col_widths_tmp[col] >= config->max_column_width) continue;
            
            // Use the main render_field + width calculation instead of duplicate logic
            char temp_buffer[config->max_field_len];
            render_field(&analysis_fields[col], temp_buffer, config->max_field_len);
            int width = strlen(temp_buffer);
            
            if (width > col_widths_tmp[col]) {
                col_widths_tmp[col] = width;
            }
        }
    }

    size_t num_cols = max_cols_found > (size_t)config->max_cols ? (size_t)config->max_cols : max_cols_found;
    if (num_cols > 0) {
        display_state->col_widths = malloc(num_cols * sizeof(int));
        if (!display_state->col_widths) {
            SAFE_FREE(col_widths_tmp);
            return DSV_ERROR_MEMORY;
        }

        for (size_t i = 0; i < num_cols; i++) {
            display_state->col_widths[i] = col_widths_tmp[i] > config->max_column_width ? config->max_column_width :
                                                  (col_widths_tmp[i] < config->min_column_width ? config->min_column_width : col_widths_tmp[i]);
        }
        display_state->num_cols = num_cols;
    } else {
        display_state->col_widths = NULL;
        display_state->num_cols = 0;
    }

    SAFE_FREE(col_widths_tmp);
    return DSV_OK;
} 