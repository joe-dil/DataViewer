#include "analysis.h"
#include "viewer.h"
#include "file_and_parse_data.h"
#include "display_state.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// --- Forward declarations for static helpers ---
// NEW: Optimized function that eliminates duplicate parsing
static int analyze_column_widths_optimized(DSVViewer *viewer, ColumnAnalysis *result, const DSVConfig *config);

// --- Public API Functions ---

void cleanup_column_analysis(ColumnAnalysis *analysis) {
    if (analysis && analysis->col_widths) {
        free(analysis->col_widths);
        analysis->col_widths = NULL;
    }
}

// Legacy function - deprecated in favor of analyze_column_widths_optimized
int analyze_column_widths(const char *data, size_t length,
                         const size_t *line_offsets, size_t num_lines,
                         char delimiter, ColumnAnalysis *result, const DSVConfig *config) {
    // This function is deprecated - no longer supported
    (void)data; (void)length; (void)line_offsets; (void)num_lines; 
    (void)delimiter; (void)result; (void)config;
    return -1; // Force callers to use the new optimized function
}

int analyze_columns_legacy(struct DSVViewer *viewer, const DSVConfig *config) {
    ColumnAnalysis analysis_result = {0};

    // Use the optimized function that eliminates duplicate parsing
    int ret = analyze_column_widths_optimized(viewer, &analysis_result, config);

    if (ret != 0) {
        return -1;
    }

    viewer->display_state->num_cols = analysis_result.num_cols;
    viewer->display_state->col_widths = analysis_result.col_widths;

    analysis_result.col_widths = NULL;

    return 0;
}

// NEW: Optimized implementation that uses main parse_line function
static int analyze_column_widths_optimized(DSVViewer *viewer, ColumnAnalysis *result, const DSVConfig *config) {
    if (!viewer || !viewer->file_data || !result || !config) return -1;

    size_t sample_lines = viewer->file_data->num_lines > (size_t)config->column_analysis_sample_lines ? 
                         (size_t)config->column_analysis_sample_lines : viewer->file_data->num_lines;
    if (sample_lines == 0) {
        result->num_cols = 0;
        result->col_widths = NULL;
        return 0;
    }

    int* col_widths_tmp = calloc(config->max_cols, sizeof(int));
    if (!col_widths_tmp) return -1;

    size_t max_cols_found = 0;
    
    // Reuse the existing fields buffer instead of allocating new one
    FieldDesc* analysis_fields = viewer->file_data->fields;

    for (size_t i = 0; i < sample_lines; i++) {
        // Use the main parse_line function instead of duplicate parse_line_pure
        size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[i], analysis_fields, config->max_cols);

        if (num_fields > max_cols_found) {
            max_cols_found = num_fields;
        }

        for (size_t col = 0; col < num_fields && col < (size_t)config->max_cols; col++) {
            if (col_widths_tmp[col] >= config->max_column_width) continue;
            
            // Use the main render_field + width calculation instead of duplicate logic
            char temp_buffer[config->max_field_len];
            render_field(&analysis_fields[col], temp_buffer, config->max_field_len);
            int width = strlen(temp_buffer); // Simplified width calculation for now
            
            if (width > col_widths_tmp[col]) {
                col_widths_tmp[col] = width;
            }
        }
    }

    result->num_cols = max_cols_found > (size_t)config->max_cols ? (size_t)config->max_cols : max_cols_found;
    if (result->num_cols > 0) {
        result->col_widths = malloc(result->num_cols * sizeof(int));
        if (!result->col_widths) {
            free(col_widths_tmp);
            return -1;
        }

        for (size_t i = 0; i < result->num_cols; i++) {
            result->col_widths[i] = col_widths_tmp[i] > config->max_column_width ? config->max_column_width :
                                   (col_widths_tmp[i] < config->min_column_width ? config->min_column_width : col_widths_tmp[i]);
        }
    } else {
        result->col_widths = NULL;
    }

    free(col_widths_tmp);
    return 0;
}

// --- All duplicate parsing logic removed ---
// Now using the main parse_line() and render_field() functions instead 