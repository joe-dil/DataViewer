#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <stddef.h>
#include "field_desc.h"
#include "error_context.h"
#include "config.h"

// Forward declarations
struct DSVViewer;

// Analysis results structure (currently unused, but kept for future extensions)
typedef struct {
    int *col_widths;           // Column width array
    size_t num_cols;           // Number of columns
    size_t max_field_length;   // Longest field found
    size_t total_fields;       // Total fields analyzed
} ColumnAnalysis;

// Clean up analysis results
void cleanup_column_analysis(ColumnAnalysis *analysis);

// Analyze column widths and store them in viewer->display_state
int analyze_columns_legacy(struct DSVViewer *viewer, const DSVConfig *config);

#endif // ANALYSIS_H 