#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stddef.h>
#include "error_context.h"
#include "config.h"

// Forward declare to avoid circular dependency
struct DSVViewer;

// --- Constants ---
// These are now managed by the config system
// #define CACHE_SIZE 16384
// #define MAX_TRUNCATED_VERSIONS 8
// #define CACHE_STRING_POOL_SIZE (1024 * 1024 * 4) // 4MB pool for strings
// #define CACHE_ENTRY_POOL_SIZE (CACHE_SIZE * 2)
// #define INTERN_TABLE_SIZE 4096

// --- Structs ---

// A single cached truncated string
typedef struct TruncatedString {
    int width;
    char *str;
} TruncatedString;

// A full cache entry for a single original string
typedef struct DisplayCacheEntry {
    uint32_t hash;
    int display_width;
    TruncatedString *truncated;
    int truncated_count;
    char *original_string;
    struct DisplayCacheEntry *next;
} DisplayCacheEntry;

// The main cache hash table
typedef struct DisplayCache {
    DisplayCacheEntry **entries;
} DisplayCache;

// A single entry in the string intern table's hash map
typedef struct StringInternEntry {
    char *str;
    struct StringInternEntry *next;
} StringInternEntry;

// A hash-table-based string interning table
typedef struct StringInternTable {
    StringInternEntry **buckets;
} StringInternTable;

// The memory pool for the cache system
typedef struct CacheMemoryPool {
    DisplayCacheEntry* entry_pool;
    int entry_pool_used;
    char* string_pool;
    size_t string_pool_used;
} CacheMemoryPool;


// --- Public Function Declarations for the Cache Subsystem ---
DSVResult init_cache_system(struct DSVViewer *viewer, const DSVConfig *config);
void cleanup_cache_system(struct DSVViewer *viewer);
const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width);

#endif // CACHE_H 