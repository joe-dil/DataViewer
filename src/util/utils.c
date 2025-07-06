#include "utils.h"
#include "logging.h"
#include "memory/constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

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
        LOG_ERROR("Failed to allocate %zu bytes for %s", size, context);
    }
    return ptr;
}

void* safe_realloc(void *ptr, size_t size, const char *context) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        LOG_ERROR("Failed to reallocate %zu bytes for %s", size, context);
    }
    return new_ptr;
}

// --- Hashing ---
// FNV-1a hash function - fast and well-distributed for string keys
uint32_t fnv1a_hash(const char *str) {
    CHECK_NULL_RET(str, 0);
    uint32_t hash = FNV_OFFSET_BASIS;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
} 