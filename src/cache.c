#include "viewer.h"
#include "cache.h"
#include "memory_pool.h"

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
static int calculate_display_width(struct DSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->buffer_pool->wide_buffer;
    mbstowcs(wcs, str, UTILS_MAX_FIELD_LEN);
    int width = wcswidth(wcs, UTILS_MAX_FIELD_LEN);
    return (width < 0) ? strlen(str) : width;
}

// Looks for a string in the intern table. If not found, it adds it.
static const char* intern_string(struct DSVViewer *viewer, const char* str) {
    StringInternTable *table = viewer->intern_table;
    if (!table) return pool_strdup(viewer, str); // Fallback if interning is off

    uint32_t hash = fnv1a_hash(str);
    uint32_t index = hash % INTERN_TABLE_SIZE;

    // Look for the string in the bucket's linked list
    StringInternEntry *entry = table->buckets[index];
    while (entry) {
        if (strcmp(entry->str, str) == 0) {
            return entry->str; // Found it
        }
        entry = entry->next;
    }

    // String not found, create a new entry
    StringInternEntry *new_entry = malloc(sizeof(StringInternEntry));
    if (!new_entry) {
        perror("Failed to allocate string intern entry");
        return NULL; // Can't intern, fallback might be needed
    }

    new_entry->str = strdup(str);
    if (!new_entry->str) {
        perror("Failed to strdup for intern table");
        free(new_entry);
        return NULL;
    }

    // Add the new entry to the front of the bucket's list
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    
    return new_entry->str;
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

void init_string_intern_table(struct DSVViewer *viewer) {
    viewer->intern_table = malloc(sizeof(StringInternTable));
    if (!viewer->intern_table) {
        perror("Failed to allocate string intern table");
        exit(1);
    }
    // Initialize all buckets to NULL
    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        viewer->intern_table->buckets[i] = NULL;
    }
}

void cleanup_string_intern_table(struct DSVViewer *viewer) {
    if (!viewer->intern_table) return;

    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        StringInternEntry *entry = viewer->intern_table->buckets[i];
        while (entry) {
            StringInternEntry *next = entry->next;
            free(entry->str); // Free the duplicated string
            free(entry);      // Free the entry struct itself
            entry = next;
        }
    }
    free(viewer->intern_table);
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