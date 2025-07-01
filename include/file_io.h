#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>
#include "error_context.h"
#include "config.h"

// Forward declarations
struct DSVViewer;

// File I/O operations

/**
 * @brief Load file using memory mapping for efficient access.
 * @param viewer Viewer instance to populate with file data
 * @param filename Path to file to load
 * @return DSV_OK on success, DSV_ERROR_FILE_IO on file errors
 */
DSVResult load_file_data(struct DSVViewer *viewer, const char *filename);

/**
 * @brief Clean up memory-mapped file resources.
 * @param viewer Viewer instance (safe to call with NULL)
 */
void cleanup_file_data(struct DSVViewer *viewer);

/**
 * @brief Scan file to build line offset index for navigation.
 * @param viewer Viewer instance with loaded file data
 * @param config Configuration for parsing limits
 * @return DSV_OK on success, error code on parsing failures
 */
DSVResult scan_file_data(struct DSVViewer *viewer, const DSVConfig *config);

/**
 * @brief Auto-detect CSV delimiter from file content.
 * @param data File content to analyze
 * @param length Number of bytes to scan
 * @param override_delimiter User-specified delimiter (0 for auto-detect)
 * @param config Configuration for detection parameters
 * @return Detected delimiter character (comma, tab, pipe, or semicolon)
 */
char detect_file_delimiter(const char *data, size_t length, char override_delimiter, const DSVConfig *config);

// Note: Internal validation functions are now static in file_io.c

#endif // FILE_IO_H 