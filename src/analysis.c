#include "analysis.h"
#include "viewer.h"
#include "file_and_parse_data.h"
#include "display_state.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// --- Public API Functions ---

void cleanup_column_analysis(ColumnAnalysis *analysis) {
    if (analysis && analysis->col_widths) {
        free(analysis->col_widths);
        analysis->col_widths = NULL;
    }
}

// Analyze column widths by sampling the file data
int analyze_columns_legacy(struct DSVViewer *viewer, const DSVConfig *config) {
    if (!viewer || !viewer->file_data || !config) return -1;

    size_t sample_lines = viewer->file_data->num_lines > (size_t)config->column_analysis_sample_lines ? 
                         (size_t)config->column_analysis_sample_lines : viewer->file_data->num_lines;
    if (sample_lines == 0) {
        viewer->display_state->num_cols = 0;
        viewer->display_state->col_widths = NULL;
        return 0;
    }

    int* col_widths_tmp = calloc(config->max_cols, sizeof(int));
    if (!col_widths_tmp) return -1;

    size_t max_cols_found = 0;
    
    // Reuse the existing fields buffer instead of allocating new one
    FieldDesc* analysis_fields = viewer->file_data->fields;

    for (size_t i = 0; i < sample_lines; i++) {
        // Use the main parse_line function instead of duplicate parsing
        size_t num_fields = parse_line(viewer, viewer->file_data->line_offsets[i], analysis_fields, config->max_cols);

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
        viewer->display_state->col_widths = malloc(num_cols * sizeof(int));
        if (!viewer->display_state->col_widths) {
            free(col_widths_tmp);
            return -1;
        }

        for (size_t i = 0; i < num_cols; i++) {
            viewer->display_state->col_widths[i] = col_widths_tmp[i] > config->max_column_width ? config->max_column_width :
                                                  (col_widths_tmp[i] < config->min_column_width ? config->min_column_width : col_widths_tmp[i]);
        }
        viewer->display_state->num_cols = num_cols;
    } else {
        viewer->display_state->col_widths = NULL;
        viewer->display_state->num_cols = 0;
    }

    free(col_widths_tmp);
    return 0;
} 