#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

// Timing utilities
double get_time_ms(void);
void log_phase_timing(const char *phase_name, double duration);
void log_total_timing(const char *operation, double total_time);

// Memory utilities (for future use)
void* safe_malloc(size_t size, const char *context);
void* safe_realloc(void *ptr, size_t size, const char *context);

#endif // UTILS_H 