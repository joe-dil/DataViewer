#ifndef FIELD_DESC_H
#define FIELD_DESC_H

#include <stddef.h>

/**
 * Zero-copy field descriptor - points directly into file data.
 * Avoids string duplication by using pointers and lengths.
 */
typedef struct {
    const char *start;     // Pointer into original file data
    size_t length;         // Length of field in bytes
    int needs_unescaping;  // True if field contains escaped quotes ("")
} FieldDesc;

#endif // FIELD_DESC_H 