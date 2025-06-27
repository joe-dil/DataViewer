#ifndef UTILS_H
#define UTILS_H

#include <wchar.h>

#define UTILS_MAX_FIELD_LEN 1024

// Reusable buffers to avoid repeated stack allocation in hot loops
typedef struct BufferPool {
    char buffer1[UTILS_MAX_FIELD_LEN];
    char buffer2[UTILS_MAX_FIELD_LEN];
    char buffer3[UTILS_MAX_FIELD_LEN];
    wchar_t wide_buffer[UTILS_MAX_FIELD_LEN];
} BufferPool;

// Forward declare CSVViewer to avoid circular dependencies.
// The full definition is only needed in the .c file.
struct CSVViewer; 

void init_buffer_pool(struct CSVViewer *viewer);
void cleanup_buffer_pool(struct CSVViewer *viewer);

#endif // UTILS_H 