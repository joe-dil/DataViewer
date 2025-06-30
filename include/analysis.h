#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <stddef.h>
#include "field_desc.h"
#include "error_context.h"
#include "config.h"

// Forward declarations
struct DSVViewer;

// Analysis results structure - FIXES IMPLICIT DEPENDENCIES
typedef struct {
    int *col_widths;           // Column width array
    size_t num_cols;           // Number of columns
    size_t max_field_length;   // Longest field found
    size_t total_fields;       // Total fields analyzed
} ColumnAnalysis;

// Explicit interface functions - NO MORE HIDDEN DEPENDENCIES
int analyze_column_widths(const char *data, size_t length,
                         const size_t *line_offsets, size_t num_lines,
                         char delimiter, ColumnAnalysis *result, const DSVConfig *config);

// Helper functions with explicit parameters
int calculate_field_width(const char *field_start, size_t field_length,
                         int needs_unescaping);
void cleanup_column_analysis(ColumnAnalysis *analysis);

// Legacy wrapper for compatibility
int analyze_columns_legacy(struct DSVViewer *viewer, const DSVConfig *config);

#endif // ANALYSIS_H 