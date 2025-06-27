#include "viewer.h"
#include "cache.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// FNV-1a hash function for cache lookups
static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 0x811c9dc5;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
}

// Helper to calculate display width, backing off to strlen for invalid multi-byte chars
static int calculate_display_width(struct CSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, str, UTILS_MAX_FIELD_LEN);
    int width = wcswidth(wcs, UTILS_MAX_FIELD_LEN);
    return (width < 0) ? strlen(str) : width;
}

// Helper to allocate a DisplayCacheEntry from the memory pool
static DisplayCacheEntry* pool_alloc_entry(struct CSVViewer *viewer) {
    if (!viewer->mem_pool || !viewer->mem_pool->entry_pool) return NULL;

    if (viewer->mem_pool->entry_pool_used >= CACHE_ENTRY_POOL_SIZE) {
        return NULL; // Pool is full
    }
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

// Helper to copy a string into the string memory pool (replaces strdup)
static char* pool_strdup(struct CSVViewer *viewer, const char* str) {
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

// Looks for a string in the intern table. If not found, it adds it.
static const char* intern_string(struct CSVViewer *viewer, const char* str) {
    StringInternTable *table = viewer->intern_table;
    if (!table) return pool_strdup(viewer, str);

    for (int i = 0; i < table->used; i++) {
        if (strcmp(table->strings[i], str) == 0) {
            return table->strings[i];
        }
    }

    if (table->used >= table->capacity) {
        table->capacity *= 2;
        table->strings = realloc(table->strings, sizeof(char*) * table->capacity);
        if (!table->strings) {
            perror("Failed to reallocate intern table");
            return NULL;
        }
    }

    char* new_str = strdup(str);
    if (!new_str) {
        perror("Failed to strdup for intern table");
        return NULL;
    }
    table->strings[table->used] = new_str;
    return table->strings[table->used++];
}

// Truncates a string using wide characters to respect display width
static void truncate_str(struct CSVViewer* viewer, const char* src, char* dest, int width) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, src, UTILS_MAX_FIELD_LEN);

    int current_width = 0;
    int i = 0;
    for (i = 0; wcs[i] != '\0'; ++i) {
        int char_width = wcwidth(wcs[i]);
        if (char_width < 0) char_width = 1;

        if (current_width + char_width > width) {
            break;
        }
        current_width += char_width;
    }
    wcs[i] = '\0';

    wcstombs(dest, wcs, UTILS_MAX_FIELD_LEN);
}

const char* get_truncated_string(struct CSVViewer *viewer, const char* original, int width) {
    if (!viewer->display_cache) return original;
    static char static_buffer[UTILS_MAX_FIELD_LEN];
    uint32_t hash = fnv1a_hash(original);
    uint32_t index = hash % CACHE_SIZE;

    DisplayCacheEntry *entry = viewer->display_cache->entries[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->original_string, original) == 0) {
            for (int i = 0; i < entry->truncated_count; i++) {
                if (entry->truncated[i].width == width) {
                    return entry->truncated[i].str;
                }
            }
            break; 
        }
        entry = entry->next;
    }

    char *truncated_buffer = viewer->buffer_pool->buffer3;
    truncate_str(viewer, original, truncated_buffer, width);

    if (entry) {
        if (entry->truncated_count < MAX_TRUNCATED_VERSIONS) {
            int i = entry->truncated_count++;
            entry->truncated[i].width = width;
            char* pooled_str = pool_strdup(viewer, truncated_buffer);
            if (!pooled_str) {
                entry->truncated_count--;
                strncpy(static_buffer, truncated_buffer, UTILS_MAX_FIELD_LEN - 1);
                static_buffer[UTILS_MAX_FIELD_LEN - 1] = '\0';
                return static_buffer;
            }
            entry->truncated[i].str = pooled_str;
            return entry->truncated[i].str;
        }
        strncpy(static_buffer, truncated_buffer, UTILS_MAX_FIELD_LEN - 1);
        static_buffer[UTILS_MAX_FIELD_LEN - 1] = '\0';
        return static_buffer;
    } else {
        DisplayCacheEntry *new_entry = pool_alloc_entry(viewer);
        if (!new_entry) {
            strncpy(static_buffer, truncated_buffer, UTILS_MAX_FIELD_LEN - 1);
            static_buffer[UTILS_MAX_FIELD_LEN - 1] = '\0';
            return static_buffer;
        }

        new_entry->hash = hash;
        new_entry->original_string = (char*)intern_string(viewer, original);
        if (!new_entry->original_string) {
            strncpy(static_buffer, truncated_buffer, UTILS_MAX_FIELD_LEN - 1);
            static_buffer[UTILS_MAX_FIELD_LEN - 1] = '\0';
            return static_buffer;
        }
        
        new_entry->display_width = calculate_display_width(viewer, original);
        new_entry->truncated_count = 1;
        new_entry->truncated[0].width = width;
        new_entry->truncated[0].str = pool_strdup(viewer, truncated_buffer);
        
        if (!new_entry->truncated[0].str) {
            strncpy(static_buffer, truncated_buffer, UTILS_MAX_FIELD_LEN - 1);
            static_buffer[UTILS_MAX_FIELD_LEN - 1] = '\0';
            return static_buffer;
        }
        
        new_entry->next = viewer->display_cache->entries[index];
        viewer->display_cache->entries[index] = new_entry;

        return new_entry->truncated[0].str;
    }
}

void init_cache_memory_pool(struct CSVViewer *viewer) {
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

void cleanup_cache_memory_pool(struct CSVViewer *viewer) {
    if (!viewer->mem_pool) return;
    free(viewer->mem_pool->entry_pool);
    free(viewer->mem_pool->string_pool);
    free(viewer->mem_pool);
}

void init_string_intern_table(struct CSVViewer *viewer) {
    viewer->intern_table = malloc(sizeof(StringInternTable));
    if (!viewer->intern_table) {
        perror("Failed to allocate string intern table");
        exit(1);
    }
    viewer->intern_table->capacity = 1024;
    viewer->intern_table->used = 0;
    viewer->intern_table->strings = malloc(sizeof(char*) * viewer->intern_table->capacity);
    if (!viewer->intern_table->strings) {
        perror("Failed to allocate strings for intern table");
        exit(1);
    }
}

void cleanup_string_intern_table(struct CSVViewer *viewer) {
    if (!viewer->intern_table) return;

    for (int i = 0; i < viewer->intern_table->used; i++) {
        free(viewer->intern_table->strings[i]);
    }
    free(viewer->intern_table->strings);
    free(viewer->intern_table);
}

void init_display_cache(struct CSVViewer *viewer) {
    viewer->display_cache = malloc(sizeof(DisplayCache));
    if (viewer->display_cache) {
        for (int i = 0; i < CACHE_SIZE; i++) {
            viewer->display_cache->entries[i] = NULL;
        }
    }
}

void cleanup_display_cache(struct CSVViewer *viewer) {
    if (!viewer->display_cache) return;
    free(viewer->display_cache);
} 