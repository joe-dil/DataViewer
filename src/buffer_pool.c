#include "buffer_pool.h"
#include "logging.h"
#include <string.h>

// Internal helper to get buffer index from buffer pointer
static int get_buffer_index(ManagedBufferPool *pool, const char* buffer) {
    if (!pool || !buffer) return -1;
    
    // Check which buffer this pointer refers to
    if (buffer == pool->render_buffer) return 0;
    if (buffer == pool->pad_buffer) return 1;
    if (buffer == pool->cache_buffer) return 2;
    if (buffer == pool->temp_buffer) return 3;
    if (buffer == pool->analysis_buffer) return 4;
    
    return -1; // Not a valid buffer
}

void init_buffer_pool(ManagedBufferPool *pool) {
    if (!pool) {
        LOG_ERROR("NULL pool passed to init_buffer_pool");
        return;
    }
    
    // Initialize buffer metadata
    pool->buffer_sizes[0] = MAX_FIELD_LEN;
    pool->buffer_sizes[1] = MAX_FIELD_LEN;
    pool->buffer_sizes[2] = MAX_FIELD_LEN;
    pool->buffer_sizes[3] = MAX_FIELD_LEN;
    pool->buffer_sizes[4] = MAX_FIELD_LEN;
    
    pool->buffer_names[0] = "render_buffer";
    pool->buffer_names[1] = "pad_buffer";
    pool->buffer_names[2] = "cache_buffer";
    pool->buffer_names[3] = "temp_buffer";
    pool->buffer_names[4] = "analysis_buffer";
    
    // Mark all buffers as available
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        pool->is_in_use[i] = 0;
    }
    
    // Clear all buffers
    memset(pool->render_buffer, 0, MAX_FIELD_LEN);
    memset(pool->pad_buffer, 0, MAX_FIELD_LEN);
    memset(pool->cache_buffer, 0, MAX_FIELD_LEN);
    memset(pool->temp_buffer, 0, MAX_FIELD_LEN);
    memset(pool->analysis_buffer, 0, MAX_FIELD_LEN);
    memset(pool->wide_buffer, 0, MAX_FIELD_LEN * sizeof(wchar_t));
}

char* acquire_buffer(ManagedBufferPool *pool, const char* purpose) {
    if (!pool) {
        LOG_ERROR("NULL pool passed to acquire_buffer");
        return NULL;
    }
    
    // Find first available buffer
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!pool->is_in_use[i]) {
            pool->is_in_use[i] = 1;
            LOG_DEBUG("Acquired %s for %s", pool->buffer_names[i], purpose ? purpose : "unknown");
            
            // Return pointer to the appropriate buffer
            switch (i) {
                case 0: return pool->render_buffer;
                case 1: return pool->pad_buffer;
                case 2: return pool->cache_buffer;
                case 3: return pool->temp_buffer;
                case 4: return pool->analysis_buffer;
                default: return NULL;
            }
        }
    }
    
    LOG_WARN("No buffers available in pool for %s", purpose ? purpose : "unknown");
    return NULL;
}

void release_buffer(ManagedBufferPool *pool, char* buffer) {
    if (!pool || !buffer) {
        LOG_ERROR("NULL pool or buffer passed to release_buffer");
        return;
    }
    
    int index = get_buffer_index(pool, buffer);
    if (index >= 0 && index < BUFFER_POOL_SIZE) {
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

void reset_buffer_pool(ManagedBufferPool *pool) {
    if (!pool) {
        LOG_ERROR("NULL pool passed to reset_buffer_pool");
        return;
    }
    
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        pool->is_in_use[i] = 0;
    }
    
    LOG_DEBUG("Reset buffer pool - all buffers now available");
}

DSVResult validate_buffer_ptr(ManagedBufferPool *pool, const char* buffer) {
    if (!pool || !buffer) {
        return DSV_ERROR_INVALID_ARGS;
    }
    
    int index = get_buffer_index(pool, buffer);
    return (index >= 0 && index < BUFFER_POOL_SIZE) ? DSV_OK : DSV_ERROR_INVALID_ARGS;
} 