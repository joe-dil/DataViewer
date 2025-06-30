#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "display_state.h"
#include "error_context.h"

// Buffer pool management functions

/**
 * Initialize a ManagedBufferPool with proper metadata
 * @param pool A pointer to the ManagedBufferPool.
 */
void init_buffer_pool(ManagedBufferPool *pool);

/**
 * Acquire a buffer from the pool for a specific purpose
 * @param pool A pointer to the ManagedBufferPool.
 * @param purpose A string describing the intended use.
 * @return A pointer to the buffer, or NULL if acquisition fails.
 */
char* acquire_buffer(ManagedBufferPool *pool, const char* purpose);

/**
 * Release a buffer back to the pool
 * @param pool A pointer to the ManagedBufferPool.
 * @param buffer A pointer to the buffer to release.
 */
void release_buffer(ManagedBufferPool *pool, char* buffer);

/**
 * Reset all buffers in the pool to available state
 * @param pool A pointer to the ManagedBufferPool.
 */
void reset_buffer_pool(ManagedBufferPool *pool);

/**
 * Validate that a buffer pointer is valid within the pool
 * @param pool A pointer to the ManagedBufferPool.
 * @param buffer A pointer to validate.
 * @return DSV_SUCCESS if valid, DSV_ERROR_INVALID_PARAMETER if invalid.
 */
DSVResult validate_buffer_ptr(ManagedBufferPool *pool, const char* buffer);

#endif // BUFFER_POOL_H 