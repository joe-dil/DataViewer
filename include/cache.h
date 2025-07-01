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

// Type-safe allocator for cache data - separate pools for different types
typedef struct CacheAllocator {
    // Entry pool for DisplayCacheEntry structs
    DisplayCacheEntry* entry_pool;
    int entry_pool_used;
    
    // String pool for character data
    char* string_pool;
    size_t string_pool_used;
    
    // Separate pool for TruncatedString arrays (properly aligned)
    TruncatedString* truncated_pool;
    int truncated_pool_used;
    
    // Pool for StringInternEntry structs
    StringInternEntry* intern_entry_pool;
    int intern_entry_pool_used;
} CacheAllocator;


// --- Public Function Declarations for the Cache Subsystem ---

/**
 * @brief Initialize display cache system for string truncation performance.
 * Caches truncated string versions to avoid repeated computation.
 * @param viewer Viewer instance to initialize cache for
 * @param config Configuration specifying cache sizes and thresholds
 * @return DSV_OK on success, DSV_ERROR_MEMORY on allocation failure
 */
DSVResult init_cache_system(struct DSVViewer *viewer, const DSVConfig *config);

/**
 * @brief Clean up all cache resources and memory pools.
 * @param viewer Viewer instance (safe to call with NULL)
 */
void cleanup_cache_system(struct DSVViewer *viewer);

/**
 * @brief Get truncated version of string, using cache for performance.
 * Returns cached version if available, otherwise computes and caches result.
 * @param viewer Viewer instance with initialized cache
 * @param original Original string to truncate
 * @param width Maximum display width in characters
 * @return Pointer to truncated string (may be original if no truncation needed)
 */
const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width);

#endif // CACHE_H 