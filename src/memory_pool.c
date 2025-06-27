#include "viewer.h"
#include "memory_pool.h"
#include "cache.h" // For DisplayCacheEntry
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper to allocate a DisplayCacheEntry from the memory pool
DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer) {
    if (!viewer->mem_pool || !viewer->mem_pool->entry_pool) return NULL;

    if (viewer->mem_pool->entry_pool_used >= CACHE_ENTRY_POOL_SIZE) {
        return NULL; // Pool is full
    }
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

// Helper to copy a string into the string memory pool (replaces strdup)
char* pool_strdup(struct DSVViewer *viewer, const char* str) {
    if (!viewer->mem_pool || !viewer->mem_pool->string_pool) return NULL;
    
    size_t len = strlen(str) + 1;
    if (viewer->mem_pool->string_pool_used + len > CACHE_STRING_POOL_SIZE) {
        return NULL; // Pool is full
    }

    char* dest = viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used;
    memcpy(dest, str, len);
    viewer->mem_pool->string_pool_used += len;

    return dest;
}

void init_cache_memory_pool(struct DSVViewer *viewer) {
    viewer->mem_pool = malloc(sizeof(CacheMemoryPool));
    if (!viewer->mem_pool) {
        viewer->display_cache = NULL;
        return;
    }

    viewer->mem_pool->entry_pool = malloc(sizeof(DisplayCacheEntry) * CACHE_ENTRY_POOL_SIZE);
    viewer->mem_pool->entry_pool_used = 0;

    viewer->mem_pool->string_pool = malloc(CACHE_STRING_POOL_SIZE);
    viewer->mem_pool->string_pool_used = 0;

    if (!viewer->mem_pool->entry_pool || !viewer->mem_pool->string_pool) {
        free(viewer->mem_pool->entry_pool);
        free(viewer->mem_pool->string_pool);
        free(viewer->mem_pool);
        viewer->mem_pool = NULL;
        viewer->display_cache = NULL;
    }
}

void cleanup_cache_memory_pool(struct DSVViewer *viewer) {
    if (!viewer->mem_pool) return;
    free(viewer->mem_pool->entry_pool);
    free(viewer->mem_pool->string_pool);
    free(viewer->mem_pool);
} 