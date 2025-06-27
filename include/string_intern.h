#ifndef STRING_INTERN_H
#define STRING_INTERN_H

#include <stddef.h>

// Forward declare to avoid circular dependency
struct DSVViewer;

#define INTERN_TABLE_SIZE 4096

// A single entry in the string intern table's hash map
typedef struct StringInternEntry {
    char *str;
    struct StringInternEntry *next;
} StringInternEntry;

// A hash-table-based string interning table to store each unique string only once.
typedef struct StringInternTable {
    StringInternEntry *buckets[INTERN_TABLE_SIZE];
} StringInternTable;

// Function declarations for the string interning subsystem
void init_string_intern_table(struct DSVViewer *viewer);
void cleanup_string_intern_table(struct DSVViewer *viewer);
const char* intern_string(struct DSVViewer *viewer, const char* str);

#endif // STRING_INTERN_H 