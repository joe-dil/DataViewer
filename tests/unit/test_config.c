#include "framework/test_runner.h"
#include "config.h"
#include <stdio.h>
#include <unistd.h>

// --- Test Cases ---

void test_config_defaults() {
    DSVConfig config;
    config_init_defaults(&config);
    
    TEST_ASSERT(config.max_cols == 256, "Default max_cols should be 256");
    TEST_ASSERT(config.cache_size == 16384, "Default cache_size should be 16384");
    TEST_ASSERT(config.buffer_size == 8192, "Default buffer_size should be 8192");
}

void test_config_loading() {
    DSVConfig config;
    config_init_defaults(&config);
    
    // Create a temporary config file
    const char *filename = "temp_config.conf";
    FILE *f = fopen(filename, "w");
    fprintf(f, "max_cols = 512\n");
    fprintf(f, "cache_size= 32768 # With comment\n");
    fprintf(f, "  buffer_size = 4096  \n");
    fprintf(f, "unknown_key=value\n"); // Should be ignored
    fprintf(f, "invalid_line\n"); // Should be ignored
    fclose(f);
    
    DSVResult res = config_load_from_file(&config, filename);
    TEST_ASSERT(res == DSV_OK, "Config loading should succeed");
    
    TEST_ASSERT(config.max_cols == 512, "max_cols should be overridden to 512");
    TEST_ASSERT(config.cache_size == 32768, "cache_size should be overridden to 32768");
    TEST_ASSERT(config.buffer_size == 4096, "buffer_size should be overridden to 4096");
    
    // Cleanup
    remove(filename);
}

void test_config_validation() {
    DSVConfig config;
    config_init_defaults(&config);
    
    // Test valid config
    DSVResult res = config_validate(&config);
    TEST_ASSERT(res == DSV_OK, "Default config should be valid");
    
    // Test invalid value
    config.max_cols = -1;
    res = config_validate(&config);
    TEST_ASSERT(res == DSV_ERROR, "Config with negative max_cols should be invalid");
    
    // Test invalid range
    config_init_defaults(&config);
    config.min_column_width = 20;
    config.max_column_width = 10;
    res = config_validate(&config);
    TEST_ASSERT(res == DSV_ERROR, "Config with min > max width should be invalid");
}

// --- Test Suite ---

TestCase config_tests[] = {
    {"Config Defaults", test_config_defaults},
    {"Config Loading", test_config_loading},
    {"Config Validation", test_config_validation},
};

int config_suite_size = sizeof(config_tests) / sizeof(TestCase); 