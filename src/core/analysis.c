#include "field_desc.h"
#include "display_state.h"
#include "file_data.h"
#include "parsed_data.h"
#include "config.h"
#include "analysis.h"
#include "app_init.h"
#include "constants.h"
#include "utils.h"
#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <sys/stat.h>
#include <stdint.h>

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

// --- Frequency Analysis ---

// Represents a single unique value and its frequency count, used for sorting internally.
typedef struct {
    const char *value;
    int count;
} FreqValueCount;

#define INITIAL_FREQ_TABLE_SIZE 1024

// An entry in the frequency analysis hash table (internal to this C file)
typedef struct FreqAnalysisEntry {
    const char* value;
    int count;
    struct FreqAnalysisEntry* next;
} FreqAnalysisEntry;

// The hash table for frequency analysis (internal to this C file)
typedef struct {
    FreqAnalysisEntry** buckets;
    int size;
    int item_count;
    struct DSVViewer *viewer; // For memory pooling
} FreqAnalysisHashTable;


// Forward declaration for the comparison function
static int compare_freq_counts(const void *a, const void *b);

// Get the column name, either from the header or by generating one.
static const char* get_column_name(struct DSVViewer *viewer, int column_index, char* buffer, size_t buffer_size) {
    if (viewer && viewer->parsed_data && viewer->parsed_data->has_header && column_index < (int)viewer->parsed_data->num_header_fields) {
        render_field(&viewer->parsed_data->header_fields[column_index], buffer, buffer_size);
        return buffer;
    } else {
        snprintf(buffer, buffer_size, "Column %d", column_index + 1);
        return buffer;
    }
}

// Create and initialize a new hash table for frequency analysis
static FreqAnalysisHashTable* hash_table_create(struct DSVViewer *viewer, int size) {
    if (size <= 0) size = INITIAL_FREQ_TABLE_SIZE;

    FreqAnalysisHashTable* table = calloc(1, sizeof(FreqAnalysisHashTable));
    if (!table) return NULL;

    table->buckets = calloc(size, sizeof(FreqAnalysisEntry*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    table->size = size;
    table->item_count = 0;
    table->viewer = viewer; // Store viewer for string interning
    return table;
}

// Free all memory associated with the hash table
static void hash_table_destroy(FreqAnalysisHashTable *table) {
    if (!table) return;

    // The string values are interned and managed by the cache system,
    // and the entries themselves will be freed with the results array.
    // We only need to free the bucket array and the table structure.
    free(table->buckets);
    free(table);
}

// Increment the count for a given value, or add it if it's new
static void hash_table_increment(FreqAnalysisHashTable *table, const char *value) {
    if (!table || !value) return;

    uint32_t hash = fnv1a_hash(value);
    int index = hash % table->size;

    // Find existing entry
    for (FreqAnalysisEntry *entry = table->buckets[index]; entry; entry = entry->next) {
        if (strcmp(entry->value, value) == 0) {
            entry->count++;
            return;
        }
    }

    // Create a new entry
    FreqAnalysisEntry *new_entry = malloc(sizeof(FreqAnalysisEntry));
    if (!new_entry) return; // Should handle memory error more gracefully

    // In a real implementation, we would use the string interning from the cache
    // to avoid duplicating strings everywhere. For now, we just point to it.
    // The lifecycle of this string needs to be managed carefully.
    // Let's assume the parser gives us a stable string for the lifetime of the analysis.
    new_entry->value = value; 
    new_entry->count = 1;
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    table->item_count++;
}


InMemoryTable* perform_frequency_analysis(struct DSVViewer *viewer, const struct View *view, int column_index) {
    if (!viewer || !view || column_index < 0 || column_index >= (int)viewer->display_state->num_cols) {
        return NULL;
    }

    FreqAnalysisHashTable *table = hash_table_create(viewer, INITIAL_FREQ_TABLE_SIZE);
    if (!table) return NULL;

    FieldDesc *fields = malloc(sizeof(FieldDesc) * viewer->config->max_cols);
    char *field_buffer = malloc(viewer->config->max_field_len);
    if (!fields || !field_buffer) {
        free(fields);
        free(field_buffer);
        hash_table_destroy(table);
        return NULL;
    }
    
    size_t num_rows = view->visible_row_count;
    for (size_t i = 0; i < num_rows; i++) {
        size_t actual_row_index = view->visible_rows ? view->visible_rows[i] : i;
        if (viewer->parsed_data->has_header) {
            actual_row_index++; // Skip header row
        }

        if (actual_row_index >= viewer->parsed_data->num_lines) continue;

        size_t line_offset = viewer->parsed_data->line_offsets[actual_row_index];
        size_t num_fields = parse_line(viewer->file_data->data, viewer->file_data->length, viewer->parsed_data->delimiter, line_offset, fields, viewer->config->max_cols);

        if ((size_t)column_index < num_fields) {
            render_field(&fields[column_index], field_buffer, viewer->config->max_field_len);
            // We need to intern the string to have a stable pointer for the hash table key
            const char* interned_value = intern_string(viewer, field_buffer);
            hash_table_increment(table, interned_value);
        }
    }

    free(fields);
    free(field_buffer);

    if (table->item_count == 0) {
        hash_table_destroy(table);
        return NULL; // No data to analyze
    }

    // Convert hash table to a sorted array first
    FreqValueCount *sorted_items = malloc(table->item_count * sizeof(FreqValueCount));
    if (!sorted_items) {
        hash_table_destroy(table);
        return NULL;
    }

    int current_item = 0;
    for (int i = 0; i < table->size; i++) {
        FreqAnalysisEntry *entry = table->buckets[i];
        while (entry) {
            sorted_items[current_item].value = entry->value;
            sorted_items[current_item].count = entry->count;
            current_item++;
            
            FreqAnalysisEntry *to_free = entry;
            entry = entry->next;
            free(to_free); // Free the hash table entry now that we've copied it
        }
    }

    // Sort the results by count
    qsort(sorted_items, table->item_count, sizeof(FreqValueCount), compare_freq_counts);
    
    // Get column name
    char col_name_buffer[256];
    char table_title_buffer[512];
    get_column_name(viewer, column_index, col_name_buffer, sizeof(col_name_buffer));
    snprintf(table_title_buffer, sizeof(table_title_buffer), "Frequency Analysis: %s", col_name_buffer);

    // Now, create the InMemoryTable
    const char *headers[] = {"Value", "Count"};
    InMemoryTable *result_table = create_in_memory_table(table_title_buffer, 2, headers);
    if (!result_table) {
        free(sorted_items);
        hash_table_destroy(table);
        return NULL;
    }

    // Populate the InMemoryTable from the sorted array
    for (int i = 0; i < table->item_count; i++) {
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%d", sorted_items[i].count);
        const char *row_data[] = {sorted_items[i].value, count_str};
        if (add_in_memory_table_row(result_table, row_data) != 0) {
            // Handle error, free everything
            free(sorted_items);
            free_in_memory_table(result_table);
            hash_table_destroy(table);
            return NULL;
        }
    }

    free(sorted_items);
    hash_table_destroy(table);

    return result_table;
}

// --- Comparison Functions ---

// qsort comparison function for sorting frequency counts in descending order
static int compare_freq_counts(const void *a, const void *b) {
    const FreqValueCount *itemA = (const FreqValueCount *)a;
    const FreqValueCount *itemB = (const FreqValueCount *)b;
    return itemB->count - itemA->count; // Descending order
} 