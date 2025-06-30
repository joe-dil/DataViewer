#include "buffer_pool.h"
#include "logging.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

// Internal helper to get buffer index from buffer pointer
static int get_buffer_index(const WorkBuffers *buffers, const char* buffer) {
    if (!buffers || !buffer) return -1;
    
    // Check which buffer this pointer refers to
    if (buffer == buffers->render_buffer) return 0;
    if (buffer == buffers->pad_buffer) return 1;
    if (buffer == buffers->cache_buffer) return 2;
    if (buffer == buffers->temp_buffer) return 3;
    if (buffer == buffers->analysis_buffer) return 4;
    
    return -1; // Not a valid buffer
}

DSVResult init_buffer_pool(WorkBuffers *buffers, const DSVConfig *config) {
    if (!buffers || !config) return DSV_ERROR_INVALID_ARGS;

    buffers->pool_size = config->buffer_pool_size;
    size_t field_len = config->max_field_len;

    // Allocate main buffers
    buffers->render_buffer = calloc(field_len, sizeof(char));
    buffers->pad_buffer = calloc(field_len, sizeof(char));
    buffers->cache_buffer = calloc(field_len, sizeof(char));
    buffers->temp_buffer = calloc(field_len, sizeof(char));
    buffers->analysis_buffer = calloc(field_len, sizeof(char));
    buffers->wide_buffer = calloc(field_len, sizeof(wchar_t));

    // Allocate metadata arrays
    buffers->buffer_sizes = calloc(buffers->pool_size, sizeof(int));
    buffers->is_in_use = calloc(buffers->pool_size, sizeof(int));
    buffers->buffer_names = calloc(buffers->pool_size, sizeof(const char*));

    // Check for allocation failures
    if (!buffers->render_buffer || !buffers->pad_buffer || !buffers->cache_buffer || !buffers->temp_buffer || !buffers->analysis_buffer || !buffers->wide_buffer || !buffers->buffer_sizes || !buffers->is_in_use || !buffers->buffer_names) {
        cleanup_buffer_pool(buffers);
        return DSV_ERROR_MEMORY;
    }

    for (int i = 0; i < buffers->pool_size; i++) {
        buffers->buffer_sizes[i] = field_len;
    }

    buffers->buffer_names[0] = "render_buffer";
    buffers->buffer_names[1] = "pad_buffer";
    buffers->buffer_names[2] = "cache_buffer";
    buffers->buffer_names[3] = "temp_buffer";
    buffers->buffer_names[4] = "analysis_buffer";

    return DSV_OK;
}

char* acquire_buffer(WorkBuffers *buffers, const char* purpose) {
    if (!buffers) {
        LOG_ERROR("NULL buffers passed to acquire_buffer");
        return NULL;
    }
    
    for (int i = 0; i < buffers->pool_size; i++) {
        if (!buffers->is_in_use[i]) {
            buffers->is_in_use[i] = 1;
            LOG_DEBUG("Acquired %s for %s", buffers->buffer_names[i], purpose ? purpose : "unknown");
            
            switch (i) {
                case 0: return buffers->render_buffer;
                case 1: return buffers->pad_buffer;
                case 2: return buffers->cache_buffer;
                case 3: return buffers->temp_buffer;
                case 4: return buffers->analysis_buffer;
                default: return NULL; // Should not happen if pool_size is managed
            }
        }
    }
    
    LOG_WARN("No buffers available for %s", purpose ? purpose : "unknown");
    return NULL;
}

void release_buffer(WorkBuffers *buffers, const char* buffer) {
    if (!buffers || !buffer) {
        LOG_ERROR("NULL buffers or buffer passed to release_buffer");
        return;
    }
    
    int index = get_buffer_index(buffers, buffer);
    if (index >= 0 && index < buffers->pool_size) {
        if (buffers->is_in_use[index]) {
            buffers->is_in_use[index] = 0;
            LOG_DEBUG("Released %s", buffers->buffer_names[index]);
        } else {
            LOG_WARN("Attempting to release %s that was not in use", buffers->buffer_names[index]);
        }
    } else {
        LOG_ERROR("Invalid buffer pointer passed to release_buffer");
    }
}

void cleanup_buffer_pool(WorkBuffers *buffers) {
    if (!buffers) return;
    free(buffers->render_buffer);
    free(buffers->pad_buffer);
    free(buffers->cache_buffer);
    free(buffers->temp_buffer);
    free(buffers->analysis_buffer);
    free(buffers->wide_buffer);
    free(buffers->buffer_sizes);
    free(buffers->is_in_use);
    free(buffers->buffer_names);
    // Set pointers to NULL to prevent double free
    buffers->render_buffer = NULL;
    buffers->pad_buffer = NULL;
    buffers->cache_buffer = NULL;
    buffers->temp_buffer = NULL;
    buffers->analysis_buffer = NULL;
    buffers->wide_buffer = NULL;
    buffers->buffer_sizes = NULL;
    buffers->is_in_use = NULL;
    buffers->buffer_names = NULL;
}

void reset_buffer_pool(WorkBuffers *buffers) {
    if (!buffers) {
        LOG_ERROR("NULL buffers passed to reset_buffer_pool");
        return;
    }
    
    for (int i = 0; i < buffers->pool_size; i++) {
        buffers->is_in_use[i] = 0;
    }
    
    LOG_DEBUG("Reset work buffers - all buffers now available");
}

DSVResult validate_buffer_ptr(WorkBuffers *buffers, const char* buffer) {
    if (!buffers || !buffer) {
        return DSV_ERROR_INVALID_ARGS;
    }
    
    int index = get_buffer_index(buffers, buffer);
    return (index >= 0 && index < buffers->pool_size) ? DSV_OK : DSV_ERROR_INVALID_ARGS;
} 