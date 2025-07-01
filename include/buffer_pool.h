#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "display_state.h"
#include "error_context.h"
#include "config.h"

// Buffer pool management functions

/**
 * @brief Initialize work buffers with proper metadata
 * @param buffers A pointer to the WorkBuffers.
 * @param config Configuration containing buffer parameters.
 * @return DSVResult indicating success or failure.
 */
DSVResult init_buffer_pool(WorkBuffers *buffers, const DSVConfig *config);

/**
 * @brief Frees all memory associated with work buffers.
 * @param buffers A pointer to the WorkBuffers.
 */
void cleanup_buffer_pool(WorkBuffers *buffers);

// Compatibility functions for test suite (simplified implementations)

/**
 * @brief Acquire a buffer for temporary use (compatibility function).
 * @param buffers A pointer to the WorkBuffers.
 * @param purpose Description of what the buffer will be used for (ignored in simplified version).
 * @return Pointer to an available buffer using round-robin selection.
 */
char* acquire_buffer(WorkBuffers *buffers, const char *purpose);

/**
 * @brief Release a buffer back to the pool (compatibility function).
 * @param buffers A pointer to the WorkBuffers.
 * @param buffer Pointer to the buffer to release (no-op in simplified version).
 */
void release_buffer(WorkBuffers *buffers, const char* buffer);

/**
 * @brief Reset all buffers to available state (compatibility function).
 * @param buffers A pointer to the WorkBuffers (no-op in simplified version).
 */
void reset_buffer_pool(WorkBuffers *buffers);

/**
 * @brief Validate that a buffer pointer is valid (compatibility function).
 * @param buffers A pointer to the WorkBuffers.
 * @param buffer The buffer pointer to validate.
 * @return DSV_OK if both pointers are non-NULL, DSV_ERROR_INVALID_ARGS otherwise.
 */
DSVResult validate_buffer_ptr(WorkBuffers *buffers, const char* buffer);

#endif // BUFFER_POOL_H 