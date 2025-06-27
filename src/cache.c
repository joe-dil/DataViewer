#include "viewer.h"
#include "cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// --- Static Helper Declarations ---
static uint32_t fnv1a_hash(const char *str);
static void init_display_cache(struct DSVViewer *viewer);
static void cleanup_display_cache(struct DSVViewer *viewer);
static void init_cache_memory_pool(struct DSVViewer *viewer);
static void cleanup_cache_memory_pool(struct DSVViewer *viewer);
static void init_string_intern_table(struct DSVViewer *viewer);
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
    if (!viewer->mem_pool || viewer->mem_pool->entry_pool_used >= CACHE_ENTRY_POOL_SIZE) return NULL;
    return &viewer->mem_pool->entry_pool[viewer->mem_pool->entry_pool_used++];
}

static char* pool_strdup(struct DSVViewer *viewer, const char* str) {
    if (!viewer->mem_pool) return NULL;
    size_t len = strlen(str) + 1;
    if (viewer->mem_pool->string_pool_used + len > CACHE_STRING_POOL_SIZE) return NULL;
    char* dest = viewer->mem_pool->string_pool + viewer->mem_pool->string_pool_used;
    memcpy(dest, str, len);
    viewer->mem_pool->string_pool_used += len;
    return dest;
}

static void init_cache_memory_pool(struct DSVViewer *viewer) {
    viewer->mem_pool = calloc(1, sizeof(CacheMemoryPool));
    viewer->mem_pool->entry_pool = calloc(CACHE_ENTRY_POOL_SIZE, sizeof(DisplayCacheEntry));
    viewer->mem_pool->string_pool = malloc(CACHE_STRING_POOL_SIZE);
}

static void cleanup_cache_memory_pool(struct DSVViewer *viewer) {
    if (!viewer->mem_pool) return;
    free(viewer->mem_pool->entry_pool);
    free(viewer->mem_pool->string_pool);
    free(viewer->mem_pool);
}

// --- String Interning ---
static const char* intern_string(struct DSVViewer *viewer, const char* str) {
    if (!viewer->intern_table) return str;
    uint32_t hash = fnv1a_hash(str);
    uint32_t index = hash % INTERN_TABLE_SIZE;
    StringInternEntry *entry = viewer->intern_table->buckets[index];
    while (entry) {
        if (strcmp(entry->str, str) == 0) return entry->str;
        entry = entry->next;
    }
    StringInternEntry *new_entry = malloc(sizeof(StringInternEntry));
    new_entry->str = pool_strdup(viewer, str);
    new_entry->next = viewer->intern_table->buckets[index];
    viewer->intern_table->buckets[index] = new_entry;
    return new_entry->str;
}

static void init_string_intern_table(struct DSVViewer *viewer) {
    viewer->intern_table = calloc(1, sizeof(StringInternTable));
}

static void cleanup_string_intern_table(struct DSVViewer *viewer) {
    if (!viewer->intern_table) return;
    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        StringInternEntry *entry = viewer->intern_table->buckets[i];
        while (entry) {
            StringInternEntry *next = entry->next;
            free(entry); // Note: string is freed by memory pool
            entry = next;
        }
    }
    free(viewer->intern_table);
}

// --- Display Cache ---
static int calculate_display_width(struct DSVViewer* viewer, const char* str) {
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
    mbstowcs(wcs, str, MAX_FIELD_LEN);
    int width = wcswidth(wcs, MAX_FIELD_LEN);
    return (width < 0) ? strlen(str) : width;
}

static void truncate_str(struct DSVViewer* viewer, const char* src, char* dest, int width) {
    wchar_t* wcs = viewer->display_state->buffers.wide_buffer;
    mbstowcs(wcs, src, MAX_FIELD_LEN);
    int current_width = 0, i = 0;
    for (i = 0; wcs[i] != '\0'; ++i) {
        int char_width = wcwidth(wcs[i]);
        if (char_width < 0) char_width = 1;
        if (current_width + char_width > width) break;
        current_width += char_width;
    }
    wcs[i] = '\0';
    wcstombs(dest, wcs, MAX_FIELD_LEN);
}

const char* get_truncated_string(struct DSVViewer *viewer, const char* original, int width) {
    if (!viewer->display_cache) return original;
    size_t orig_len = strlen(original);
    if (orig_len <= (size_t)width) return original;
    
    uint32_t hash = fnv1a_hash(original);
    uint32_t index = hash % CACHE_SIZE;

    for (DisplayCacheEntry *entry = viewer->display_cache->entries[index]; entry; entry = entry->next) {
        if (entry->hash == hash && strcmp(entry->original_string, original) == 0) {
            for (int i = 0; i < entry->truncated_count; i++) {
                if (entry->truncated[i].width == width) return entry->truncated[i].str;
            }
            char *truncated_buffer = viewer->display_state->buffers.buffer_three;
            truncate_str(viewer, original, truncated_buffer, width);
            if (entry->truncated_count < MAX_TRUNCATED_VERSIONS) {
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
        
    char *truncated_buffer = viewer->display_state->buffers.buffer_three;
    truncate_str(viewer, original, truncated_buffer, width);

    new_entry->hash = hash;
    new_entry->original_string = (char*)intern_string(viewer, original);
    new_entry->display_width = calculate_display_width(viewer, original);
    new_entry->truncated_count = 1;
    new_entry->truncated[0].width = width;
    new_entry->truncated[0].str = pool_strdup(viewer, truncated_buffer);
    
    new_entry->next = viewer->display_cache->entries[index];
    viewer->display_cache->entries[index] = new_entry;
    return new_entry->truncated[0].str;
}

static void init_display_cache(struct DSVViewer *viewer) {
    viewer->display_cache = calloc(1, sizeof(DisplayCache));
}

static void cleanup_display_cache(struct DSVViewer *viewer) {
    if (!viewer->display_cache) return;
    free(viewer->display_cache);
}

// --- Public-Facing System Functions ---
void init_cache_system(struct DSVViewer *viewer) {
    init_cache_memory_pool(viewer);
    init_string_intern_table(viewer);
    init_display_cache(viewer);
}

void cleanup_cache_system(struct DSVViewer *viewer) {
    cleanup_string_intern_table(viewer);
    cleanup_display_cache(viewer);
    cleanup_cache_memory_pool(viewer);
} 