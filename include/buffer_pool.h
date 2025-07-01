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

#endif // BUFFER_POOL_H 