/**
 * @file queue.h
 * @brief Lock-free FIFO queue implementation
 * 
 * A thread-safe, lock-free circular queue using C11 atomics.
 * Supports multiple producers and consumers with bounded capacity.
 */

#ifndef PAUMIOT_QUEUE_H
#define PAUMIOT_QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque queue handle
 */
typedef struct queue queue_t;

/**
 * @brief Create a new lock-free queue
 * 
 * @param capacity Maximum number of elements (must be power of 2)
 * @param element_size Size of each element in bytes
 * @return Queue handle on success, NULL on failure
 */
queue_t* queue_create(size_t capacity, size_t element_size);

/**
 * @brief Destroy a queue and free its resources
 * 
 * @param queue Queue to destroy
 */
void queue_destroy(queue_t* queue);

/**
 * @brief Enqueue an element (thread-safe)
 * 
 * @param queue Queue handle
 * @param element Pointer to element data to enqueue
 * @return PAUMIOT_SUCCESS on success, PAUMIOT_ERROR_QUEUE_FULL if full
 */
paumiot_result_t queue_enqueue(queue_t* queue, const void* element);

/**
 * @brief Dequeue an element (thread-safe)
 * 
 * @param queue Queue handle
 * @param element Pointer to buffer to store dequeued element
 * @return PAUMIOT_SUCCESS on success, PAUMIOT_ERROR_QUEUE_EMPTY if empty
 */
paumiot_result_t queue_dequeue(queue_t* queue, void* element);

/**
 * @brief Peek at front element without removing it
 * 
 * @param queue Queue handle
 * @param element Pointer to buffer to store peeked element
 * @return PAUMIOT_SUCCESS on success, PAUMIOT_ERROR_QUEUE_EMPTY if empty
 */
paumiot_result_t queue_peek(queue_t* queue, void* element);

/**
 * @brief Check if queue is empty
 * 
 * @param queue Queue handle
 * @return true if empty, false otherwise
 */
bool queue_is_empty(const queue_t* queue);

/**
 * @brief Check if queue is full
 * 
 * @param queue Queue handle
 * @return true if full, false otherwise
 */
bool queue_is_full(const queue_t* queue);

/**
 * @brief Get current number of elements in queue
 * 
 * @param queue Queue handle
 * @return Number of elements (approximate in concurrent scenarios)
 */
size_t queue_size(const queue_t* queue);

/**
 * @brief Get queue capacity
 * 
 * @param queue Queue handle
 * @return Maximum capacity
 */
size_t queue_capacity(const queue_t* queue);

/**
 * @brief Clear all elements from queue
 * 
 * @param queue Queue handle
 */
void queue_clear(queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_QUEUE_H */
