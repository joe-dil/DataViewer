#include "buffer_pool.h"
#include "display_state.h"
#include "logging.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

// Simplified buffer pool - no complex tracking needed

DSVResult init_buffer_pool(WorkBuffers *buffers, const DSVConfig *config) {
    if (!buffers || !config) return DSV_ERROR_INVALID_ARGS;

    size_t field_len = config->max_field_len;

    // Allocate main buffers for direct access
    buffers->render_buffer = calloc(field_len, sizeof(char));
    buffers->pad_buffer = calloc(field_len, sizeof(char));
    buffers->cache_buffer = calloc(field_len, sizeof(char));
    buffers->temp_buffer = calloc(field_len, sizeof(char));
    buffers->analysis_buffer = calloc(field_len, sizeof(char));
    buffers->wide_buffer = calloc(field_len, sizeof(wchar_t));

    // Check for allocation failures
    if (!buffers->render_buffer || !buffers->pad_buffer || !buffers->cache_buffer || 
        !buffers->temp_buffer || !buffers->analysis_buffer || !buffers->wide_buffer) {
        cleanup_buffer_pool(buffers);
        return DSV_ERROR_MEMORY;
    }

    // Initialize simplified metadata (minimal tracking for test compatibility)
    buffers->buffer_sizes = calloc(5, sizeof(int));
    buffers->is_in_use = calloc(5, sizeof(int));
    buffers->buffer_names = NULL;
    buffers->pool_size = 5; // Fixed size: render, pad, cache, temp, analysis

    // Check allocation for tracking arrays
    if (!buffers->buffer_sizes || !buffers->is_in_use) {
        cleanup_buffer_pool(buffers);
        return DSV_ERROR_MEMORY;
    }

    LOG_DEBUG("Initialized simplified buffer pool with %zu byte buffers", field_len);
    return DSV_OK;
}

// Removed acquire/release mechanism - use direct buffer access instead

void cleanup_buffer_pool(WorkBuffers *buffers) {
    if (!buffers) return;
    
    free(buffers->render_buffer);
    free(buffers->pad_buffer);
    free(buffers->cache_buffer);
    free(buffers->temp_buffer);
    free(buffers->analysis_buffer);
    free(buffers->wide_buffer);
    
    // Clear pointers to prevent double free
    buffers->render_buffer = NULL;
    buffers->pad_buffer = NULL;
    buffers->cache_buffer = NULL;
    buffers->temp_buffer = NULL;
    buffers->analysis_buffer = NULL;
    buffers->wide_buffer = NULL;
    
    // Free minimal tracking arrays
    free(buffers->buffer_sizes);
    free(buffers->is_in_use);
    buffers->buffer_sizes = NULL;
    buffers->is_in_use = NULL;
    buffers->buffer_names = NULL;
    buffers->pool_size = 0;
    
    LOG_DEBUG("Cleaned up simplified buffer pool");
}

// Compatibility functions for test suite - simplified implementations
// These maintain API compatibility while preserving the performance benefits of direct access

char* acquire_buffer(WorkBuffers *buffers, const char *purpose) {
    if (!buffers || !buffers->is_in_use) {
        LOG_ERROR("NULL buffers passed to acquire_buffer");
        return NULL;
    }
    
    const char* buffer_name = purpose ? purpose : "unknown";
    
    // Find first available buffer (simple linear search)
    for (int i = 0; i < buffers->pool_size; i++) {
        if (!buffers->is_in_use[i]) {
            buffers->is_in_use[i] = 1;
            
            switch (i) {
                case 0:
                    LOG_DEBUG("Acquired render_buffer for %s", buffer_name);
                    return buffers->render_buffer;
                case 1:
                    LOG_DEBUG("Acquired pad_buffer for %s", buffer_name);
                    return buffers->pad_buffer;
                case 2:
                    LOG_DEBUG("Acquired cache_buffer for %s", buffer_name);
                    return buffers->cache_buffer;
                case 3:
                    LOG_DEBUG("Acquired temp_buffer for %s", buffer_name);
                    return buffers->temp_buffer;
                case 4:
                    LOG_DEBUG("Acquired analysis_buffer for %s", buffer_name);
                    return buffers->analysis_buffer;
                default:
                    return NULL; // Should not happen
            }
        }
    }
    
    LOG_WARN("No buffers available for %s", buffer_name);
    return NULL;
}

void release_buffer(WorkBuffers *buffers, const char* buffer) {
    if (!buffers || !buffer || !buffers->is_in_use) {
        LOG_ERROR("NULL buffers or buffer passed to release_buffer");
        return;
    }
    
    // Mark buffer as available based on which buffer it is
    if (buffer == buffers->render_buffer) {
        buffers->is_in_use[0] = 0;
        LOG_DEBUG("Released render_buffer");
    } else if (buffer == buffers->pad_buffer) {
        buffers->is_in_use[1] = 0;
        LOG_DEBUG("Released pad_buffer");
    } else if (buffer == buffers->cache_buffer) {
        buffers->is_in_use[2] = 0;
        LOG_DEBUG("Released cache_buffer");
    } else if (buffer == buffers->temp_buffer) {
        buffers->is_in_use[3] = 0;
        LOG_DEBUG("Released temp_buffer");
    } else if (buffer == buffers->analysis_buffer) {
        buffers->is_in_use[4] = 0;
        LOG_DEBUG("Released analysis_buffer");
    } else {
        LOG_WARN("release_buffer called with unrecognized buffer pointer");
    }
}

void reset_buffer_pool(WorkBuffers *buffers) {
    if (!buffers || !buffers->is_in_use) {
        LOG_ERROR("NULL buffers passed to reset_buffer_pool");
        return;
    }
    
    // Reset usage tracking - mark all buffers as available
    for (int i = 0; i < buffers->pool_size; i++) {
        buffers->is_in_use[i] = 0;
    }
    
    LOG_DEBUG("Reset buffer pool - all buffers now available");
}

DSVResult validate_buffer_ptr(WorkBuffers *buffers, const char* buffer) {
    if (!buffers || !buffer) {
        return DSV_ERROR_INVALID_ARGS;
    }
    
    // Check if it's one of our allocated buffers
    if (buffer == buffers->render_buffer ||
        buffer == buffers->pad_buffer ||
        buffer == buffers->cache_buffer ||
        buffer == buffers->temp_buffer ||
        buffer == buffers->analysis_buffer) {
        return DSV_OK;
    }
    
    return DSV_ERROR_INVALID_ARGS;
}

// Note: Main codebase uses direct access (e.g., buffers.render_buffer) for optimal performance 