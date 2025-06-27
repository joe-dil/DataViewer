#include "hash_utils.h"

// FNV-1a hash function for cache lookups
uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 0x811c9dc5;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
} 