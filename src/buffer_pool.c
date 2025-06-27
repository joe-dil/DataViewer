#include "viewer.h"
#include "buffer_pool.h"
#include <stdlib.h>
#include <stdio.h>

void init_buffer_pool(struct DSVViewer *viewer) {
    viewer->buffer_pool = malloc(sizeof(BufferPool));
    if (!viewer->buffer_pool) {
        perror("Failed to allocate buffer pool");
        exit(1);
    }
}

void cleanup_buffer_pool(struct DSVViewer *viewer) {
    if (viewer->buffer_pool) {
        free(viewer->buffer_pool);
    }
} 