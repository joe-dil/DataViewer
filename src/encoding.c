#include "encoding.h"
#include "logging.h"
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h>

// --- BOM Detection ---

static EncodingDetectionResult detect_bom(const char *data, size_t length) {
    EncodingDetectionResult result = {ENCODING_UNKNOWN, 0.0, 0, "Unknown"};
    
    if (length >= 3 && (unsigned char)data[0] == 0xEF && 
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        result.detected_encoding = ENCODING_UTF8_BOM;
        result.confidence = 1.0;
        result.bom_size = 3;
        result.encoding_name = "UTF-8 (with BOM)";
        return result;
    }
    
    return result;
}

// --- Heuristic Detection ---

static int is_valid_utf8_sequence(const unsigned char *data, size_t pos, size_t length) {
    if (pos >= length) return 0;
    
    unsigned char byte = data[pos];
    
    // ASCII (0-127)
    if (byte < 0x80) return 1;
    
    // Multi-byte UTF-8 sequences
    int expected_bytes = 0;
    if ((byte & 0xE0) == 0xC0) expected_bytes = 2;
    else if ((byte & 0xF0) == 0xE0) expected_bytes = 3;
    else if ((byte & 0xF8) == 0xF0) expected_bytes = 4;
    else return 0; // Invalid start byte
    
    // Check continuation bytes
    for (int i = 1; i < expected_bytes; i++) {
        if (pos + i >= length) return 0;
        if ((data[pos + i] & 0xC0) != 0x80) return 0;
    }
    
    return expected_bytes;
}

static double calculate_utf8_confidence(const char *data, size_t length, size_t sample_size) {
    if (sample_size > length) sample_size = length;
    
    size_t valid_sequences = 0;
    size_t total_multibyte = 0;
    size_t pos = 0;
    
    while (pos < sample_size) {
        unsigned char byte = data[pos];
        
        if (byte < 0x80) {
            // ASCII character
            pos++;
        } else {
            // Potential multi-byte sequence
            total_multibyte++;
            int seq_len = is_valid_utf8_sequence((const unsigned char*)data, pos, length);
            if (seq_len > 0) {
                valid_sequences++;
                pos += seq_len;
            } else {
                pos++;
            }
        }
    }
    
    if (total_multibyte == 0) return 0.0; // Pure ASCII, not UTF-8
    return (double)valid_sequences / total_multibyte;
}

static double calculate_latin1_confidence(const char *data, size_t length, size_t sample_size) {
    if (sample_size > length) sample_size = length;
    
    size_t high_bytes = 0;
    size_t printable_high = 0;
    
    for (size_t i = 0; i < sample_size; i++) {
        unsigned char byte = data[i];
        if (byte >= 0x80) {
            high_bytes++;
            // Common Latin-1 printable characters (accented letters, symbols)
            if ((byte >= 0xA0 && byte <= 0xFF) || 
                (byte >= 0x80 && byte <= 0x9F && 
                 (byte == 0x80 || byte == 0x82 || byte == 0x83 || byte == 0x84 || 
                  byte == 0x85 || byte == 0x86 || byte == 0x87 || byte == 0x89 ||
                  byte == 0x8A || byte == 0x8B || byte == 0x8C || byte == 0x8E ||
                  byte == 0x91 || byte == 0x92 || byte == 0x93 || byte == 0x94 ||
                  byte == 0x95 || byte == 0x96 || byte == 0x97 || byte == 0x99 ||
                  byte == 0x9A || byte == 0x9B || byte == 0x9C || byte == 0x9E ||
                  byte == 0x9F))) {
                printable_high++;
            }
        }
    }
    
    if (high_bytes == 0) return 0.0; // Pure ASCII
    return (double)printable_high / high_bytes;
}

static EncodingDetectionResult detect_heuristic(const char *data, size_t length, const DSVConfig *config) {
    EncodingDetectionResult result = {ENCODING_ASCII, 1.0, 0, "ASCII"};
    
    size_t sample_size = (size_t)config->encoding_detection_sample_size;
    if (sample_size > length) sample_size = length;
    
    // Check if pure ASCII first
    int has_high_bytes = 0;
    for (size_t i = 0; i < sample_size; i++) {
        if ((unsigned char)data[i] >= 0x80) {
            has_high_bytes = 1;
            break;
        }
    }
    
    if (!has_high_bytes) {
        return result; // Pure ASCII
    }
    
    // Calculate confidence for UTF-8 and Latin-1
    double utf8_conf = calculate_utf8_confidence(data, length, sample_size);
    double latin1_conf = calculate_latin1_confidence(data, length, sample_size);
    
    LOG_DEBUG("Encoding detection: UTF-8 confidence %.2f, Latin-1 confidence %.2f", 
              utf8_conf, latin1_conf);
    
    if (utf8_conf > 0.8) {
        result.detected_encoding = ENCODING_UTF8;
        result.encoding_name = "UTF-8";
        result.confidence = utf8_conf;
    } else if (latin1_conf > 0.7) {
        result.detected_encoding = ENCODING_LATIN1;
        result.encoding_name = "ISO-8859-1 (Latin-1)";
        result.confidence = latin1_conf;
    } else {
        // Default to Latin-1 for unknown high-byte content
        result.detected_encoding = ENCODING_LATIN1;
        result.encoding_name = "ISO-8859-1 (Latin-1, assumed)";
        result.confidence = 0.5;
    }
    
    return result;
}

// --- Public Functions ---

EncodingDetectionResult detect_file_encoding(const char *data, size_t length, const DSVConfig *config) {
    if (!data || length == 0 || !config) {
        EncodingDetectionResult result = {ENCODING_ASCII, 1.0, 0, "ASCII"};
        return result;
    }
    
    // Check if user forced an encoding
    if (config->force_encoding) {
        FileEncoding forced = parse_encoding_name(config->force_encoding);
        if (forced != ENCODING_UNKNOWN) {
            EncodingDetectionResult result = {forced, 1.0, 0, get_encoding_name(forced)};
            LOG_DEBUG("Using forced encoding: %s", result.encoding_name);
            return result;
        }
    }
    
    if (!config->auto_detect_encoding) {
        EncodingDetectionResult result = {ENCODING_UTF8, 1.0, 0, "UTF-8 (assumed)"};
        return result;
    }
    
    // Try BOM detection first
    EncodingDetectionResult bom_result = detect_bom(data, length);
    if (bom_result.detected_encoding != ENCODING_UNKNOWN) {
        LOG_DEBUG("Detected encoding via BOM: %s", bom_result.encoding_name);
        return bom_result;
    }
    
    // Fall back to heuristic detection
    EncodingDetectionResult heuristic_result = detect_heuristic(data, length, config);
    LOG_DEBUG("Detected encoding via heuristics: %s (confidence: %.2f)", 
              heuristic_result.encoding_name, heuristic_result.confidence);
    
    return heuristic_result;
}

int get_text_display_width(const char *text, FileEncoding encoding, int max_chars) {
    if (!text) return 0;
    
    switch (encoding) {
        case ENCODING_ASCII:
        case ENCODING_LATIN1:
        case ENCODING_WINDOWS_1252:
            // For Latin-1, each byte is one character, one display column
            return strnlen(text, max_chars);
            
        case ENCODING_UTF8:
        case ENCODING_UTF8_BOM: {
            // Use existing Unicode width calculation
            wchar_t wcs[max_chars + 1];
            size_t converted = mbstowcs(wcs, text, max_chars);
            if (converted == (size_t)-1) {
                // Conversion failed, fall back to byte count
                return strnlen(text, max_chars);
            }
            int width = wcswidth(wcs, converted);
            return (width < 0) ? converted : width;
        }
        
        default:
            return strnlen(text, max_chars);
    }
}

void truncate_text_safe(const char *src, char *dest, size_t dest_size, int display_width, FileEncoding encoding) {
    if (!src || !dest || dest_size == 0) {
        if (dest && dest_size > 0) dest[0] = '\0';
        return;
    }
    
    switch (encoding) {
        case ENCODING_ASCII:
        case ENCODING_LATIN1:
        case ENCODING_WINDOWS_1252: {
            // Simple byte-based truncation for Latin-1
            size_t copy_len = (size_t)display_width < dest_size - 1 ? (size_t)display_width : dest_size - 1;
            size_t src_len = strlen(src);
            if (copy_len > src_len) copy_len = src_len;
            
            strncpy(dest, src, copy_len);
            dest[copy_len] = '\0';
            break;
        }
        
        case ENCODING_UTF8:
        case ENCODING_UTF8_BOM: {
            // Unicode-aware truncation
            wchar_t wcs[display_width + 1];
            mbstowcs(wcs, src, display_width);
            
            int current_width = 0;
            size_t i;
            for (i = 0; wcs[i] != L'\0'; i++) {
                int char_width = wcwidth(wcs[i]);
                if (char_width < 0) char_width = 1; // Control chars
                if (current_width + char_width > display_width) break;
                current_width += char_width;
            }
            wcs[i] = L'\0';
            
            wcstombs(dest, wcs, dest_size - 1);
            dest[dest_size - 1] = '\0';
            break;
        }
        
        default:
            strncpy(dest, src, dest_size - 1);
            dest[dest_size - 1] = '\0';
            break;
    }
}

const char* get_encoding_name(FileEncoding encoding) {
    switch (encoding) {
        case ENCODING_ASCII: return "ASCII";
        case ENCODING_UTF8: return "UTF-8";
        case ENCODING_UTF8_BOM: return "UTF-8 (with BOM)";
        case ENCODING_LATIN1: return "ISO-8859-1 (Latin-1)";
        case ENCODING_WINDOWS_1252: return "Windows-1252";
        default: return "Unknown";
    }
}

FileEncoding parse_encoding_name(const char *name) {
    if (!name) return ENCODING_UNKNOWN;
    
    // Convert to lowercase for comparison
    char lower_name[64];
    strncpy(lower_name, name, sizeof(lower_name) - 1);
    lower_name[sizeof(lower_name) - 1] = '\0';
    
    for (char *p = lower_name; *p; p++) {
        *p = tolower(*p);
    }
    
    if (strcmp(lower_name, "ascii") == 0) return ENCODING_ASCII;
    if (strcmp(lower_name, "utf-8") == 0 || strcmp(lower_name, "utf8") == 0) return ENCODING_UTF8;
    if (strcmp(lower_name, "utf-8-bom") == 0) return ENCODING_UTF8_BOM;
    if (strcmp(lower_name, "latin-1") == 0 || strcmp(lower_name, "latin1") == 0 ||
        strcmp(lower_name, "iso-8859-1") == 0) return ENCODING_LATIN1;
    if (strcmp(lower_name, "windows-1252") == 0 || strcmp(lower_name, "cp1252") == 0) return ENCODING_WINDOWS_1252;
    
    return ENCODING_UNKNOWN;
} 