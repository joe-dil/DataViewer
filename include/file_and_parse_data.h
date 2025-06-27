#ifndef FILE_AND_PARSE_DATA_H
#define FILE_AND_PARSE_DATA_H

#include <stddef.h>
#include "field_desc.h"

// A component to hold file and parsing related data.
typedef struct {
    char *data;
    size_t length;
    int fd;
    char delimiter;
    FieldDesc *fields;
    size_t num_fields;
    size_t *line_offsets;
    size_t num_lines;
    size_t capacity;
} FileAndParseData;

#endif // FILE_AND_PARSE_DATA_H 