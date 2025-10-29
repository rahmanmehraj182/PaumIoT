#ifndef PAUMIOT_MEMORY_POOL_H
#define PAUMIOT_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Memory pool structure
 * Provides fast, fixed-size block allocation with O(1) alloc/free
 */
typedef struct memory_pool memory_pool_t;

/**
 * Create a new memory pool
 * 
 * @param block_size Size of each block in bytes (must be > 0)
 * @param num_blocks Number of blocks in the pool (must be > 0)
 * @return Pointer to the created pool, or NULL on error
 */
memory_pool_t *pool_create(size_t block_size, size_t num_blocks);

/**
 * Destroy a memory pool and free all resources
 * 
 * @param pool The pool to destroy (can be NULL)
 */
void pool_destroy(memory_pool_t *pool);

/**
 * Allocate a block from the pool
 * 
 * @param pool The pool to allocate from
 * @return Pointer to allocated block, or NULL if pool is full
 */
void *pool_alloc(memory_pool_t *pool);

/**
 * Free a block back to the pool
 * 
 * @param pool The pool to return the block to
 * @param ptr Pointer to the block to free
 */
void pool_free(memory_pool_t *pool, void *ptr);

/**
 * Get the number of available (free) blocks
 * 
 * @param pool The pool to query
 * @return Number of available blocks, or 0 if pool is NULL
 */
size_t pool_available(const memory_pool_t *pool);

/**
 * Get the number of allocated (in-use) blocks
 * 
 * @param pool The pool to query
 * @return Number of allocated blocks, or 0 if pool is NULL
 */
size_t pool_allocated(const memory_pool_t *pool);

/**
 * Get the total capacity of the pool
 * 
 * @param pool The pool to query
 * @return Total number of blocks, or 0 if pool is NULL
 */
size_t pool_capacity(const memory_pool_t *pool);

/**
 * Get the block size of the pool
 * 
 * @param pool The pool to query
 * @return Block size in bytes, or 0 if pool is NULL
 */
size_t pool_block_size(const memory_pool_t *pool);

/**
 * Reset the pool (free all blocks)
 * WARNING: Only use if you're sure no blocks are still in use!
 * 
 * @param pool The pool to reset
 */
void pool_reset(memory_pool_t *pool);

#endif /* PAUMIOT_MEMORY_POOL_H */
