#include "core/value_index.h"
#include "util/utils.h"
#include <stdlib.h>
#include <string.h>

ValueIndex* create_value_index(size_t size) {
    ValueIndex *index = calloc(1, sizeof(ValueIndex));
    if (!index) return NULL;
    index->size = size > 0 ? size : 1024;
    index->buckets = calloc(index->size, sizeof(ValueIndexEntry*));
    if (!index->buckets) {
        free(index);
        return NULL;
    }
    return index;
}

void free_value_index(ValueIndex *index) {
    if (!index) return;
    for (size_t i = 0; i < index->size; i++) {
        ValueIndexEntry *entry = index->buckets[i];
        while (entry) {
            ValueIndexEntry *next = entry->next;
            free(entry->value);
            free(entry->rows.indices);
            free(entry);
            entry = next;
        }
    }
    free(index->buckets);
    free(index);
}

void add_to_value_index(ValueIndex *index, const char *value, size_t *row_indices, size_t count) {
    uint32_t hash = fnv1a_hash(value);
    size_t i = hash % index->size;
    
    // Create new entry
    ValueIndexEntry *entry = malloc(sizeof(ValueIndexEntry));
    entry->value = strdup(value);
    entry->rows.indices = malloc(count * sizeof(size_t));
    memcpy(entry->rows.indices, row_indices, count * sizeof(size_t));
    entry->rows.count = count;
    
    // Insert into bucket
    entry->next = index->buckets[i];
    index->buckets[i] = entry;
    index->count++;
}

const RowIndexArray* get_from_value_index(const ValueIndex *index, const char *value) {
    if (!index) return NULL;
    uint32_t hash = fnv1a_hash(value);
    size_t i = hash % index->size;
    
    for (ValueIndexEntry *entry = index->buckets[i]; entry; entry = entry->next) {
        if (strcmp(entry->value, value) == 0) {
            return &entry->rows;
        }
    }
    return NULL;
} 