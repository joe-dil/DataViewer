#ifndef PARSER_H
#define PARSER_H

#include "core/field_desc.h"
#include <stddef.h>

size_t parse_line(const char *data, size_t length, char delimiter, size_t offset, FieldDesc *fields, size_t max_fields);
void render_field(const FieldDesc *field, char *buffer, size_t buffer_size);

#endif // PARSER_H 