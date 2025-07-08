#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "error_context.h"
#include "logging.h"

// Timing utilities
double get_time_ms(void);
void log_phase_timing(const char *phase_name, double duration);
void log_total_timing(const char *operation, double total_time);

// Memory utilities (for future use)
void* safe_malloc(size_t size, const char *context);
void* safe_realloc(void *ptr, size_t size, const char *context);

// Phase 3: Memory Safety Macros
#define SAFE_FREE(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0)
#define CHECK_NULL_RET(ptr, ret) do { if(!(ptr)) return (ret); } while(0)
#define CHECK_NULL_RET_VOID(ptr) do { if(!(ptr)) return; } while(0)
#define CHECK_ALLOC(ptr) do { if(!(ptr)) { LOG_ERROR("Allocation failed: %s:%d", __FILE__, __LINE__); return DSV_ERROR_MEMORY; } } while(0)

// Hashing
uint32_t fnv1a_hash(const char *str);
bool is_string_numeric(const char *s);

#endif // UTILS_H 