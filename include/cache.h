#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stddef.h>

// Forward declare to avoid circular dependency
struct DSVViewer;

// Cache for display widths and truncated strings
#define CACHE_SIZE 16384 // Power of 2 for efficient modulo
#define MAX_TRUNCATED_VERSIONS 8

// Memory Pool definitions for the display cache
#define CACHE_STRING_POOL_SIZE (1024 * 1024 * 4) // 4MB pool for strings
#define CACHE_ENTRY_POOL_SIZE (CACHE_SIZE * 2)   // Pool for cache entries

// A simple string interning table
typedef struct StringInternTable {
    char** strings;
    int capacity;
    int used;
} StringInternTable;

// A single cached truncated string
typedef struct TruncatedString {
    int width;
    char *str;
} TruncatedString;

// A full cache entry for a single original string
typedef struct DisplayCacheEntry {
    uint32_t hash;
    int display_width;
    TruncatedString truncated[MAX_TRUNCATED_VERSIONS];
    int truncated_count;
    char *original_string;
    struct DisplayCacheEntry *next;
} DisplayCacheEntry;

// The main cache hash table
typedef struct DisplayCache {
    DisplayCacheEntry *entries[CACHE_SIZE];
} DisplayCache;

// The memory pool for the cache system
typedef struct CacheMemoryPool {
    DisplayCacheEntry* entry_pool;
    int entry_pool_used;
    char* string_pool;
    size_t string_pool_used;
} CacheMemoryPool;

// Function declarations for the cache subsystem
void init_cache_memory_pool(struct DSVViewer *viewer);
void cleanup_cache_memory_pool(struct DSVViewer *viewer);
void init_string_intern_table(struct DSVViewer *viewer);
void cleanup_string_intern_table(struct DSVViewer *viewer);
void init_display_cache(struct DSVViewer *viewer);
void cleanup_display_cache(struct DSVViewer *viewer);
const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width);

#endif // CACHE_H 