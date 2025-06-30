#include "viewer.h"
#include "cache.h"
#include "logging.h"
#include "error_context.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// --- Static Helper Declarations ---
static uint32_t fnv1a_hash(const char *str);
static int init_display_cache(struct DSVViewer *viewer);
static void cleanup_display_cache(struct DSVViewer *viewer);
static int init_cache_memory_pool(struct DSVViewer *viewer);
static void cleanup_cache_memory_pool(struct DSVViewer *viewer);
static int init_string_intern_table(struct DSVViewer *viewer);
static void cleanup_string_intern_table(struct DSVViewer *viewer);
static const char* intern_string(struct DSVViewer *viewer, const char* str);
static char* pool_strdup(struct DSVViewer *viewer, const char* str);
static DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer);

// --- Hashing ---
static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 0x811c9dc5;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)(*p);
        hash *= 0x01000193;
    }
    return hash;
}

// --- Memory Pool Management ---
static DisplayCacheEntry* pool_alloc_entry(struct DSVViewer *viewer) {
    if (!viewer->mem_pool || (size_t)viewer->mem_pool->entry_pool_used >= (size_t)viewer->config->cache_size * 2) return NULL;
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

static char* pool_strdup(struct DSVViewer *viewer, const char* str) {
    if (!viewer->mem_pool) return NULL;
    size_t len = strlen(str) + 1;
    if (viewer->mem_pool->string_pool_used + len > viewer->config->cache_string_pool_size) return NULL;
    char* dest = viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used;
    memcpy(dest, str, len);
    viewer->mem_pool->string_pool_used += len;
    return dest;
}

static int init_cache_memory_pool(struct DSVViewer *viewer) {
    viewer->mem_pool = calloc(1, sizeof(CacheMemoryPool));
    if (!viewer->mem_pool) return -1;

    viewer->mem_pool->entry_pool = calloc(viewer->config->cache_size * 2, sizeof(DisplayCacheEntry));
    if (!viewer->mem_pool->entry_pool) return -1;

    viewer->mem_pool->string_pool = malloc(viewer->config->cache_string_pool_size);
    if (!viewer->mem_pool->string_pool) return -1;

    return 0;
}

static void cleanup_cache_memory_pool(struct DSVViewer *viewer) {
    if (!viewer->mem_pool) return;
    if (viewer->mem_pool->entry_pool) {
        free(viewer->mem_pool->entry_pool);
        viewer->mem_pool->entry_pool = NULL;
    }
    if (viewer->mem_pool->string_pool) {
        free(viewer->mem_pool->string_pool);
        viewer->mem_pool->string_pool = NULL;
    }
    free(viewer->mem_pool);
    viewer->mem_pool = NULL;
}

// --- String Interning ---
static const char* intern_string(struct DSVViewer *viewer, const char* str) {
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
        free(new_entry);
        return str; // On failure, just return original string
    }

    new_entry->next = viewer->intern_table->buckets[index];
    viewer->intern_table->buckets[index] = new_entry;
    return new_entry->str;
}

static int init_string_intern_table(struct DSVViewer *viewer) {
    viewer->intern_table = calloc(1, sizeof(StringInternTable));
    if (!viewer->intern_table) return -1;
    viewer->intern_table->buckets = calloc(viewer->config->intern_table_size, sizeof(StringInternEntry*));
    if (!viewer->intern_table->buckets) {
        free(viewer->intern_table);
        viewer->intern_table = NULL;
        return -1;
    }
    return 0;
}

static void cleanup_string_intern_table(struct DSVViewer *viewer) {
    if (!viewer->intern_table) return;
    for (int i = 0; i < viewer->config->intern_table_size; i++) {
        StringInternEntry *entry = viewer->intern_table->buckets[i];
        while (entry) {
            StringInternEntry *next = entry->next;
            free(entry); // Note: string is freed by memory pool
            entry = next;
        }
        viewer->intern_table->buckets[i] = NULL;
    }
    free(viewer->intern_table->buckets);
    free(viewer->intern_table);
    viewer->intern_table = NULL;
}

// --- Display Cache ---
static int calculate_display_width(struct DSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
    mbstowcs(wcs, str, viewer->config->max_field_len);
    int width = wcswidth(wcs, viewer->config->max_field_len);
    return (width < 0) ? strlen(str) : width;
}

static void truncate_str(struct DSVViewer* viewer, const char* src, char* dest, int width) {
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
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

const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width) {
    if (!viewer->display_cache) return original;
    size_t orig_len = strlen(original);
    if (orig_len <= (size_t)width) return original;
    
    uint32_t hash = fnv1a_hash(original);
    uint32_t index = hash % viewer->config->cache_size;

    for (DisplayCacheEntry *entry = viewer->display_cache->entries[index]; entry; entry = entry->next) {
        if (entry->hash == hash && strcmp(entry->original_string, original) == 0) {
            for (int i = 0; i < entry->truncated_count; i++) {
                if (entry->truncated[i].width == width) return entry->truncated[i].str;
            }
            char *truncated_buffer = viewer->display_state->buffers.cache_buffer;
            truncate_str(viewer, original, truncated_buffer, width);
            if (entry->truncated_count < viewer->config->max_truncated_versions && entry->truncated) {
                int i = entry->truncated_count++;
                entry->truncated[i].width = width;
                entry->truncated[i].str = pool_strdup(viewer, truncated_buffer);
                return entry->truncated[i].str;
            } else {
                return truncated_buffer; // Cache full for this entry
            }
        }
    }
    
    DisplayCacheEntry *new_entry = pool_alloc_entry(viewer);
    if (!new_entry) return original; // Pool full
        
    // Allocate the truncated versions array from the memory pool
    size_t trunc_array_size = sizeof(TruncatedString) * viewer->config->max_truncated_versions;
    if (viewer->mem_pool->string_pool_used + trunc_array_size <= viewer->config->cache_string_pool_size) {
        new_entry->truncated = (TruncatedString*)(viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used);
        viewer->mem_pool->string_pool_used += trunc_array_size;
    } else {
        new_entry->truncated = NULL; // Not enough space
    }
        
    char *truncated_buffer = viewer->display_state->buffers.cache_buffer;
    truncate_str(viewer, original, truncated_buffer, width);

    new_entry->hash = hash;
    new_entry->original_string = (char*)intern_string(viewer, original);
    new_entry->display_width = calculate_display_width(viewer, original);
    new_entry->truncated_count = 0;

    if (new_entry->truncated) {
        new_entry->truncated_count = 1;
        new_entry->truncated[0].width = width;
        new_entry->truncated[0].str = pool_strdup(viewer, truncated_buffer);
    }
    
    new_entry->next = viewer->display_cache->entries[index];
    viewer->display_cache->entries[index] = new_entry;

    if (new_entry->truncated && new_entry->truncated[0].str) {
        return new_entry->truncated[0].str;
    }

    // Fallback if allocation failed
    return truncated_buffer;
}

static int init_display_cache(struct DSVViewer *viewer) {
    viewer->display_cache = calloc(1, sizeof(DisplayCache));
    if (!viewer->display_cache) return -1;
    viewer->display_cache->entries = calloc(viewer->config->cache_size, sizeof(DisplayCacheEntry*));
    if (!viewer->display_cache->entries) {
        free(viewer->display_cache);
        viewer->display_cache = NULL;
        return -1;
    }
    return 0;
}

static void cleanup_display_cache(struct DSVViewer *viewer) {
    if (!viewer->display_cache) return;
    free(viewer->display_cache->entries);
    free(viewer->display_cache);
    viewer->display_cache = NULL;
}

// --- Public-Facing System Functions ---
DSVResult init_cache_system(struct DSVViewer *viewer, const DSVConfig *config) {
    // Config is already in the viewer struct, but this makes the dependency explicit.
    (void)config; 
    if (init_cache_memory_pool(viewer) != 0) return DSV_ERROR_CACHE;
    if (init_string_intern_table(viewer) != 0) return DSV_ERROR_CACHE;
    if (init_display_cache(viewer) != 0) return DSV_ERROR_CACHE;
    return DSV_OK;
}

void cleanup_cache_system(struct DSVViewer *viewer) {
    cleanup_string_intern_table(viewer);
    cleanup_display_cache(viewer);
    cleanup_cache_memory_pool(viewer);
} 