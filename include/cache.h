#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stddef.h>
#include "memory_pool.h"
#include "string_intern.h"

// Forward declare to avoid circular dependency
struct DSVViewer;

// Cache for display widths and truncated strings
#define CACHE_SIZE 16384 // Power of 2 for efficient modulo
#define MAX_TRUNCATED_VERSIONS 8

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

// Function declarations for the cache subsystem
void init_display_cache(struct DSVViewer *viewer);
void cleanup_display_cache(struct DSVViewer *viewer);
const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width);

#endif // CACHE_H 