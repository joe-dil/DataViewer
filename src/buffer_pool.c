#include "buffer_pool.h"
#include "logging.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

// Internal helper to get buffer index from buffer pointer
static int get_buffer_index(const ManagedBufferPool *pool, const char* buffer) {
    if (!pool || !buffer) return -1;
    
    // Check which buffer this pointer refers to
    if (buffer == pool->render_buffer) return 0;
    if (buffer == pool->pad_buffer) return 1;
    if (buffer == pool->cache_buffer) return 2;
    if (buffer == pool->temp_buffer) return 3;
    if (buffer == pool->analysis_buffer) return 4;
    
    return -1; // Not a valid buffer
}

DSVResult init_buffer_pool(ManagedBufferPool *pool, const DSVConfig *config) {
    if (!pool || !config) return DSV_ERROR_INVALID_ARGS;

    pool->pool_size = config->buffer_pool_size;
    size_t field_len = config->max_field_len;

    // Allocate main buffers
    pool->render_buffer = calloc(field_len, sizeof(char));
    pool->pad_buffer = calloc(field_len, sizeof(char));
    pool->cache_buffer = calloc(field_len, sizeof(char));
    pool->temp_buffer = calloc(field_len, sizeof(char));
    pool->analysis_buffer = calloc(field_len, sizeof(char));
    pool->wide_buffer = calloc(field_len, sizeof(wchar_t));

    // Allocate metadata arrays
    pool->buffer_sizes = calloc(pool->pool_size, sizeof(int));
    pool->is_in_use = calloc(pool->pool_size, sizeof(int));
    pool->buffer_names = calloc(pool->pool_size, sizeof(const char*));

    // Check for allocation failures
    if (!pool->render_buffer || !pool->pad_buffer || !pool->cache_buffer || !pool->temp_buffer || !pool->analysis_buffer || !pool->wide_buffer || !pool->buffer_sizes || !pool->is_in_use || !pool->buffer_names) {
        cleanup_buffer_pool(pool);
        return DSV_ERROR_MEMORY;
    }

    for (int i = 0; i < pool->pool_size; i++) {
        pool->buffer_sizes[i] = field_len;
    }

    pool->buffer_names[0] = "render_buffer";
    pool->buffer_names[1] = "pad_buffer";
    pool->buffer_names[2] = "cache_buffer";
    pool->buffer_names[3] = "temp_buffer";
    pool->buffer_names[4] = "analysis_buffer";

    return DSV_OK;
}

char* acquire_buffer(ManagedBufferPool *pool, const char* purpose) {
    if (!pool) {
        LOG_ERROR("NULL pool passed to acquire_buffer");
        return NULL;
    }
    
    for (int i = 0; i < pool->pool_size; i++) {
        if (!pool->is_in_use[i]) {
            pool->is_in_use[i] = 1;
            LOG_DEBUG("Acquired %s for %s", pool->buffer_names[i], purpose ? purpose : "unknown");
            
            switch (i) {
                case 0: return pool->render_buffer;
                case 1: return pool->pad_buffer;
                case 2: return pool->cache_buffer;
                case 3: return pool->temp_buffer;
                case 4: return pool->analysis_buffer;
                default: return NULL; // Should not happen if pool_size is managed
            }
        }
    }
    
    LOG_WARN("No buffers available in pool for %s", purpose ? purpose : "unknown");
    return NULL;
}

void release_buffer(ManagedBufferPool *pool, const char* buffer) {
    if (!pool || !buffer) {
        LOG_ERROR("NULL pool or buffer passed to release_buffer");
        return;
    }
    
    int index = get_buffer_index(pool, buffer);
    if (index >= 0 && index < pool->pool_size) {
        if (pool->is_in_use[index]) {
            pool->is_in_use[index] = 0;
            LOG_DEBUG("Released %s", pool->buffer_names[index]);
        } else {
            LOG_WARN("Attempting to release %s that was not in use", pool->buffer_names[index]);
        }
    } else {
        LOG_ERROR("Invalid buffer pointer passed to release_buffer");
    }
}

void cleanup_buffer_pool(ManagedBufferPool *pool) {
    if (!pool) return;
    free(pool->render_buffer);
    free(pool->pad_buffer);
    free(pool->cache_buffer);
    free(pool->temp_buffer);
    free(pool->analysis_buffer);
    free(pool->wide_buffer);
    free(pool->buffer_sizes);
    free(pool->is_in_use);
    free(pool->buffer_names);
    // Set pointers to NULL to prevent double free
    pool->render_buffer = NULL;
    pool->pad_buffer = NULL;
    pool->cache_buffer = NULL;
    pool->temp_buffer = NULL;
    pool->analysis_buffer = NULL;
    pool->wide_buffer = NULL;
    pool->buffer_sizes = NULL;
    pool->is_in_use = NULL;
    pool->buffer_names = NULL;
}

void reset_buffer_pool(ManagedBufferPool *pool) {
    if (!pool) {
        LOG_ERROR("NULL pool passed to reset_buffer_pool");
        return;
    }
    
    for (int i = 0; i < pool->pool_size; i++) {
        pool->is_in_use[i] = 0;
    }
    
    LOG_DEBUG("Reset buffer pool - all buffers now available");
}

DSVResult validate_buffer_ptr(ManagedBufferPool *pool, const char* buffer) {
    if (!pool || !buffer) {
        return DSV_ERROR_INVALID_ARGS;
    }
    
    int index = get_buffer_index(pool, buffer);
    return (index >= 0 && index < pool->pool_size) ? DSV_OK : DSV_ERROR_INVALID_ARGS;
} 