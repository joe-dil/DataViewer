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
#include "logging.h"
#include "util/utils.h"

// --- Public API Functions ---

void cleanup_column_analysis(ColumnAnalysis *analysis) {
    if (!analysis) return;
    SAFE_FREE(analysis->col_widths);
}

// Calculate the width for a single column by sampling the file data
static int calculate_single_column_width(DSVViewer *viewer, int column_index) {
    const FileData *file_data = viewer->file_data;
    const ParsedData *parsed_data = viewer->parsed_data;
    const DSVConfig *config = viewer->config;

    // Handle empty files or invalid column index
    if (parsed_data->num_lines == 0 || column_index < 0 || (size_t)column_index >= viewer->display_state->num_cols) {
        return config->min_column_width;
    }

    size_t sample_lines = parsed_data->num_lines > (size_t)config->column_analysis_sample_lines 
                         ? (size_t)config->column_analysis_sample_lines : parsed_data->num_lines;
    
    int max_width = 0;
    
    // Reuse the existing fields buffer instead of allocating new one
    FieldDesc* analysis_fields = parsed_data->fields;
    if (!analysis_fields) {
        return config->min_column_width;
    }
    
    // Get header width
    if (parsed_data->has_header) {
        char temp_buffer[config->max_field_len];
        render_field(&parsed_data->header_fields[column_index], temp_buffer, config->max_field_len);
        max_width = strlen(temp_buffer);
    }

    for (size_t i = 0; i < sample_lines; i++) {
        size_t num_fields = parse_line(file_data->data, file_data->length, parsed_data->delimiter, parsed_data->line_offsets[i], analysis_fields, config->max_cols);

        if ((size_t)column_index < num_fields) {
            if (max_width >= config->max_column_width) break;
            
            char temp_buffer[config->max_field_len];
            render_field(&analysis_fields[column_index], temp_buffer, config->max_field_len);
            int width = strlen(temp_buffer);
            
            if (width > max_width) {
                max_width = width;
            }
        }
    }

    // Clamp width to configured min/max
    if (max_width > config->max_column_width) max_width = config->max_column_width;
    if (max_width < config->min_column_width) max_width = config->min_column_width;
    
    return max_width;
}

int analysis_get_column_width(struct DSVViewer *viewer, int column_index) {
    if (!viewer || !viewer->display_state || !viewer->display_state->col_widths || column_index < 0 || (size_t)column_index >= viewer->display_state->num_cols) {
        return 0; // Or a default width
    }

    // If width is not calculated yet (is sentinel value), calculate and cache it
    if (viewer->display_state->col_widths[column_index] == -1) {
        int width = calculate_single_column_width(viewer, column_index);
        viewer->display_state->col_widths[column_index] = width;
    }

    return viewer->display_state->col_widths[column_index];
} 

// --- Frequency Analysis ---

// Represents a single unique value and its frequency count, used for sorting internally.
typedef struct {
    const char *value;  // Points to string owned by hash table entries
    int count;
} FreqValueCount;

// Context for the frequency analysis qsort_r
typedef struct {
    bool is_value_numeric;
} FreqSortContext;

#ifdef __APPLE__
// macOS comparator for frequency analysis sort
static int compare_freq_counts(void *context, const void *a, const void *b) {
    FreqSortContext *ctx = (FreqSortContext *)context;
    const FreqValueCount *itemA = (const FreqValueCount *)a;
    const FreqValueCount *itemB = (const FreqValueCount *)b;

    if (itemA->count != itemB->count) {
        return itemB->count - itemA->count;
    }

    if (ctx->is_value_numeric) {
        long long val_a = strtoll(itemA->value, NULL, 10);
        long long val_b = strtoll(itemB->value, NULL, 10);
        if (val_a < val_b) return 1;
        if (val_a > val_b) return -1;
        return 0;
    } else {
        return strcmp(itemB->value, itemA->value);
    }
}
#else
// Linux comparator for frequency analysis sort
static int compare_freq_counts(const void *a, const void *b, void *context) {
    FreqSortContext *ctx = (FreqSortContext *)context;
    const FreqValueCount *itemA = (const FreqValueCount *)a;
    const FreqValueCount *itemB = (const FreqValueCount *)b;

    if (itemA->count != itemB->count) {
        return itemB->count - itemA->count;
    }

    if (ctx->is_value_numeric) {
        long long val_a = strtoll(itemA->value, NULL, 10);
        long long val_b = strtoll(itemB->value, NULL, 10);
        if (val_a < val_b) return 1;
        if (val_a > val_b) return -1;
        return 0;
    } else {
        return strcmp(itemB->value, itemA->value);
    }
}
#endif


#define INITIAL_FREQ_TABLE_SIZE 1024

// An entry in the frequency analysis hash table (internal to this C file)
typedef struct FreqAnalysisEntry {
    const char* value;  // OWNED: malloc'd string, must be freed when entry is destroyed
    int count;
    struct FreqAnalysisEntry* next;
} FreqAnalysisEntry;

// The hash table for frequency analysis (internal to this C file)
typedef struct {
    FreqAnalysisEntry** buckets;
    int size;
    int item_count;
    struct DSVViewer *viewer; // For memory pooling (currently unused)
} FreqAnalysisHashTable;

// Forward declaration for the comparison function
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
// OWNERSHIP: Caller owns the returned table and must call hash_table_destroy()
// Note: Strings stored in the table must be freed separately with hash_table_free_strings()
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
// OWNERSHIP: Frees the table structure, the buckets array, all entries,
// and all strings pointed to by the entries.
static void hash_table_destroy(FreqAnalysisHashTable *table) {
    if (!table) return;

    // Free the linked lists of entries in each bucket
    if (table->buckets) {
    for (int i = 0; i < table->size; i++) {
        FreqAnalysisEntry *entry = table->buckets[i];
        while (entry) {
            FreqAnalysisEntry *next = entry->next;
                free((char*)entry->value); // Free the strdup'd string
            free(entry);
            entry = next;
        }
    }
}

    // Free the buckets array and the table structure itself
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
// OWNERSHIP: This function takes ownership of 'value' parameter.
// - If the value already exists, the passed string is freed
// - If the value is new, the string is stored in the hash table
// - If allocation fails, the string is freed to prevent leaks
static void hash_table_increment(FreqAnalysisHashTable *table, const char *value) {
    if (!table || !value) return;

    uint32_t hash = fnv1a_hash(value);
    int index = hash % table->size;

    // Find existing entry
    for (FreqAnalysisEntry *entry = table->buckets[index]; entry; entry = entry->next) {
        if (strcmp(entry->value, value) == 0) {
            entry->count++;
            free((char*)value);  // Free the duplicate since we found existing entry
            return;
        }
    }

    // Create a new entry
    FreqAnalysisEntry *new_entry = malloc(sizeof(FreqAnalysisEntry));
    if (!new_entry) {
        free((char*)value);  // Free the string on allocation failure
        LOG_WARN("Failed to allocate memory for frequency entry");
        return;
    }

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

/**
 * @brief Perform frequency analysis on a specific column of a view.
 *
 * This function analyzes the values in the given column, counts the occurrences
 * of each unique value, and returns the results as a new in-memory table.
 *
 * MEMORY OWNERSHIP:
 * - Field values are copied (strdup'd) from the data source
 * - The hash table owns all strdup'd strings until cleanup
 * - The returned InMemoryTable creates its own copies of strings
 * - All intermediate allocations are cleaned up before return
 *
 * @param viewer The main application viewer instance.
 * @param view The data view to analyze.
 * @param column_index The index of the column to analyze.
 * @return An InMemoryTable containing the frequency counts, or NULL on failure.
 *         The caller is responsible for freeing this table with free_in_memory_table().
 */
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
        size_t actual_row_index = view_get_displayed_row_index(view, i);
        if (actual_row_index == SIZE_MAX) continue; // Should not happen
        
        FieldDesc fd = ds->ops->get_cell(ds->context, actual_row_index, column_index);
        if (fd.start == NULL || fd.length == 0) {
            continue;
        }

        render_field(&fd, field_buffer, viewer->config->max_field_len);
        
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
        // hash_table_increment now owns value_copy - it will free it on error or if duplicate exists
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
            
            entry = entry->next;
            // Don't free the string here - we'll free it after creating the table
        }
    }

    // --- Secondary Sort Logic ---
    // Check if the 'Value' column is numeric to sort it correctly as a tie-breaker
    bool is_value_numeric = true;
    if (table->item_count > 0) {
        // Sample up to 50 values to decide
        size_t sample_size = table->item_count < 50 ? table->item_count : 50;
        for (size_t i = 0; i < sample_size; i++) {
            if (!is_string_numeric(sorted_items[i].value)) {
                is_value_numeric = false;
                break;
            }
        }
    } else {
        is_value_numeric = false;
    }

    FreqSortContext sort_ctx = { .is_value_numeric = is_value_numeric };

#ifdef __APPLE__
    qsort_r(sorted_items, table->item_count, sizeof(FreqValueCount), &sort_ctx, compare_freq_counts);
#else
    qsort_r(sorted_items, table->item_count, sizeof(FreqValueCount), compare_freq_counts, &sort_ctx);
#endif
    
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
    
    // The strings from the hash table have been copied into the result_table.
    // Now we need to free the hash table and the strings it owns.
    // The hash_table_destroy function now handles freeing the entries correctly.
    hash_table_destroy(table);

    return result_table;
} 