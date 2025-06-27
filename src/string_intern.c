#include "viewer.h"
#include "string_intern.h"
#include "memory_pool.h" // For pool_strdup
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declare the hash function used from cache.c
uint32_t fnv1a_hash(const char *str);

// Looks for a string in the intern table. If not found, it adds it.
const char* intern_string(struct DSVViewer *viewer, const char* str) {
    StringInternTable *table = viewer->intern_table;
    if (!table) return pool_strdup(viewer, str); // Fallback if interning is off

    uint32_t hash = fnv1a_hash(str);
    uint32_t index = hash % INTERN_TABLE_SIZE;

    // Look for the string in the bucket's linked list
    StringInternEntry *entry = table->buckets[index];
    while (entry) {
        if (strcmp(entry->str, str) == 0) {
            return entry->str; // Found it
        }
        entry = entry->next;
    }

    // String not found, create a new entry
    StringInternEntry *new_entry = malloc(sizeof(StringInternEntry));
    if (!new_entry) {
        perror("Failed to allocate string intern entry");
        return NULL; // Can't intern, fallback might be needed
    }

    new_entry->str = strdup(str);
    if (!new_entry->str) {
        perror("Failed to strdup for intern table");
        free(new_entry);
        return NULL;
    }

    // Add the new entry to the front of the bucket's list
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    
    return new_entry->str;
}

void init_string_intern_table(struct DSVViewer *viewer) {
    viewer->intern_table = malloc(sizeof(StringInternTable));
    if (!viewer->intern_table) {
        perror("Failed to allocate string intern table");
        exit(1);
    }
    // Initialize all buckets to NULL
    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        viewer->intern_table->buckets[i] = NULL;
    }
}

void cleanup_string_intern_table(struct DSVViewer *viewer) {
    if (!viewer->intern_table) return;

    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        StringInternEntry *entry = viewer->intern_table->buckets[i];
        while (entry) {
            StringInternEntry *next = entry->next;
            free(entry->str); // Free the duplicated string
            free(entry);      // Free the entry struct itself
            entry = next;
        }
    }
    free(viewer->intern_table);
} 