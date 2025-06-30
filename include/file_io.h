#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>
#include "error_context.h"
#include "config.h"

// Forward declarations
struct DSVViewer;

// File I/O operations
DSVResult load_file_data(struct DSVViewer *viewer, const char *filename);
void cleanup_file_data(struct DSVViewer *viewer);
DSVResult scan_file_data(struct DSVViewer *viewer, const DSVConfig *config);
char detect_file_delimiter(const char *data, size_t length, char override_delimiter, const DSVConfig *config);

// Validation functions - FIX CRITICAL ISSUES HERE
int validate_file_bounds(struct DSVViewer *viewer);
int handle_empty_file(struct DSVViewer *viewer);
int handle_single_line_file(struct DSVViewer *viewer);

#endif // FILE_IO_H 