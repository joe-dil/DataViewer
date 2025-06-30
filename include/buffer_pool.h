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
 * @brief Acquire a buffer for temporary use.
 * @param buffers A pointer to the WorkBuffers.
 * @param purpose Description of what the buffer will be used for (for debugging).
 * @return Pointer to an available buffer, or NULL if none available.
 */
char* acquire_buffer(WorkBuffers *buffers, const char *purpose);

/**
 * @brief Release a buffer back to the available pool.
 * @param buffers A pointer to the WorkBuffers.
 * @param buffer Pointer to the buffer to release.
 */
void release_buffer(WorkBuffers *buffers, const char* buffer);

/**
 * @brief Frees all memory associated with work buffers.
 * @param buffers A pointer to the WorkBuffers.
 */
void cleanup_buffer_pool(WorkBuffers *buffers);

/**
 * @brief Reset all buffers to available state.
 * @param buffers A pointer to the WorkBuffers.
 */
void reset_buffer_pool(WorkBuffers *buffers);

/**
 * @brief Validate that a buffer pointer is valid.
 * @param buffers A pointer to the WorkBuffers.
 * @param buffer The buffer pointer to validate.
 * @return DSV_OK if valid, DSV_ERROR_INVALID_ARGS if invalid.
 */
DSVResult validate_buffer_ptr(WorkBuffers *buffers, const char* buffer);

#endif // BUFFER_POOL_H 