#include "../framework/test_runner.h"
#include "viewer.h"
#include "config.h"
#include "logging.h"
#include "error_context.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Test data setup
static void create_test_config_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static void create_test_csv_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

// =============================================================================
// CONFIGURATION LOADING TESTS
// =============================================================================

void test_config_default_initialization() {
    DSVConfig config;
    config_init_defaults(&config);
    
    // Verify all default values are reasonable
    ASSERT_GT(config.max_field_len, 0);
    ASSERT_GT(config.max_cols, 0);
    ASSERT_GT(config.buffer_pool_size, 0);
    ASSERT_GT(config.cache_size, 0);
    ASSERT_EQ(config_validate(&config), DSV_OK);
    
    printf("✓ Default config initialization passed\n");
}

void test_config_file_loading_valid() {
    const char* test_config = 
        "max_field_len=2048\n"
        "max_cols=512\n"
        "buffer_pool_size=10\n"
        "cache_size=8192\n";
    
    create_test_config_file("test_valid.conf", test_config);
    
    DSVConfig config;
    config_init_defaults(&config);
    
    DSVResult result = config_load_from_file(&config, "test_valid.conf");
    ASSERT_EQ(result, DSV_OK);
    ASSERT_EQ(config.max_field_len, 2048);
    ASSERT_EQ(config.max_cols, 512);
    ASSERT_EQ(config.buffer_pool_size, 10);
    ASSERT_EQ(config.cache_size, 8192);
    ASSERT_EQ(config_validate(&config), DSV_OK);
    
    unlink("test_valid.conf");
    printf("✓ Valid config file loading passed\n");
}

void test_config_file_loading_invalid() {
    const char* invalid_config = 
        "max_field_len=-100\n"  // Invalid negative value
        "invalid_key=value\n"   // Unknown key
        "malformed line\n";     // No = sign
    
    create_test_config_file("test_invalid.conf", invalid_config);
    
    DSVConfig config;
    config_init_defaults(&config);
    
    // Should load successfully but validation should fail due to negative value
    DSVResult load_result = config_load_from_file(&config, "test_invalid.conf");
    ASSERT_EQ(load_result, DSV_OK); // Loading succeeds
    
    DSVResult validate_result = config_validate(&config);
    ASSERT_NE(validate_result, DSV_OK); // But validation fails
    
    unlink("test_invalid.conf");
    printf("✓ Invalid config handling passed\n");
}

void test_config_nonexistent_file() {
    DSVConfig config;
    config_init_defaults(&config);
    
    DSVResult result = config_load_from_file(&config, "nonexistent.conf");
    ASSERT_EQ(result, DSV_ERROR_FILE_IO);
    
    // Config should remain in default state
    ASSERT_EQ(config_validate(&config), DSV_OK);
    
    printf("✓ Nonexistent config file handling passed\n");
}

// =============================================================================
// ERROR HANDLING PATH TESTS  
// =============================================================================

void test_viewer_init_with_invalid_file() {
    DSVConfig config;
    config_init_defaults(&config);
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, "nonexistent_file.csv", 0, &config);
    
    ASSERT_NE(result, DSV_OK);
    ASSERT_EQ(result, DSV_ERROR_FILE_IO);
    
    // Cleanup should be safe even on failed init
    cleanup_viewer(&viewer);
    
    printf("✓ Invalid file error handling passed\n");
}

void test_viewer_init_with_empty_file() {
    create_test_csv_file("empty.csv", "");
    
    DSVConfig config;
    config_init_defaults(&config);
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, "empty.csv", 0, &config);
    
    ASSERT_EQ(result, DSV_OK); // Empty files should be handled gracefully
    ASSERT_EQ(viewer.file_data->num_lines, 1); // Should have at least one line
    
    cleanup_viewer(&viewer);
    unlink("empty.csv");
    
    printf("✓ Empty file handling passed\n");
}

void test_error_result_strings() {
    // Test all error codes have valid string representations
    const char* error_str;
    
    error_str = dsv_result_to_string(DSV_OK);
    ASSERT_NOT_NULL(error_str);
    ASSERT_GT(strlen(error_str), 0);
    
    error_str = dsv_result_to_string(DSV_ERROR_MEMORY);
    ASSERT_NOT_NULL(error_str);
    ASSERT_GT(strlen(error_str), 0);
    
    error_str = dsv_result_to_string(DSV_ERROR_FILE_IO);
    ASSERT_NOT_NULL(error_str);
    ASSERT_GT(strlen(error_str), 0);
    
    error_str = dsv_result_to_string((DSVResult)999); // Invalid code
    ASSERT_NOT_NULL(error_str);
    ASSERT_GT(strlen(error_str), 0);
    
    printf("✓ Error result strings passed\n");
}

// =============================================================================
// MEMORY MANAGEMENT STRESS TESTS
// =============================================================================

void test_memory_multiple_init_cleanup_cycles() {
    DSVConfig config;
    config_init_defaults(&config);
    
    const char* test_data = "col1,col2,col3\ndata1,data2,data3\ntest1,test2,test3\n";
    create_test_csv_file("memory_test.csv", test_data);
    
    // Perform multiple init/cleanup cycles to check for memory leaks
    for (int i = 0; i < 10; i++) {
        DSVViewer viewer = {0};
        DSVResult result = init_viewer(&viewer, "memory_test.csv", 0, &config);
        ASSERT_EQ(result, DSV_OK);
        
        // Verify basic functionality
        ASSERT_NOT_NULL(viewer.file_data);
        ASSERT_GT(viewer.file_data->num_lines, 0);
        ASSERT_NOT_NULL(viewer.display_state);
        
        cleanup_viewer(&viewer);
    }
    
    unlink("memory_test.csv");
    printf("✓ Multiple init/cleanup cycles passed\n");
}

void test_memory_large_field_handling() {
    DSVConfig config;
    config_init_defaults(&config);
    
    // Create a CSV with a very large field (but within limits)
    char large_field[1000];
    memset(large_field, 'A', 999);
    large_field[999] = '\0';
    
    char test_data[2048];
    snprintf(test_data, sizeof(test_data), 
             "short,\"%s\",another\ndata1,data2,data3\n", large_field);
    
    create_test_csv_file("large_field_test.csv", test_data);
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, "large_field_test.csv", 0, &config);
    ASSERT_EQ(result, DSV_OK);
    
    // Parse the first line to test large field handling
    size_t num_fields = parse_line(&viewer, viewer.file_data->line_offsets[0], 
                                  viewer.file_data->fields, config.max_cols);
    ASSERT_EQ(num_fields, 3);
    
    // Test field rendering with large field
    char render_buffer[2048];
    render_field(&viewer.file_data->fields[1], render_buffer, sizeof(render_buffer));
    ASSERT_GT(strlen(render_buffer), 500); // Should contain most of the large field
    
    cleanup_viewer(&viewer);
    unlink("large_field_test.csv");
    
    printf("✓ Large field memory handling passed\n");
}

// =============================================================================
// PERFORMANCE BASELINE TESTS
// =============================================================================

void test_performance_file_loading() {
    // Create a moderately sized test file
    const int num_rows = 1000;
    const char* row_template = "data%d,value%d,item%d,field%d\n";
    
    FILE* f = fopen("performance_test.csv", "w");
    ASSERT_NOT_NULL(f);
    
    fprintf(f, "col1,col2,col3,col4\n"); // Header
    for (int i = 0; i < num_rows; i++) {
        fprintf(f, row_template, i, i*2, i*3, i*4);
    }
    fclose(f);
    
    DSVConfig config;
    config_init_defaults(&config);
    
    double start_time = get_time_ms();
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, "performance_test.csv", 0, &config);
    
    double end_time = get_time_ms();
    double duration = end_time - start_time;
    
    ASSERT_EQ(result, DSV_OK);
    ASSERT_EQ(viewer.file_data->num_lines, num_rows + 1); // +1 for header
    
    printf("✓ Performance: Loaded %d rows in %.2f ms (%.1f rows/ms)\n", 
           num_rows, duration, num_rows / duration);
    
    // Performance regression check: should process at least 50 rows/ms
    ASSERT_GT(num_rows / duration, 50.0); // Reasonable baseline
    
    cleanup_viewer(&viewer);
    unlink("performance_test.csv");
}

void test_performance_parsing_benchmark() {
    DSVConfig config;
    config_init_defaults(&config);
    
    const char* test_data = 
        "name,age,city,country,score\n"
        "John Doe,25,New York,USA,95.5\n"
        "Jane Smith,30,London,UK,87.2\n"
        "Bob Johnson,35,Toronto,Canada,92.1\n"
        "Alice Brown,28,Sydney,Australia,89.7\n";
    
    create_test_csv_file("parsing_test.csv", test_data);
    
    DSVViewer viewer = {0};
    DSVResult result = init_viewer(&viewer, "parsing_test.csv", 0, &config);
    ASSERT_EQ(result, DSV_OK);
    
    // Benchmark parsing performance
    const int parse_iterations = 1000;
    double start_time = get_time_ms();
    
    for (int i = 0; i < parse_iterations; i++) {
        for (size_t line = 0; line < viewer.file_data->num_lines; line++) {
            size_t num_fields = parse_line(&viewer, viewer.file_data->line_offsets[line],
                                          viewer.file_data->fields, config.max_cols);
            ASSERT_GT(num_fields, 0);
        }
    }
    
    double end_time = get_time_ms();
    double duration = end_time - start_time;
    double parses_per_ms = (parse_iterations * viewer.file_data->num_lines) / duration;
    
    printf("✓ Performance: %.1f line parses/ms over %d iterations\n", 
           parses_per_ms, parse_iterations);
    
    // Should be able to parse at least 50 lines per millisecond
    ASSERT_GT(parses_per_ms, 50.0);
    
    cleanup_viewer(&viewer);
    unlink("parsing_test.csv");
}

// =============================================================================
// INTEGRATION TEST SUITE
// =============================================================================

TestCase foundation_tests[] = {
    {"Config Default Initialization", test_config_default_initialization},
    {"Config File Loading Valid", test_config_file_loading_valid},
    {"Config File Loading Invalid", test_config_file_loading_invalid},
    {"Config Nonexistent File", test_config_nonexistent_file},
    {"Viewer Init Invalid File", test_viewer_init_with_invalid_file},
    {"Viewer Init Empty File", test_viewer_init_with_empty_file},
    {"Error Result Strings", test_error_result_strings},
    {"Memory Multiple Cycles", test_memory_multiple_init_cleanup_cycles},
    {"Memory Large Field", test_memory_large_field_handling},
    {"Performance File Loading", test_performance_file_loading},
    {"Performance Parsing", test_performance_parsing_benchmark},
};

int foundation_suite_size = sizeof(foundation_tests) / sizeof(TestCase); 