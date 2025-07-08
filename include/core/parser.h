#ifndef PARSER_H
#define PARSER_H

#include "core/field_desc.h"
#include <stddef.h>

/**
 * @brief Parses a single line of delimited text into a series of fields.
 *
 * This function handles quoted fields and escaped quotes according to standard
 * CSV-like rules. It is a zero-copy parser, meaning the `FieldDesc` structs
 * it produces point directly into the input `data` buffer.
 *
 * @param data The raw character buffer containing the line to parse.
 * @param length The total length of the data buffer.
 * @param delimiter The character used to separate fields.
 * @param offset The starting position within `data` to begin parsing.
 * @param fields An array of `FieldDesc` structs to be populated.
 * @param max_fields The maximum number of fields to parse.
 * @return The number of fields successfully parsed.
 */
size_t parse_line(const char *data, size_t length, char delimiter, size_t offset, FieldDesc *fields, size_t max_fields);

/**
 * @brief Renders a field descriptor into a null-terminated string.
 *
 * This function takes a `FieldDesc`, which points to a potentially non-null-terminated
 * segment of a larger buffer, and copies it into the provided `buffer`. It handles
 * un-quoting and un-escaping of the field content. Newlines are replaced with spaces.
 *
 * @param field The field descriptor to render.
 * @param buffer The destination buffer for the rendered string.
 * @param buffer_size The size of the destination buffer.
 */
void render_field(const FieldDesc *field, char *buffer, size_t buffer_size);

#endif // PARSER_H 