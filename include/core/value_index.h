#ifndef VALUE_INDEX_H
#define VALUE_INDEX_H

#include <stddef.h>

// Represents the list of rows where a specific value was found.
typedef struct {
    size_t *indices;    // Dynamically allocated array of row indices
    size_t count;       // Number of row indices
} RowIndexArray;

// Represents the complete index for a column analysis.
// This is a simplified hash map from a string value to a RowIndexArray.
typedef struct ValueIndexEntry {
    char *value;
    RowIndexArray rows;
    struct ValueIndexEntry *next;
} ValueIndexEntry;

typedef struct {
    ValueIndexEntry **buckets;
    size_t size;
    size_t count;
} ValueIndex;

// --- Function Declarations ---

ValueIndex* create_value_index(size_t size);
void free_value_index(ValueIndex *index);
const RowIndexArray* get_from_value_index(const ValueIndex *index, const char *value);
void add_to_value_index(ValueIndex *index, const char *value, size_t *row_indices, size_t count);

#endif // VALUE_INDEX_H 