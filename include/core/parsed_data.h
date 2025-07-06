#ifndef PARSED_DATA_H
#define PARSED_DATA_H

#include <stddef.h>
#include "field_desc.h"

// A component to hold parsing related data.
typedef struct {
    char delimiter;
    int has_header;
    FieldDesc *header_fields;
    size_t num_header_fields;
    FieldDesc *fields;
    size_t num_fields;
    size_t *line_offsets;
    size_t num_lines;
    size_t capacity;
} ParsedData;

#endif // PARSED_DATA_H 