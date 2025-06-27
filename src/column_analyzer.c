#include "viewer.h"
#include "column_analyzer.h"
#include <stdlib.h> // For malloc

void analyze_columns(struct DSVViewer *viewer) {
    // Adaptive sampling: smaller sample for very large files to improve startup time
    viewer->num_cols = 0;
    size_t sample_size;
    if (viewer->num_lines > 10000) {
        sample_size = 500;  // Reduced sample for very large files
    } else if (viewer->num_lines > 1000) {
        sample_size = 1000;
    } else {
        sample_size = viewer->num_lines;
    }

    // Temporary width tracking
    int temp_widths[MAX_COLS];
    for (int i = 0; i < MAX_COLS; i++) {
        temp_widths[i] = 8; // Default width
    }

    // Single pass through sample lines with early termination for wide columns
    for (size_t i = 0; i < sample_size; i++) {
        size_t num_fields = parse_line(viewer, viewer->line_offsets[i],
                                  viewer->fields, MAX_COLS);

        // Track max columns
        if (num_fields > viewer->num_cols) {
            viewer->num_cols = num_fields;
        }

        // Track column widths using zero-copy field width calculation
        for (size_t col = 0; col < num_fields && col < MAX_COLS; col++) {
            // Skip width calculation if already at maximum
            if (temp_widths[col] >= 16) continue;
            
            int display_width = calculate_field_display_width(viewer, &viewer->fields[col]);

            if (display_width > temp_widths[col]) {
                temp_widths[col] = display_width;
            }
        }
    }

    // Copy calculated widths with a cap of 16
    viewer->col_widths = malloc(viewer->num_cols * sizeof(int));
    for (size_t i = 0; i < viewer->num_cols; i++) {
        viewer->col_widths[i] = temp_widths[i] > 16 ? 16 :  // Cap at 16 chars max
                               (temp_widths[i] < 4 ? 4 : temp_widths[i]);  // Min 4 chars
    }
} 