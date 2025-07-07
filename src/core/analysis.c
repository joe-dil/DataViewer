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
static void hash_table_resize(FreqAnalysisHashTable *table);

// Helper to get column name, trying header first
const char* get_column_name(struct DSVViewer *viewer, int column_index, char* buffer, size_t buffer_size) {
    if (viewer->parsed_data && viewer->parsed_data->has_header && column_index < (int)viewer->parsed_data->num_header_fields) {
        // Use the actual header name
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

    // Don't free the strings here - caller is responsible
    free(table->buckets);
    free(table);
}

// Resize the hash table when it gets too full
static void hash_table_resize(FreqAnalysisHashTable *table) {
    if (!table) return;
    
    int new_size = table->size * 2;
    FreqAnalysisEntry** new_buckets = calloc(new_size, sizeof(FreqAnalysisEntry*));
    if (!new_buckets) return; // Can't resize, oh well
    
    // Rehash all existing entries
    for (int i = 0; i < table->size; i++) {
        FreqAnalysisEntry *entry = table->buckets[i];
        while (entry) {
            FreqAnalysisEntry *next = entry->next;
            
            // Rehash to new location
            uint32_t hash = fnv1a_hash(entry->value);
            int new_index = hash % new_size;
            
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            
            entry = next;
        }
    }
    
    // Replace old buckets with new ones
    free(table->buckets);
    table->buckets = new_buckets;
    table->size = new_size;
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

    new_entry->value = value; 
    new_entry->count = 1;
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    table->item_count++;
    
    // Resize if load factor > 0.75
    if (table->item_count > (table->size * 3) / 4) {
        hash_table_resize(table);
    }
}


InMemoryTable* perform_frequency_analysis(struct DSVViewer *viewer, const struct View *view, int column_index) {
    if (!viewer || !view || !view->data_source || column_index < 0) {
        return NULL;
    }

    DataSource *ds = view->data_source;
    size_t col_count = ds->ops->get_col_count(ds->context);
    if ((size_t)column_index >= col_count) {
        return NULL;
    }

    FreqAnalysisHashTable *table = hash_table_create(viewer, INITIAL_FREQ_TABLE_SIZE);
    if (!table) return NULL;

    char *field_buffer = malloc(viewer->config->max_field_len);
    if (!field_buffer) {
        hash_table_destroy(table);
        return NULL;
    }

    bool warning_logged = false;
    size_t num_rows = view->visible_row_count;

    for (size_t i = 0; i < num_rows; i++) {
        size_t actual_row_index = view->visible_rows ? view->visible_rows[i] : i;
        
        FieldDesc fd = ds->ops->get_cell(ds->context, actual_row_index, column_index);
        if (fd.start == NULL || fd.length == 0) {
            continue;
        }

        render_field(&fd, field_buffer, viewer->config->max_field_len);
        
        // Don't use intern_string - it's completely wrong for this use case
        // Just duplicate the string for the hash table
        char* value_copy = strdup(field_buffer);
        if (!value_copy) {
            if (!warning_logged) {
                LOG_WARN("Out of memory during frequency analysis.");
                warning_logged = true;
            }
            continue;
        }
        
        hash_table_increment(table, value_copy);
    }

    free(field_buffer);

    if (table->item_count == 0) {
        hash_table_destroy(table);
        return NULL;
    }

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
            // Don't free the string here - we'll free it after creating the table
        }
    }

    qsort(sorted_items, table->item_count, sizeof(FreqValueCount), compare_freq_counts);
    
    char col_name_buffer[256];
    FieldDesc header_fd = ds->ops->get_header(ds->context, column_index);
    if (header_fd.start) {
        render_field(&header_fd, col_name_buffer, sizeof(col_name_buffer));
    } else {
        snprintf(col_name_buffer, sizeof(col_name_buffer), "Column %d", column_index + 1);
    }

    char table_title_buffer[512];
    snprintf(table_title_buffer, sizeof(table_title_buffer), "Frequency Analysis: %s", col_name_buffer);

    const char *headers[] = {"Value", "Count"};
    InMemoryTable *result_table = create_in_memory_table(table_title_buffer, 2, headers);
    if (!result_table) {
        free(sorted_items);
        hash_table_destroy(table);
        return NULL;
    }

    for (int i = 0; i < table->item_count; i++) {
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%d", sorted_items[i].count);
        const char *row_data[] = {sorted_items[i].value, count_str};
        if (add_in_memory_table_row(result_table, row_data) != 0) {
            free(sorted_items);
            free_in_memory_table(result_table);
            hash_table_destroy(table);
            return NULL;
        }
    }

    free(sorted_items);
    
    // Now free all the strings we allocated
    for (int i = 0; i < table->size; i++) {
        FreqAnalysisEntry *entry = table->buckets[i];
        while (entry) {
            free((char*)entry->value);
            FreqAnalysisEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
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