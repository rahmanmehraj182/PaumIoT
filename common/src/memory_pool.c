#include "../include/memory_pool.h"
#include <stdlib.h>
#include <string.h>

/**
 * Memory pool structure
 * Uses a free list for O(1) allocation and deallocation
 */
struct memory_pool {
    void *memory;           /* Pointer to the allocated memory block */
    size_t block_size;      /* Size of each block */
    size_t num_blocks;      /* Total number of blocks */
    size_t free_count;      /* Number of free blocks */
    uint8_t **free_list;    /* Array of pointers to free blocks */
};

/**
 * Create a new memory pool
 */
memory_pool_t *pool_create(size_t block_size, size_t num_blocks) {
    if (block_size == 0 || num_blocks == 0) {
        return NULL;
    }
    
    /* Allocate the pool structure */
    memory_pool_t *pool = (memory_pool_t *)malloc(sizeof(memory_pool_t));
    if (!pool) {
        return NULL;
    }
    
    /* Allocate the memory block */
    pool->memory = malloc(block_size * num_blocks);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }
    
    /* Allocate the free list */
    pool->free_list = (uint8_t **)malloc(sizeof(uint8_t *) * num_blocks);
    if (!pool->free_list) {
        free(pool->memory);
        free(pool);
        return NULL;
    }
    
    /* Initialize fields */
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_count = num_blocks;
    
    /* Initialize free list - all blocks are free initially */
    uint8_t *block_ptr = (uint8_t *)pool->memory;
    for (size_t i = 0; i < num_blocks; i++) {
        pool->free_list[i] = block_ptr;
        block_ptr += block_size;
    }
    
    return pool;
}

/**
 * Destroy a memory pool
 */
void pool_destroy(memory_pool_t *pool) {
    if (!pool) {
        return;
    }
    
    if (pool->free_list) {
        free(pool->free_list);
    }
    
    if (pool->memory) {
        free(pool->memory);
    }
    
    free(pool);
}

/**
 * Allocate a block from the pool
 */
void *pool_alloc(memory_pool_t *pool) {
    if (!pool || pool->free_count == 0) {
        return NULL;
    }
    
    /* Get a block from the free list */
    pool->free_count--;
    void *block = pool->free_list[pool->free_count];
    
    /* Zero out the block for safety */
    memset(block, 0, pool->block_size);
    
    return block;
}

/**
 * Free a block back to the pool
 */
void pool_free(memory_pool_t *pool, void *ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    /* Verify the pointer is within our memory range */
    uint8_t *block_ptr = (uint8_t *)ptr;
    uint8_t *memory_start = (uint8_t *)pool->memory;
    uint8_t *memory_end = memory_start + (pool->block_size * pool->num_blocks);
    
    if (block_ptr < memory_start || block_ptr >= memory_end) {
        /* Invalid pointer - not from this pool */
        return;
    }
    
    /* Verify pointer is aligned to block boundaries */
    size_t offset = block_ptr - memory_start;
    if (offset % pool->block_size != 0) {
        /* Invalid pointer - not aligned */
        return;
    }
    
    /* Check if we have room in the free list */
    if (pool->free_count >= pool->num_blocks) {
        /* Double free or error - pool is already full */
        return;
    }
    
    /* Add back to free list */
    pool->free_list[pool->free_count] = block_ptr;
    pool->free_count++;
}

/**
 * Get the number of available blocks
 */
size_t pool_available(const memory_pool_t *pool) {
    if (!pool) {
        return 0;
    }
    return pool->free_count;
}

/**
 * Get the number of allocated blocks
 */
size_t pool_allocated(const memory_pool_t *pool) {
    if (!pool) {
        return 0;
    }
    return pool->num_blocks - pool->free_count;
}

/**
 * Get the total capacity
 */
size_t pool_capacity(const memory_pool_t *pool) {
    if (!pool) {
        return 0;
    }
    return pool->num_blocks;
}

/**
 * Get the block size
 */
size_t pool_block_size(const memory_pool_t *pool) {
    if (!pool) {
        return 0;
    }
    return pool->block_size;
}

/**
 * Reset the pool
 */
void pool_reset(memory_pool_t *pool) {
    if (!pool) {
        return;
    }
    
    /* Rebuild the free list */
    pool->free_count = pool->num_blocks;
    uint8_t *block_ptr = (uint8_t *)pool->memory;
    for (size_t i = 0; i < pool->num_blocks; i++) {
        pool->free_list[i] = block_ptr;
        block_ptr += pool->block_size;
    }
}
