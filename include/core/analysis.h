#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <stddef.h>
#include "field_desc.h"
#include "error_context.h"
#include "config.h"
#include "file_data.h"
#include "parsed_data.h"
#include "display_state.h"

// Forward declarations
struct DSVViewer;
struct View;

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
 * @param file_data File data structure
 * @param parsed_data Parsed data structure
 * @param display_state Display state structure
 * @param config Configuration with column width limits and sample size
 * @return DSV_OK on success, DSV_ERROR_MEMORY on allocation failure
 */
DSVResult analyze_column_widths(const FileData *file_data, const ParsedData *parsed_data, DisplayState *display_state, const DSVConfig *config);


// --- Frequency Analysis ---

// Represents a single unique value and its frequency count
typedef struct {
    const char *value;
    int count;
} FreqValueCount;

// Holds the results of a frequency analysis for a column
typedef struct {
    FreqValueCount *items; // Array of unique values and their counts
    int count;             // Number of unique items
    int capacity;          // Allocated capacity of the items array
    char *column_name;     // Name of the analyzed column
} FreqAnalysisResult;

/**
 * @brief Perform frequency analysis on a specific column of a view.
 *
 * This function analyzes the specified column, counts the occurrences of each
 * unique value, and returns a sorted list of these values and their counts.
 * The results are sorted by count in descending order.
 *
 * @param viewer The main application state.
 * @param view The view to analyze (respects filters).
 * @param column_index The index of the column to analyze.
 * @return A pointer to a FreqAnalysisResult struct, or NULL on failure.
 *         The caller is responsible for freeing the result with
 *         free_frequency_analysis_result().
 */
FreqAnalysisResult* perform_frequency_analysis(struct DSVViewer *viewer, const struct View *view, int column_index);

/**
 * @brief Free the memory allocated for frequency analysis results.
 * @param result The result object to free.
 */
void free_frequency_analysis_result(FreqAnalysisResult *result);


#endif // ANALYSIS_H 