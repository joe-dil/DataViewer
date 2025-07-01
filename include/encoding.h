#ifndef ENCODING_H
#define ENCODING_H

#include "error_context.h"
#include "config.h"
#include <stddef.h>

// --- Encoding Types ---

typedef enum {
    ENCODING_UNKNOWN = 0,
    ENCODING_ASCII,        // Pure ASCII (0-127)
    ENCODING_UTF8,         // UTF-8 without BOM
    ENCODING_UTF8_BOM,     // UTF-8 with BOM
    ENCODING_LATIN1,       // ISO-8859-1 (extends ASCII to 0-255)
    ENCODING_WINDOWS_1252, // Windows variant of Latin-1
    ENCODING_COUNT
} FileEncoding;

typedef struct {
    FileEncoding detected_encoding;
    double confidence;      // 0.0 to 1.0
    size_t bom_size;       // Size of BOM if present (0 for none)
    const char *encoding_name;
} EncodingDetectionResult;

// --- Public Function Declarations ---

/**
 * @brief Detects the encoding of a file's initial data.
 * 
 * @param data Pointer to the file data buffer.
 * @param length Length of the data buffer.
 * @param config Configuration containing detection parameters.
 * @return EncodingDetectionResult with detected encoding info.
 */
EncodingDetectionResult detect_file_encoding(const char *data, size_t length, const DSVConfig *config);

/**
 * @brief Calculate display width of text accounting for encoding.
 * 
 * @param text The text string to measure.
 * @param encoding The encoding of the text.
 * @param max_chars Maximum characters to process.
 * @return Display width in terminal columns.
 */
int get_text_display_width(const char *text, FileEncoding encoding, int max_chars);

/**
 * @brief Safely truncate text at character boundaries for given encoding.
 * 
 * @param src Source text string.
 * @param dest Destination buffer.
 * @param dest_size Size of destination buffer.
 * @param display_width Target display width.
 * @param encoding Encoding of the source text.
 */
void truncate_text_safe(const char *src, char *dest, size_t dest_size, int display_width, FileEncoding encoding);

/**
 * @brief Get the name string for an encoding type.
 * 
 * @param encoding The encoding type.
 * @return Human-readable name of the encoding.
 */
const char* get_encoding_name(FileEncoding encoding);

/**
 * @brief Parse encoding name string to FileEncoding enum.
 * 
 * @param name The encoding name string (case-insensitive).
 * @return Corresponding FileEncoding, or ENCODING_UNKNOWN if not recognized.
 */
FileEncoding parse_encoding_name(const char *name);

#endif // ENCODING_H 