#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

// Forward declare to avoid circular dependency
struct DSVViewer;
struct DisplayCacheEntry;

// Memory Pool definitions for the display cache
#define CACHE_STRING_POOL_SIZE (1024 * 1024 * 4) // 4MB pool for strings
#define CACHE_ENTRY_POOL_SIZE (CACHE_SIZE * 2)   // Pool for cache entries

// The memory pool for the cache system
typedef struct CacheMemoryPool {
    struct DisplayCacheEntry* entry_pool;
    int entry_pool_used;
    char* string_pool;
    size_t string_pool_used;
} CacheMemoryPool;

// Function declarations for the memory pool
void init_cache_memory_pool(struct DSVViewer *viewer);
void cleanup_cache_memory_pool(struct DSVViewer *viewer);
struct DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer);
char* pool_strdup(struct DSVViewer *viewer, const char* str);

#endif // MEMORY_POOL_H 