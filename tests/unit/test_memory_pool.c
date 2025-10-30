/**
 * @file test_memory_pool.c
 * @brief Unit tests for memory pool allocator
 */

#include "memory_pool.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_pool_create_destroy(void) {
    printf("Testing pool create/destroy...\n");
    
    memory_pool_t* pool = pool_create(10, 64);
    assert(pool != NULL);
    assert(pool_capacity(pool) == 10);
    assert(pool_block_size(pool) == 64);
    assert(pool_available(pool) == 10);
    assert(pool_allocated(pool) == 0);
    
    pool_destroy(pool);
    
    /* NULL destroy should not crash */
    pool_destroy(NULL);
    
    printf("  ✓ Pool create/destroy test passed\n");
}

static void test_pool_invalid_params(void) {
    printf("Testing pool with invalid parameters...\n");
    
    /* Invalid capacity */
    memory_pool_t* pool1 = pool_create(0, 64);
    assert(pool1 == NULL);
    
    /* Invalid block size */
    memory_pool_t* pool2 = pool_create(10, 0);
    assert(pool2 == NULL);
    
    /* Both invalid */
    memory_pool_t* pool3 = pool_create(0, 0);
    assert(pool3 == NULL);
    
    printf("  ✓ Invalid parameters test passed\n");
}

static void test_pool_alloc_free(void) {
    printf("Testing pool alloc/free...\n");
    
    memory_pool_t* pool = pool_create(5, 32);
    assert(pool != NULL);
    
    /* Allocate blocks */
    void* block1 = pool_alloc(pool);
    assert(block1 != NULL);
    assert(pool_allocated(pool) == 1);
    assert(pool_available(pool) == 4);
    
    void* block2 = pool_alloc(pool);
    assert(block2 != NULL);
    assert(block1 != block2);
    assert(pool_allocated(pool) == 2);
    
    void* block3 = pool_alloc(pool);
    assert(block3 != NULL);
    assert(pool_allocated(pool) == 3);
    
    /* Free blocks */
    pool_free(pool, block2);
    assert(pool_allocated(pool) == 2);
    assert(pool_available(pool) == 3);
    
    pool_free(pool, block1);
    assert(pool_allocated(pool) == 1);
    
    pool_free(pool, block3);
    assert(pool_allocated(pool) == 0);
    assert(pool_available(pool) == 5);
    
    pool_destroy(pool);
    
    printf("  ✓ Pool alloc/free test passed\n");
}

static void test_pool_block_reuse(void) {
    printf("Testing pool block reuse...\n");
    
    memory_pool_t* pool = pool_create(3, 16);
    assert(pool != NULL);
    
    /* Allocate and free */
    void* block1 = pool_alloc(pool);
    void* block2 = pool_alloc(pool);
    pool_free(pool, block1);
    pool_free(pool, block2);
    
    /* Allocate again - should reuse freed blocks */
    void* block3 = pool_alloc(pool);
    void* block4 = pool_alloc(pool);
    
    /* One of the reused blocks should match a previously freed one */
    assert(block3 == block2 || block3 == block1);
    assert(block4 == block2 || block4 == block1);
    assert(block3 != block4);
    
    pool_free(pool, block3);
    pool_free(pool, block4);
    pool_destroy(pool);
    
    printf("  ✓ Pool block reuse test passed\n");
}

static void test_pool_block_zeroing(void) {
    printf("Testing pool block zeroing...\n");
    
    memory_pool_t* pool = pool_create(2, 32);
    assert(pool != NULL);
    
    /* Allocate block and write data */
    int* block = (int*)pool_alloc(pool);
    assert(block != NULL);
    
    for (int i = 0; i < 8; i++) {
        block[i] = i + 100;
    }
    
    /* Free and reallocate */
    pool_free(pool, block);
    int* block2 = (int*)pool_alloc(pool);
    
    /* Should be zeroed (free zeros the block) */
    for (int i = 0; i < 8; i++) {
        assert(block2[i] == 0);
    }
    
    pool_free(pool, block2);
    pool_destroy(pool);
    
    printf("  ✓ Pool block zeroing test passed\n");
}

static void test_pool_null_operations(void) {
    printf("Testing pool NULL operations...\n");
    
    /* All operations with NULL pool should handle gracefully */
    void* block = pool_alloc(NULL);
    assert(block == NULL);
    
    pool_free(NULL, (void*)0x1234);  /* Should not crash */
    
    assert(pool_capacity(NULL) == 0);
    assert(pool_block_size(NULL) == 0);
    assert(pool_available(NULL) == 0);
    assert(pool_allocated(NULL) == 0);
    
    pool_reset(NULL);  /* Should not crash */
    
    printf("  ✓ Pool NULL operations test passed\n");
}

static void test_pool_statistics(void) {
    printf("Testing pool statistics...\n");
    
    memory_pool_t* pool = pool_create(5, 128);
    assert(pool != NULL);
    
    /* Initial state */
    assert(pool_capacity(pool) == 5);
    assert(pool_block_size(pool) == 128);
    assert(pool_available(pool) == 5);
    assert(pool_allocated(pool) == 0);
    
    /* After allocations */
    void* blocks[3];
    for (int i = 0; i < 3; i++) {
        blocks[i] = pool_alloc(pool);
    }
    assert(pool_available(pool) == 2);
    assert(pool_allocated(pool) == 3);
    
    /* After frees */
    pool_free(pool, blocks[0]);
    pool_free(pool, blocks[1]);
    assert(pool_available(pool) == 4);
    assert(pool_allocated(pool) == 1);
    
    pool_free(pool, blocks[2]);
    assert(pool_available(pool) == 5);
    assert(pool_allocated(pool) == 0);
    
    pool_destroy(pool);
    
    printf("  ✓ Pool statistics test passed\n");
}

static void test_pool_reset(void) {
    printf("Testing pool reset...\n");
    
    memory_pool_t* pool = pool_create(4, 64);
    assert(pool != NULL);
    
    /* Allocate some blocks */
    void* block1 = pool_alloc(pool);
    void* block2 = pool_alloc(pool);
    void* block3 = pool_alloc(pool);
    assert(pool_allocated(pool) == 3);
    assert(pool_available(pool) == 1);
    
    /* Reset pool */
    pool_reset(pool);
    assert(pool_allocated(pool) == 0);
    assert(pool_available(pool) == 4);
    
    /* Should be able to allocate again */
    void* block4 = pool_alloc(pool);
    assert(block4 != NULL);
    
    pool_free(pool, block4);
    pool_destroy(pool);
    
    /* Suppress unused variable warnings */
    (void)block1;
    (void)block2;
    (void)block3;
    
    printf("  ✓ Pool reset test passed\n");
}

static void test_pool_invalid_free(void) {
    printf("Testing pool invalid free...\n");
    
    memory_pool_t* pool = pool_create(3, 32);
    assert(pool != NULL);
    
    /* Allocate a block */
    void* block = pool_alloc(pool);
    assert(block != NULL);
    
    /* Free with NULL pointer - should not crash */
    pool_free(pool, NULL);
    
    /* Double free - should not crash (implementation dependent) */
    pool_free(pool, block);
    pool_free(pool, block);  /* Second free */
    
    pool_destroy(pool);
    
    printf("  ✓ Pool invalid free test passed\n");
}

int main(void) {
    printf("\n========================================\n");
    printf("Running memory_pool.h tests...\n");
    printf("========================================\n\n");
    
    test_pool_create_destroy();
    test_pool_invalid_params();
    test_pool_alloc_free();
    test_pool_block_reuse();
    test_pool_block_zeroing();
    test_pool_null_operations();
    test_pool_statistics();
    test_pool_reset();
    test_pool_invalid_free();
    
    printf("\n========================================\n");
    printf("✅ All tests passed successfully!\n");
    printf("========================================\n\n");
    
    return 0;
}
