#include "viewer.h"
#include "file_handler.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

int load_file(struct DSVViewer *viewer, const char *filename) {
    struct stat st;
    
    viewer->fd = open(filename, O_RDONLY);
    if (viewer->fd == -1) {
        perror("Failed to open file");
        return -1;
    }
    
    if (fstat(viewer->fd, &st) == -1) {
        perror("Failed to get file stats");
        close(viewer->fd);
        return -1;
    }
    
    viewer->length = st.st_size;
    
    viewer->data = mmap(NULL, viewer->length, PROT_READ, MAP_PRIVATE, viewer->fd, 0);
    if (viewer->data == MAP_FAILED) {
        perror("Failed to map file");
        close(viewer->fd);
        return -1;
    }
    
    return 0;
}

void unload_file(struct DSVViewer *viewer) {
    if (viewer->data != MAP_FAILED) {
        munmap(viewer->data, viewer->length);
    }
    if (viewer->fd != -1) {
        close(viewer->fd);
    }
} 