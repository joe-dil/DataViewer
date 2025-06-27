#ifndef FIELD_DESC_H
#define FIELD_DESC_H

#include <stddef.h>

// Zero-copy field descriptor
typedef struct {
    const char *start;
    size_t length;
    int needs_unescaping;
} FieldDesc;

#endif // FIELD_DESC_H 