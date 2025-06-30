#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// --- Timing Utilities ---

double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void log_phase_timing(const char *phase_name, double duration) {
    printf("%s: %.2f ms\n", phase_name, duration);
}

void log_total_timing(const char *operation, double total_time) {
    printf("%s: %.2f ms\n", operation, total_time);
}

// --- Memory Utilities (Placeholders) ---

void* safe_malloc(size_t size, const char *context) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "ERROR: Failed to allocate %zu bytes for %s\n", size, context);
        // In a real application, this might exit or trigger a more robust error handler
    }
    return ptr;
}

void* safe_realloc(void *ptr, size_t size, const char *context) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "ERROR: Failed to reallocate %zu bytes for %s\n", size, context);
    }
    return new_ptr;
} 