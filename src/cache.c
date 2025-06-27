#include "viewer.h"
#include "cache.h"
#include "memory_pool.h"
#include "string_intern.h"
#include "hash_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Helper to calculate display width, backing off to strlen for invalid multi-byte chars
static int calculate_display_width(struct DSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, str, UTILS_MAX_FIELD_LEN);
    int width = wcswidth(wcs, UTILS_MAX_FIELD_LEN);
    return (width < 0) ? strlen(str) : width;
}

// Truncates a string using wide characters to respect display width
static void truncate_str(struct DSVViewer* viewer, const char* src, char* dest, int width) {
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

const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width) {
    if (!viewer->display_cache) return original;
    
    // Quick check for common case - if string is already short enough
    size_t orig_len = strlen(original);
    if (orig_len <= (size_t)width) return original;
    
    static char static_buffer[UTILS_MAX_FIELD_LEN];
    uint32_t hash = fnv1a_hash(original);
    uint32_t index = hash % CACHE_SIZE;

    DisplayCacheEntry *entry = viewer->display_cache->entries[index];
    while (entry) {
        // Compare hash first (faster than string comparison)
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

    char *truncated_buffer = viewer->buffer_pool->buffer_three;
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

void init_display_cache(struct DSVViewer *viewer) {
    viewer->display_cache = malloc(sizeof(DisplayCache));
    if (viewer->display_cache) {
        for (int i = 0; i < CACHE_SIZE; i++) {
            viewer->display_cache->entries[i] = NULL;
        }
    }
}

void cleanup_display_cache(struct DSVViewer *viewer) {
    if (!viewer->display_cache) return;
    free(viewer->display_cache);
} 