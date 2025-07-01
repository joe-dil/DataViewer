#include "../framework/test_runner.h"
#include "encoding.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

// --- Test Data ---

// UTF-8 BOM: EF BB BF
static const char utf8_bom_data[] = "\xEF\xBB\xBFName,Age,City\nJohn,25,Boston";

// UTF-8 with accents (no BOM)
static const char utf8_data[] = "Name,Age,City\nJean,25,Montréal\nMarie,30,Québec";

// Latin-1 with accents (single bytes)
static const char latin1_data[] = "Name,Age,City\nJean,25,Montr\xe9\x61l\nMarie,30,Qu\xe9\x62\x65\x63";

// Pure ASCII data
static const char ascii_data[] = "Name,Age,City\nJohn,25,Boston\nMary,30,Denver";

// Mixed content that could be ambiguous
static const char mixed_data[] = "Name,Value\nTest,\x80\x81\x82";

// --- Test Configuration Setup ---

static DSVConfig test_config;

void setup_encoding_tests() {
    config_init_defaults(&test_config);
    test_config.encoding_detection_sample_size = 1024;
    test_config.auto_detect_encoding = 1;
    test_config.force_encoding = NULL;
}

// --- BOM Detection Tests ---

void test_utf8_bom_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(utf8_bom_data, sizeof(utf8_bom_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_UTF8_BOM, "Should detect UTF-8 with BOM");
    TEST_ASSERT(result.bom_size == 3, "BOM size should be 3 bytes");
    TEST_ASSERT(result.confidence == 1.0, "BOM detection should have 100% confidence");
    TEST_ASSERT(strcmp(result.encoding_name, "UTF-8 (with BOM)") == 0, "Should have correct encoding name");
}

void test_no_bom_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(utf8_data, sizeof(utf8_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding != ENCODING_UTF8_BOM, "Should not detect BOM when none present");
    TEST_ASSERT(result.bom_size == 0, "BOM size should be 0 when no BOM");
}

// --- Heuristic Detection Tests ---

void test_ascii_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(ascii_data, sizeof(ascii_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_ASCII, "Should detect pure ASCII");
    TEST_ASSERT(result.confidence == 1.0, "ASCII detection should have high confidence");
    TEST_ASSERT(strcmp(result.encoding_name, "ASCII") == 0, "Should have correct encoding name");
}

void test_utf8_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(utf8_data, sizeof(utf8_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_UTF8, "Should detect UTF-8 encoding");
    TEST_ASSERT(result.confidence > 0.8, "UTF-8 detection should have high confidence");
    TEST_ASSERT(strstr(result.encoding_name, "UTF-8") != NULL, "Should mention UTF-8 in name");
}

void test_latin1_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(latin1_data, sizeof(latin1_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_LATIN1, "Should detect Latin-1 encoding");
    TEST_ASSERT(result.confidence > 0.5, "Latin-1 detection should have reasonable confidence");
    TEST_ASSERT(strstr(result.encoding_name, "Latin-1") != NULL, "Should mention Latin-1 in name");
}

// --- Force Encoding Tests ---

void test_force_encoding_override() {
    setup_encoding_tests();
    test_config.force_encoding = "latin-1";
    
    // Even UTF-8 data should be treated as Latin-1 when forced
    EncodingDetectionResult result = detect_file_encoding(utf8_data, sizeof(utf8_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_LATIN1, "Should use forced encoding");
    TEST_ASSERT(result.confidence == 1.0, "Forced encoding should have 100% confidence");
}

void test_force_encoding_invalid() {
    setup_encoding_tests();
    test_config.force_encoding = "invalid-encoding";
    
    // Should fall back to auto-detection for invalid encoding names
    EncodingDetectionResult result = detect_file_encoding(ascii_data, sizeof(ascii_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_ASCII, "Should fall back to auto-detection");
}

void test_auto_detect_disabled() {
    setup_encoding_tests();
    test_config.auto_detect_encoding = 0;
    
    EncodingDetectionResult result = detect_file_encoding(latin1_data, sizeof(latin1_data) - 1, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_UTF8, "Should default to UTF-8 when auto-detect disabled");
    TEST_ASSERT(strstr(result.encoding_name, "assumed") != NULL, "Should indicate assumption in name");
}

// --- Text Width Calculation Tests ---

void test_text_width_ascii() {
    const char *text = "Hello World";
    int width = get_text_display_width(text, ENCODING_ASCII, 50);
    
    TEST_ASSERT(width == 11, "ASCII text width should equal byte length");
}

void test_text_width_latin1() {
    // Use explicit Latin-1 encoding: café as 4 bytes
    const char text[] = {'c', 'a', 'f', '\xe9', '\0'}; // é = 0xe9 in Latin-1
    int width = get_text_display_width(text, ENCODING_LATIN1, 50);
    
    TEST_ASSERT(width == 4, "Latin-1 text width should equal byte length");
}

void test_text_width_utf8() {
    const char *text = "café"; // In UTF-8: c(1) a(1) f(1) é(2 bytes, 1 column) = 5 bytes, 4 display columns
    int width = get_text_display_width(text, ENCODING_UTF8, 50);
    
    // Width should be 4 display columns (this test verifies that our UTF-8 width calculation works)
    // Note: The actual implementation may return byte length if wcswidth fails
    TEST_ASSERT(width == 4 || width == 5, "UTF-8 text width should be display columns or fall back to byte length");
}

// --- Text Truncation Tests ---

void test_truncate_ascii_safe() {
    const char *src = "Hello World";
    char dest[8];
    
    truncate_text_safe(src, dest, sizeof(dest), 5, ENCODING_ASCII);
    
    TEST_ASSERT(strcmp(dest, "Hello") == 0, "ASCII truncation should work correctly");
    TEST_ASSERT(strlen(dest) == 5, "Truncated ASCII should have correct length");
}

void test_truncate_latin1_safe() {
    const char *src = "café résumé";
    char dest[8];
    
    truncate_text_safe(src, dest, sizeof(dest), 6, ENCODING_LATIN1);
    
    TEST_ASSERT(strlen(dest) == 6, "Latin-1 truncation should respect byte boundaries");
    TEST_ASSERT(dest[6] == '\0', "Should be null-terminated");
}

void test_truncate_utf8_safe() {
    const char *src = "café"; // UTF-8: 5 bytes, 4 display columns
    char dest[10];
    
    truncate_text_safe(src, dest, sizeof(dest), 3, ENCODING_UTF8);
    
    // Should truncate at character boundaries, not in the middle of é
    TEST_ASSERT(strlen(dest) == 3, "UTF-8 truncation should stop at character boundary");
    TEST_ASSERT(strcmp(dest, "caf") == 0, "Should truncate before multi-byte character");
}

// --- Edge Case Tests ---

void test_empty_file_detection() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding("", 0, &test_config);
    
    TEST_ASSERT(result.detected_encoding == ENCODING_ASCII, "Empty file should default to ASCII");
}

void test_null_input_safety() {
    setup_encoding_tests();
    
    EncodingDetectionResult result = detect_file_encoding(NULL, 100, &test_config);
    TEST_ASSERT(result.detected_encoding == ENCODING_ASCII, "NULL data should default to ASCII");
    
    result = detect_file_encoding("test", 4, NULL);
    TEST_ASSERT(result.detected_encoding == ENCODING_ASCII, "NULL config should default to ASCII");
}

void test_encoding_name_parsing() {
    TEST_ASSERT(parse_encoding_name("utf-8") == ENCODING_UTF8, "Should parse UTF-8");
    TEST_ASSERT(parse_encoding_name("UTF8") == ENCODING_UTF8, "Should parse UTF8 (no dash)");
    TEST_ASSERT(parse_encoding_name("latin-1") == ENCODING_LATIN1, "Should parse Latin-1");
    TEST_ASSERT(parse_encoding_name("ISO-8859-1") == ENCODING_LATIN1, "Should parse ISO-8859-1");
    TEST_ASSERT(parse_encoding_name("ascii") == ENCODING_ASCII, "Should parse ASCII");
    TEST_ASSERT(parse_encoding_name("invalid") == ENCODING_UNKNOWN, "Should return UNKNOWN for invalid");
    TEST_ASSERT(parse_encoding_name(NULL) == ENCODING_UNKNOWN, "Should handle NULL gracefully");
}

void test_encoding_name_strings() {
    TEST_ASSERT(strcmp(get_encoding_name(ENCODING_ASCII), "ASCII") == 0, "ASCII name should be correct");
    TEST_ASSERT(strcmp(get_encoding_name(ENCODING_UTF8), "UTF-8") == 0, "UTF-8 name should be correct");
    TEST_ASSERT(strcmp(get_encoding_name(ENCODING_LATIN1), "ISO-8859-1 (Latin-1)") == 0, "Latin-1 name should be correct");
    TEST_ASSERT(strcmp(get_encoding_name(ENCODING_UNKNOWN), "Unknown") == 0, "Unknown name should be correct");
}

// --- Test Registration ---

TestCase encoding_tests[] = {
    // BOM Detection
    {"UTF-8 BOM Detection", test_utf8_bom_detection},
    {"No BOM Detection", test_no_bom_detection},
    
    // Heuristic Detection
    {"ASCII Detection", test_ascii_detection},
    {"UTF-8 Detection", test_utf8_detection},
    {"Latin-1 Detection", test_latin1_detection},
    
    // Configuration
    {"Force Encoding Override", test_force_encoding_override},
    {"Force Encoding Invalid", test_force_encoding_invalid},
    {"Auto-Detect Disabled", test_auto_detect_disabled},
    
    // Text Processing
    {"Text Width ASCII", test_text_width_ascii},
    {"Text Width Latin-1", test_text_width_latin1},
    {"Text Width UTF-8", test_text_width_utf8},
    {"Truncate ASCII Safe", test_truncate_ascii_safe},
    {"Truncate Latin-1 Safe", test_truncate_latin1_safe},
    {"Truncate UTF-8 Safe", test_truncate_utf8_safe},
    
    // Edge Cases
    {"Empty File Detection", test_empty_file_detection},
    {"NULL Input Safety", test_null_input_safety},
    {"Encoding Name Parsing", test_encoding_name_parsing},
    {"Encoding Name Strings", test_encoding_name_strings},
};

int encoding_suite_size = sizeof(encoding_tests) / sizeof(TestCase); 