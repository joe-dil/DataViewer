#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <wchar.h>

#define UTILS_MAX_FIELD_LEN 1024

// Reusable buffers to avoid repeated stack allocation in hot loops
typedef struct BufferPool {
    char buffer_one[UTILS_MAX_FIELD_LEN];
    char buffer_two[UTILS_MAX_FIELD_LEN];
    char buffer_three[UTILS_MAX_FIELD_LEN];
    wchar_t wide_buffer[UTILS_MAX_FIELD_LEN];
} BufferPool;

// Forward declare DSVViewer to avoid circular dependencies.
// The full definition is only needed in the .c file.
struct DSVViewer; 

void init_buffer_pool(struct DSVViewer *viewer);
void cleanup_buffer_pool(struct DSVViewer *viewer);

#endif // BUFFER_POOL_H 