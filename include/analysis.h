#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <stddef.h>
#include "field_desc.h"
#include "error_context.h"
#include "config.h"

// Forward declarations
struct DSVViewer;

// Analysis results structure (for future extensions: histograms, stats, etc.)
typedef struct {
    int *col_widths;           // Column width array
    size_t num_cols;           // Number of columns
    size_t max_field_length;   // Longest field found
    size_t total_fields;       // Total fields analyzed
} ColumnAnalysis;

/**
 * @brief Clean up analysis results and free allocated memory.
 * @param analysis Analysis structure to cleanup (safe to call with NULL)
 */
void cleanup_column_analysis(ColumnAnalysis *analysis);

/**
 * @brief Analyze CSV data to determine optimal column display widths.
 * Samples file content to calculate column widths, respecting min/max limits.
 * Results are stored in viewer->display_state for rendering.
 * @param viewer Viewer instance with loaded file data
 * @param config Configuration with column width limits and sample size
 * @return DSV_OK on success, DSV_ERROR_MEMORY on allocation failure
 */
DSVResult analyze_column_widths(struct DSVViewer *viewer, const DSVConfig *config);

#endif // ANALYSIS_H 