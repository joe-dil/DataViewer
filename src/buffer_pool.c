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

    // Initialize simplified metadata (no complex tracking needed)
    buffers->buffer_sizes = NULL;
    buffers->is_in_use = NULL;
    buffers->buffer_names = NULL;
    buffers->pool_size = 0;

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
    
    // Metadata was simplified away
    buffers->buffer_sizes = NULL;
    buffers->is_in_use = NULL;
    buffers->buffer_names = NULL;
    buffers->pool_size = 0;
    
    LOG_DEBUG("Cleaned up simplified buffer pool");
}

// Removed reset and validation functions - direct access makes them unnecessary 