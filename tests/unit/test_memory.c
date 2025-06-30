#include "framework/test_runner.h"
#include "buffer_pool.h"
#include "display_state.h"
#include "error_context.h"
#include <string.h>

// Test 1: Buffer Pool Usage - Basic Functionality
void test_buffer_pool_initialization() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Test that all buffers are properly initialized as available
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        TEST_ASSERT(pool.is_in_use[i] == 0, "All buffers should be available after initialization");
        TEST_ASSERT(pool.buffer_sizes[i] == MAX_FIELD_LEN, "Buffer sizes should be set correctly");
        TEST_ASSERT(pool.buffer_names[i] != NULL, "Buffer names should be set");
    }
}

// Test 2: Buffer Pool Usage - Acquire and Release
void test_buffer_pool_acquire_release() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Acquire a buffer
    char* buffer1 = acquire_buffer(&pool, "test purpose");
    TEST_ASSERT(buffer1 != NULL, "Should be able to acquire first buffer");
    TEST_ASSERT(buffer1 == pool.render_buffer, "First buffer should be render_buffer");
    
    // Acquire another buffer
    char* buffer2 = acquire_buffer(&pool, "test purpose 2");
    TEST_ASSERT(buffer2 != NULL, "Should be able to acquire second buffer");
    TEST_ASSERT(buffer2 == pool.pad_buffer, "Second buffer should be pad_buffer");
    TEST_ASSERT(buffer2 != buffer1, "Buffers should be different");
    
    // Release first buffer
    release_buffer(&pool, buffer1);
    
    // Acquire again - should get the first buffer back
    char* buffer3 = acquire_buffer(&pool, "test purpose 3");
    TEST_ASSERT(buffer3 == buffer1, "Should reuse the released buffer");
}

// Test 3: Buffer Pool Usage - Pool Exhaustion
void test_buffer_pool_exhaustion() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    char* buffers[BUFFER_POOL_SIZE + 1];
    
    // Exhaust all buffers
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        buffers[i] = acquire_buffer(&pool, "exhaustion test");
        TEST_ASSERT(buffers[i] != NULL, "Should be able to acquire buffer within pool size");
    }
    
    // Try to acquire one more - should fail
    buffers[BUFFER_POOL_SIZE] = acquire_buffer(&pool, "overflow test");
    TEST_ASSERT(buffers[BUFFER_POOL_SIZE] == NULL, "Should fail to acquire buffer when pool is exhausted");
    
    // Release one buffer and try again
    release_buffer(&pool, buffers[0]);
    char* recovered_buffer = acquire_buffer(&pool, "recovery test");
    TEST_ASSERT(recovered_buffer != NULL, "Should be able to acquire buffer after release");
}

// Test 4: Buffer Pool Usage - Reset Functionality
void test_buffer_pool_reset() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Acquire some buffers
    char* buffer1 = acquire_buffer(&pool, "reset test 1");
    char* buffer2 = acquire_buffer(&pool, "reset test 2");
    TEST_ASSERT(buffer1 != NULL && buffer2 != NULL, "Should acquire buffers before reset");
    
    // Reset pool
    reset_buffer_pool(&pool);
    
    // All buffers should be available again
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        TEST_ASSERT(pool.is_in_use[i] == 0, "All buffers should be available after reset");
    }
}

// Test 5: Buffer Pool Usage - Validation
void test_buffer_pool_validation() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Test valid buffer pointers
    TEST_ASSERT(validate_buffer_ptr(&pool, pool.render_buffer) == DSV_OK, "render_buffer should be valid");
    TEST_ASSERT(validate_buffer_ptr(&pool, pool.pad_buffer) == DSV_OK, "pad_buffer should be valid");
    TEST_ASSERT(validate_buffer_ptr(&pool, pool.cache_buffer) == DSV_OK, "cache_buffer should be valid");
    
    // Test invalid buffer pointer
    char external_buffer[100];
    TEST_ASSERT(validate_buffer_ptr(&pool, external_buffer) == DSV_ERROR_INVALID_ARGS, "External buffer should be invalid");
    
    // Test null inputs
    TEST_ASSERT(validate_buffer_ptr(NULL, pool.render_buffer) == DSV_ERROR_INVALID_ARGS, "NULL pool should be invalid");
    TEST_ASSERT(validate_buffer_ptr(&pool, NULL) == DSV_ERROR_INVALID_ARGS, "NULL buffer should be invalid");
}

// Test 6: Memory Leak Detection - Buffer Pool Cleanup
void test_buffer_pool_memory_cleanup() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Acquire all buffers
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        char* buffer = acquire_buffer(&pool, "cleanup test");
        TEST_ASSERT(buffer != NULL, "Should acquire buffer for cleanup test");
        
        // Write to buffer to ensure it's usable
        strcpy(buffer, "test data");
        TEST_ASSERT(strcmp(buffer, "test data") == 0, "Buffer should be writable and readable");
    }
    
    // Reset should clean up properly (no memory leaks)
    reset_buffer_pool(&pool);
    
    // All buffers should be clean and available
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        TEST_ASSERT(pool.is_in_use[i] == 0, "Buffer should be marked as available after cleanup");
    }
}

// Test 7: Memory Leak Detection - Proper Initialization
void test_buffer_pool_initialization_safety() {
    ManagedBufferPool pool;
    
    // Initialize pool
    init_buffer_pool(&pool);
    
    // Verify all buffers are properly cleared
    TEST_ASSERT(pool.render_buffer[0] == '\0', "render_buffer should be cleared");
    TEST_ASSERT(pool.pad_buffer[0] == '\0', "pad_buffer should be cleared");
    TEST_ASSERT(pool.cache_buffer[0] == '\0', "cache_buffer should be cleared");
    TEST_ASSERT(pool.temp_buffer[0] == '\0', "temp_buffer should be cleared");
    TEST_ASSERT(pool.analysis_buffer[0] == '\0', "analysis_buffer should be cleared");
    
    // Wide buffer should also be cleared
    TEST_ASSERT(pool.wide_buffer[0] == L'\0', "wide_buffer should be cleared");
}

// Test 8: Memory Leak Detection - Double Cleanup Safety
void test_buffer_pool_double_cleanup() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Acquire and use a buffer
    char* buffer = acquire_buffer(&pool, "double cleanup test");
    TEST_ASSERT(buffer != NULL, "Should acquire buffer");
    strcpy(buffer, "test");
    
    // Release normally
    release_buffer(&pool, buffer);
    
    // Try to release again - should handle gracefully
    release_buffer(&pool, buffer);
    
    // Pool should still be in valid state
    char* new_buffer = acquire_buffer(&pool, "recovery test");
    TEST_ASSERT(new_buffer != NULL, "Should be able to acquire buffer after double cleanup");
}

// Test 9: Null Pointer Safety - Init with NULL
void test_buffer_pool_null_init() {
    // Test init with null pointer - should not crash
    init_buffer_pool(NULL);
    // If we get here, the function handled null gracefully
    TEST_ASSERT(1, "init_buffer_pool should handle NULL gracefully");
}

// Test 10: Null Pointer Safety - Acquire with NULL
void test_buffer_pool_null_acquire() {
    char* result = acquire_buffer(NULL, "test");
    TEST_ASSERT(result == NULL, "acquire_buffer should return NULL for NULL pool");
    
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Should work with NULL purpose
    char* buffer = acquire_buffer(&pool, NULL);
    TEST_ASSERT(buffer != NULL, "acquire_buffer should work with NULL purpose");
}

// Test 11: Null Pointer Safety - Release with NULL
void test_buffer_pool_null_release() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Test releasing with null pool
    release_buffer(NULL, pool.render_buffer);
    
    // Test releasing null buffer
    release_buffer(&pool, NULL);
    
    // Test releasing both null
    release_buffer(NULL, NULL);
    
    // Pool should still be functional
    char* buffer = acquire_buffer(&pool, "null safety test");
    TEST_ASSERT(buffer != NULL, "Pool should still work after null release attempts");
}

// Test 12: Null Pointer Safety - Reset with NULL
void test_buffer_pool_null_reset() {
    // Should not crash
    reset_buffer_pool(NULL);
    TEST_ASSERT(1, "reset_buffer_pool should handle NULL gracefully");
}

// Test 13: Allocation Failure Handling - Pool Robustness
void test_buffer_pool_robustness() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Test that repeated acquire/release cycles work
    for (int cycle = 0; cycle < 10; cycle++) {
        char* buffers[BUFFER_POOL_SIZE];
        
        // Acquire all buffers
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            buffers[i] = acquire_buffer(&pool, "robustness test");
            TEST_ASSERT(buffers[i] != NULL, "Should acquire buffer in robustness test");
        }
        
        // Release all buffers
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            release_buffer(&pool, buffers[i]);
        }
    }
    
    TEST_ASSERT(1, "Buffer pool should handle repeated acquire/release cycles");
}

// Test 14: Allocation Failure Handling - Invalid Buffer Release
void test_buffer_pool_invalid_release() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Try to release a buffer that was never acquired
    char invalid_buffer[100];
    release_buffer(&pool, invalid_buffer);
    
    // Pool should still be functional
    char* valid_buffer = acquire_buffer(&pool, "post-invalid test");
    TEST_ASSERT(valid_buffer != NULL, "Pool should work after invalid release attempt");
}

// Test 15: Allocation Failure Handling - Stress Test
void test_buffer_pool_stress() {
    ManagedBufferPool pool;
    init_buffer_pool(&pool);
    
    // Rapid acquire/release pattern
    for (int i = 0; i < 100; i++) {
        char* buffer = acquire_buffer(&pool, "stress test");
        if (buffer != NULL) {
            strcpy(buffer, "stress");
            TEST_ASSERT(strcmp(buffer, "stress") == 0, "Buffer should maintain data integrity under stress");
            release_buffer(&pool, buffer);
        }
    }
    
    TEST_ASSERT(1, "Buffer pool should handle stress test");
}

// Test registration
TestCase memory_tests[] = {
    {"Buffer Pool Initialization", test_buffer_pool_initialization},
    {"Buffer Pool Acquire/Release", test_buffer_pool_acquire_release},
    {"Buffer Pool Exhaustion", test_buffer_pool_exhaustion},
    {"Buffer Pool Reset", test_buffer_pool_reset},
    {"Buffer Pool Validation", test_buffer_pool_validation},
    {"Memory Cleanup", test_buffer_pool_memory_cleanup},
    {"Initialization Safety", test_buffer_pool_initialization_safety},
    {"Double Cleanup Safety", test_buffer_pool_double_cleanup},
    {"Null Init Safety", test_buffer_pool_null_init},
    {"Null Acquire Safety", test_buffer_pool_null_acquire},
    {"Null Release Safety", test_buffer_pool_null_release},
    {"Null Reset Safety", test_buffer_pool_null_reset},
    {"Pool Robustness", test_buffer_pool_robustness},
    {"Invalid Release Handling", test_buffer_pool_invalid_release},
    {"Stress Test", test_buffer_pool_stress}
};

int memory_suite_size = sizeof(memory_tests) / sizeof(TestCase); 