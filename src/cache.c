#include "constants.h"
#include "viewer.h"
#include "cache.h"
#include "logging.h"
#include "error_context.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// --- Static Helper Declarations ---
static uint32_t fnv1a_hash(const char *str);
static DSVResult init_display_cache(struct DSVViewer *viewer);
static void cleanup_display_cache(struct DSVViewer *viewer);
static DSVResult init_cache_allocator(struct DSVViewer *viewer);
static void cleanup_cache_allocator(struct DSVViewer *viewer);
static DSVResult init_string_intern_table(struct DSVViewer *viewer);
static void cleanup_string_intern_table(struct DSVViewer *viewer);
static const char* intern_string(struct DSVViewer *viewer, const char* str);
static char* pool_strdup(struct DSVViewer *viewer, const char* str);
static DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer);

// --- Cache Entry Management Helpers ---
static DisplayCacheEntry* find_cache_entry(struct DSVViewer *viewer, const char* original, uint32_t hash);
static const char* find_truncated_version(DisplayCacheEntry *entry, int width);
static const char* add_truncated_version(struct DSVViewer *viewer, DisplayCacheEntry *entry, const char* original, int width);
static DisplayCacheEntry* create_cache_entry(struct DSVViewer *viewer, const char* original, uint32_t hash, int width);

// --- Hashing ---
static uint32_t fnv1a_hash(const char *str) {
    CHECK_NULL_RET(str, 0);
    uint32_t hash = FNV_OFFSET_BASIS;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

// --- Cache Allocator Management ---
static DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(viewer->mem_pool, NULL);
    
    if ((size_t)viewer->mem_pool->entry_pool_used >= (size_t)viewer->config->cache_size * 2) return NULL;
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

static char* pool_strdup(struct DSVViewer *viewer, const char* str) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(viewer->mem_pool, NULL);
    CHECK_NULL_RET(str, NULL);
    
    size_t len = strlen(str) + 1;
    if (viewer->mem_pool->string_pool_used + len > viewer->config->cache_string_pool_size) return NULL;
    char* dest = viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used;
    memcpy(dest, str, len);
    viewer->mem_pool->string_pool_used += len;
    return dest;
}

static DSVResult init_cache_allocator(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->config, DSV_ERROR_INVALID_ARGS);
    
    viewer->mem_pool = calloc(1, sizeof(CacheAllocator));
    CHECK_ALLOC(viewer->mem_pool);

    viewer->mem_pool->entry_pool = calloc(viewer->config->cache_size * 2, sizeof(DisplayCacheEntry));
    if (!viewer->mem_pool->entry_pool) {
        SAFE_FREE(viewer->mem_pool);
        return DSV_ERROR_MEMORY;
    }

    viewer->mem_pool->string_pool = malloc(viewer->config->cache_string_pool_size);
    if (!viewer->mem_pool->string_pool) {
        SAFE_FREE(viewer->mem_pool->entry_pool);
        SAFE_FREE(viewer->mem_pool);
        return DSV_ERROR_MEMORY;
    }

    return DSV_OK;
}

static void cleanup_cache_allocator(struct DSVViewer *viewer) {
    if (!viewer || !viewer->mem_pool) return;
    
    SAFE_FREE(viewer->mem_pool->entry_pool);
    SAFE_FREE(viewer->mem_pool->string_pool);
    SAFE_FREE(viewer->mem_pool);
}

// --- String Interning ---
static const char* intern_string(struct DSVViewer *viewer, const char* str) {
    CHECK_NULL_RET(viewer, str);
    CHECK_NULL_RET(str, NULL);
    if (!viewer->intern_table) return str;
    
    uint32_t hash = fnv1a_hash(str);
    uint32_t index = hash % viewer->config->intern_table_size;
    StringInternEntry *entry = viewer->intern_table->buckets[index];
    
    while (entry) {
        if (strcmp(entry->str, str) == 0) return entry->str;
        entry = entry->next;
    }
    
    StringInternEntry *new_entry = malloc(sizeof(StringInternEntry));
    if (!new_entry) return str; // On failure, just return original string

    new_entry->str = pool_strdup(viewer, str);
    if (!new_entry->str) {
        SAFE_FREE(new_entry);
        return str; // On failure, just return original string
    }

    new_entry->next = viewer->intern_table->buckets[index];
    viewer->intern_table->buckets[index] = new_entry;
    return new_entry->str;
}

static DSVResult init_string_intern_table(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->config, DSV_ERROR_INVALID_ARGS);
    
    viewer->intern_table = calloc(1, sizeof(StringInternTable));
    CHECK_ALLOC(viewer->intern_table);
    
    viewer->intern_table->buckets = calloc(viewer->config->intern_table_size, sizeof(StringInternEntry*));
    if (!viewer->intern_table->buckets) {
        SAFE_FREE(viewer->intern_table);
        return DSV_ERROR_MEMORY;
    }
    
    return DSV_OK;
}

static void cleanup_string_intern_table(struct DSVViewer *viewer) {
    if (!viewer || !viewer->intern_table) return;
    
    if (viewer->intern_table->buckets) {
        for (int i = 0; i < viewer->config->intern_table_size; i++) {
            StringInternEntry *entry = viewer->intern_table->buckets[i];
            while (entry) {
                StringInternEntry *next = entry->next;
                SAFE_FREE(entry); // Note: string is freed by memory pool
                entry = next;
            }
            viewer->intern_table->buckets[i] = NULL;
        }
        SAFE_FREE(viewer->intern_table->buckets);
    }
    SAFE_FREE(viewer->intern_table);
}

// --- Display Cache ---
static int calculate_display_width(struct DSVViewer* viewer, const char* str) {
    CHECK_NULL_RET(viewer, 0);
    CHECK_NULL_RET(str, 0);
    CHECK_NULL_RET(viewer->display_state, 0);
    
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
    if (!wcs) return strlen(str); // Fallback to byte length
    
    mbstowcs(wcs, str, viewer->config->max_field_len);
    int width = wcswidth(wcs, viewer->config->max_field_len);
    return (width < 0) ? strlen(str) : width;
}

static void truncate_str(struct DSVViewer* viewer, const char* src, char* dest, int width) {
    if (!viewer || !src || !dest || !viewer->display_state) return;
    
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
    if (!wcs) {
        // Fallback: simple byte truncation
        size_t max_len = (size_t)width < (size_t)viewer->config->max_field_len ? (size_t)width : (size_t)viewer->config->max_field_len - 1;
        strncpy(dest, src, max_len);
        dest[max_len] = '\0';
        return;
    }
    
    mbstowcs(wcs, src, viewer->config->max_field_len);
    int current_width = 0, i = 0;
    for (i = 0; wcs[i] != '\0'; ++i) {
        int char_width = wcwidth(wcs[i]);
        if (char_width < 0) char_width = 1;
        if (current_width + char_width > width) break;
        current_width += char_width;
    }
    wcs[i] = '\0';
    wcstombs(dest, wcs, viewer->config->max_field_len);
}

// --- Cache Entry Management Helpers ---

// Find existing cache entry for a string
static DisplayCacheEntry* find_cache_entry(struct DSVViewer *viewer, const char* original, uint32_t hash) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(original, NULL);
    CHECK_NULL_RET(viewer->display_cache, NULL);
    
    uint32_t index = hash % viewer->config->cache_size;
    for (DisplayCacheEntry *entry = viewer->display_cache->entries[index]; entry; entry = entry->next) {
        if (entry->hash == hash && strcmp(entry->original_string, original) == 0) {
            return entry;
        }
    }
    return NULL;
}

// Find specific width version in an existing cache entry
static const char* find_truncated_version(DisplayCacheEntry *entry, int width) {
    CHECK_NULL_RET(entry, NULL);
    
    for (int i = 0; i < entry->truncated_count; i++) {
        if (entry->truncated[i].width == width) {
            return entry->truncated[i].str;
        }
    }
    return NULL;
}

// Add new truncated version to existing cache entry
static const char* add_truncated_version(struct DSVViewer *viewer, DisplayCacheEntry *entry, const char* original, int width) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(entry, NULL);
    CHECK_NULL_RET(original, NULL);
    
    char *truncated_buffer = viewer->display_state->buffers.cache_buffer;
    if (!truncated_buffer) return original;
    
    // Create the truncated string
    truncate_str(viewer, original, truncated_buffer, width);
    
    // Try to add to cache if there's space
    if (entry->truncated_count < viewer->config->max_truncated_versions && entry->truncated) {
        int i = entry->truncated_count++;
        entry->truncated[i].width = width;
        entry->truncated[i].str = pool_strdup(viewer, truncated_buffer);
        return entry->truncated[i].str;
    }
    
    // Cache full for this entry, return temporary buffer
    return truncated_buffer;
}

// Create completely new cache entry
static DisplayCacheEntry* create_cache_entry(struct DSVViewer *viewer, const char* original, uint32_t hash, int width) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(original, NULL);
    
    DisplayCacheEntry *new_entry = pool_alloc_entry(viewer);
    if (!new_entry) return NULL; // Pool full
    
    // Allocate the truncated versions array from the memory pool
    size_t trunc_array_size = sizeof(TruncatedString) * viewer->config->max_truncated_versions;
    if (viewer->mem_pool->string_pool_used + trunc_array_size <= viewer->config->cache_string_pool_size) {
        new_entry->truncated = (TruncatedString*)(viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used);
        viewer->mem_pool->string_pool_used += trunc_array_size;
    } else {
        new_entry->truncated = NULL; // Not enough space
    }
    
    // Initialize entry metadata
    new_entry->hash = hash;
    new_entry->original_string = (char*)intern_string(viewer, original);
    new_entry->display_width = calculate_display_width(viewer, original);
    new_entry->truncated_count = 0;
    
    // Create first truncated version
    char *truncated_buffer = viewer->display_state->buffers.cache_buffer;
    if (truncated_buffer) {
        truncate_str(viewer, original, truncated_buffer, width);
        
        if (new_entry->truncated) {
            new_entry->truncated_count = 1;
            new_entry->truncated[0].width = width;
            new_entry->truncated[0].str = pool_strdup(viewer, truncated_buffer);
        }
    }
    
    // Add to hash table
    uint32_t index = hash % viewer->config->cache_size;
    new_entry->next = viewer->display_cache->entries[index];
    viewer->display_cache->entries[index] = new_entry;
    
    return new_entry;
}

// Main truncated string function - now much simpler and focused
const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width) {
    CHECK_NULL_RET(viewer, original);
    CHECK_NULL_RET(original, NULL);
    if (!viewer->display_cache) return original;
    
    // Early return if no truncation needed
    size_t orig_len = strlen(original);
    if (orig_len <= (size_t)width) return original;
    
    uint32_t hash = fnv1a_hash(original);
    
    // Try to find existing cache entry
    DisplayCacheEntry *entry = find_cache_entry(viewer, original, hash);
    if (entry) {
        // Check if we already have this width cached
        const char *cached_version = find_truncated_version(entry, width);
        if (cached_version) return cached_version;
        
        // Add new version to existing entry
        return add_truncated_version(viewer, entry, original, width);
    }
    
    // Create new cache entry
    DisplayCacheEntry *new_entry = create_cache_entry(viewer, original, hash, width);
    if (new_entry && new_entry->truncated && new_entry->truncated[0].str) {
        return new_entry->truncated[0].str;
    }
    
    // Fallback: create truncated string in temporary buffer
    char *truncated_buffer = viewer->display_state->buffers.cache_buffer;
    if (truncated_buffer) {
        truncate_str(viewer, original, truncated_buffer, width);
        return truncated_buffer;
    }
    
    return original;
}

static DSVResult init_display_cache(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    CHECK_NULL_RET(viewer->config, DSV_ERROR_INVALID_ARGS);
    
    viewer->display_cache = calloc(1, sizeof(DisplayCache));
    CHECK_ALLOC(viewer->display_cache);
    
    viewer->display_cache->entries = calloc(viewer->config->cache_size, sizeof(DisplayCacheEntry*));
    if (!viewer->display_cache->entries) {
        SAFE_FREE(viewer->display_cache);
        return DSV_ERROR_MEMORY;
    }
    
    return DSV_OK;
}

static void cleanup_display_cache(struct DSVViewer *viewer) {
    if (!viewer || !viewer->display_cache) return;
    
    SAFE_FREE(viewer->display_cache->entries);
    SAFE_FREE(viewer->display_cache);
}

// --- Public-Facing System Functions ---
DSVResult init_cache_system(struct DSVViewer *viewer, const DSVConfig *config) {
    CHECK_NULL_RET(viewer, DSV_ERROR_INVALID_ARGS);
    // Config is already in the viewer struct, but this makes the dependency explicit.
    (void)config; 
    
    DSVResult result = init_cache_allocator(viewer);
    if (result != DSV_OK) return result;
    
    result = init_string_intern_table(viewer);
    if (result != DSV_OK) {
        cleanup_cache_allocator(viewer);
        return result;
    }
    
    result = init_display_cache(viewer);
    if (result != DSV_OK) {
        cleanup_string_intern_table(viewer);
        cleanup_cache_allocator(viewer);
        return result;
    }
    
    return DSV_OK;
}

void cleanup_cache_system(struct DSVViewer *viewer) {
    if (!viewer) return;
    cleanup_string_intern_table(viewer);
    cleanup_display_cache(viewer);
    cleanup_cache_allocator(viewer);
} 