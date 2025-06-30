#include "../framework/test_runner.h"
#include "buffer_pool.h"
#include "display_state.h"
#include "error_context.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// --- Test Setup ---
static DSVConfig test_config;

void setup_memory_tests() {
    config_init_defaults(&test_config);
}

// --- Test Cases ---

void test_buffer_pool_initialization() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    
    DSVResult result = init_buffer_pool(&buffers, &config);
    TEST_ASSERT(result == DSV_OK, "Buffer pool initialization should succeed");
    
    // Verify all buffers are allocated
    TEST_ASSERT(buffers.render_buffer != NULL, "Render buffer should be allocated");
    TEST_ASSERT(buffers.pad_buffer != NULL, "Pad buffer should be allocated");
    TEST_ASSERT(buffers.cache_buffer != NULL, "Cache buffer should be allocated");
    TEST_ASSERT(buffers.temp_buffer != NULL, "Temp buffer should be allocated");
    TEST_ASSERT(buffers.analysis_buffer != NULL, "Analysis buffer should be allocated");
    TEST_ASSERT(buffers.wide_buffer != NULL, "Wide buffer should be allocated");
    
    cleanup_buffer_pool(&buffers);
}

void test_buffer_pool_acquire_release() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    char* buffer1 = acquire_buffer(&buffers, "test1");
    TEST_ASSERT(buffer1 != NULL, "First buffer acquisition should succeed");
    
    char* buffer2 = acquire_buffer(&buffers, "test2");
    TEST_ASSERT(buffer2 != NULL, "Second buffer acquisition should succeed");
    TEST_ASSERT(buffer1 != buffer2, "Different buffers should be returned");
    
    release_buffer(&buffers, buffer1);
    release_buffer(&buffers, buffer2);
    
    // Should be able to acquire again
    char* buffer3 = acquire_buffer(&buffers, "test3");
    TEST_ASSERT(buffer3 != NULL, "Buffer acquisition after release should succeed");
    
    release_buffer(&buffers, buffer3);
    cleanup_buffer_pool(&buffers);
}

void test_buffer_pool_exhaustion() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    char* acquired[10];
    int count = 0;
    
    // Try to acquire more buffers than available
    for (int i = 0; i < 10; i++) {
        acquired[i] = acquire_buffer(&buffers, "exhaustion_test");
        if (acquired[i] != NULL) {
            count++;
        }
    }
    
    TEST_ASSERT(count == config.buffer_pool_size, "Should only acquire available buffers");
    
    // Release all acquired buffers
    for (int i = 0; i < count; i++) {
        release_buffer(&buffers, acquired[i]);
    }
    
    cleanup_buffer_pool(&buffers);
}

void test_buffer_pool_reset() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    // Acquire all buffers
    char* acquired[5];
    for (int i = 0; i < config.buffer_pool_size; i++) {
        acquired[i] = acquire_buffer(&buffers, "reset_test");
        TEST_ASSERT(acquired[i] != NULL, "Should be able to acquire buffer");
    }
    
    // Pool should be exhausted
    char* extra = acquire_buffer(&buffers, "should_fail");
    TEST_ASSERT(extra == NULL, "Pool should be exhausted");
    
    // Reset should make all buffers available
    reset_buffer_pool(&buffers);
    
    char* after_reset = acquire_buffer(&buffers, "after_reset");
    TEST_ASSERT(after_reset != NULL, "Should be able to acquire after reset");
    
    release_buffer(&buffers, after_reset);
    cleanup_buffer_pool(&buffers);
}

void test_buffer_pool_validation() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    char* valid_buffer = acquire_buffer(&buffers, "validation_test");
    TEST_ASSERT(valid_buffer != NULL, "Should acquire valid buffer");
    
    DSVResult result = validate_buffer_ptr(&buffers, valid_buffer);
    TEST_ASSERT(result == DSV_OK, "Valid buffer should pass validation");
    
    char invalid_buffer[100];
    result = validate_buffer_ptr(&buffers, invalid_buffer);
    TEST_ASSERT(result == DSV_ERROR_INVALID_ARGS, "Invalid buffer should fail validation");
    
    release_buffer(&buffers, valid_buffer);
    cleanup_buffer_pool(&buffers);
}

void test_memory_cleanup() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    char* buffer = acquire_buffer(&buffers, "cleanup_test");
    TEST_ASSERT(buffer != NULL, "Should acquire buffer");
    
    cleanup_buffer_pool(&buffers);
    
    // Verify all pointers are NULL after cleanup
    TEST_ASSERT(buffers.render_buffer == NULL, "Render buffer should be NULL after cleanup");
    TEST_ASSERT(buffers.pad_buffer == NULL, "Pad buffer should be NULL after cleanup");
    TEST_ASSERT(buffers.cache_buffer == NULL, "Cache buffer should be NULL after cleanup");
    TEST_ASSERT(buffers.temp_buffer == NULL, "Temp buffer should be NULL after cleanup");
    TEST_ASSERT(buffers.analysis_buffer == NULL, "Analysis buffer should be NULL after cleanup");
}

void test_initialization_safety() {
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    
    DSVResult result = init_buffer_pool(&buffers, NULL);
    TEST_ASSERT(result == DSV_ERROR_INVALID_ARGS, "Should reject NULL config");
    
    result = init_buffer_pool(NULL, NULL);
    TEST_ASSERT(result == DSV_ERROR_INVALID_ARGS, "Should reject NULL buffers");
}

void test_double_cleanup_safety() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    cleanup_buffer_pool(&buffers);
    cleanup_buffer_pool(&buffers); // Should not crash
    
    TEST_ASSERT(buffers.render_buffer == NULL, "Double cleanup should be safe");
}

void test_null_init_safety() {
    // Test init with null pointer - should not crash
    init_buffer_pool(NULL, &test_config);
    // If we get here, the function handled null gracefully
    TEST_ASSERT(1, "init_buffer_pool should handle NULL gracefully");
}

void test_null_acquire_safety() {
    char* result = acquire_buffer(NULL, "test");
    TEST_ASSERT(result == NULL, "acquire_buffer should return NULL for NULL buffers");
    
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    // Should work with NULL purpose
    char* buffer = acquire_buffer(&buffers, NULL);
    TEST_ASSERT(buffer != NULL, "acquire_buffer should work with NULL purpose");
    cleanup_buffer_pool(&buffers);
}

void test_null_release_safety() {
    // Should not crash with NULL inputs
    release_buffer(NULL, NULL);
    release_buffer(NULL, "dummy");
}

void test_null_reset_safety() {
    // Should not crash with NULL input
    reset_buffer_pool(NULL);
}

void test_pool_robustness() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    // Test multiple acquire/release cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        char* buffer = acquire_buffer(&buffers, "robustness_test");
        TEST_ASSERT(buffer != NULL, "Should acquire buffer in cycle");
        
        // Write to buffer to test it's actually usable
        strcpy(buffer, "test_data");
        TEST_ASSERT(strcmp(buffer, "test_data") == 0, "Buffer should be writable");
        
        release_buffer(&buffers, buffer);
    }
    
    cleanup_buffer_pool(&buffers);
}

void test_invalid_release_handling() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    // Try to release an invalid buffer
    char dummy_buffer[100];
    release_buffer(&buffers, dummy_buffer); // Should not crash
    
    // Try to release the same buffer twice
    char* valid_buffer = acquire_buffer(&buffers, "double_release_test");
    TEST_ASSERT(valid_buffer != NULL, "Should acquire valid buffer");
    
    release_buffer(&buffers, valid_buffer);
    release_buffer(&buffers, valid_buffer); // Should handle gracefully
    
    cleanup_buffer_pool(&buffers);
}

void test_stress_test() {
    DSVConfig config;
    config_init_defaults(&config);
    
    WorkBuffers buffers;
    memset(&buffers, 0, sizeof(WorkBuffers));
    init_buffer_pool(&buffers, &config);
    
    // Rapid acquire/release cycles
    for (int i = 0; i < 100; i++) {
        char* buffer = acquire_buffer(&buffers, "stress_test");
        if (buffer) {
            sprintf(buffer, "test_%d", i);
            release_buffer(&buffers, buffer);
        }
        
        if (i % 10 == 0) {
            reset_buffer_pool(&buffers);
        }
    }
    
    cleanup_buffer_pool(&buffers);
}

// Test registration
TestCase memory_tests[] = {
    {"Buffer Pool Initialization", test_buffer_pool_initialization},
    {"Buffer Pool Acquire/Release", test_buffer_pool_acquire_release},
    {"Buffer Pool Exhaustion", test_buffer_pool_exhaustion},
    {"Buffer Pool Reset", test_buffer_pool_reset},
    {"Buffer Pool Validation", test_buffer_pool_validation},
    {"Memory Cleanup", test_memory_cleanup},
    {"Initialization Safety", test_initialization_safety},
    {"Double Cleanup Safety", test_double_cleanup_safety},
    {"Null Init Safety", test_null_init_safety},
    {"Null Acquire Safety", test_null_acquire_safety},
    {"Null Release Safety", test_null_release_safety},
    {"Null Reset Safety", test_null_reset_safety},
    {"Pool Robustness", test_pool_robustness},
    {"Invalid Release Handling", test_invalid_release_handling},
    {"Stress Test", test_stress_test}
};

int memory_suite_size = sizeof(memory_tests) / sizeof(TestCase); 