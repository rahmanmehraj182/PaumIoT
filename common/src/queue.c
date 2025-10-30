/**
 * @file queue.c
 * @brief Lock-free queue implementation
 */

#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/**
 * @brief Queue structure (circular buffer with atomics)
 */
struct queue {
    void* buffer;              /* Circular buffer for elements */
    size_t capacity;           /* Maximum number of elements (power of 2) */
    size_t element_size;       /* Size of each element in bytes */
    size_t mask;               /* Bit mask for wrapping (capacity - 1) */
    
    atomic_size_t head;        /* Write position (producer) */
    atomic_size_t tail;        /* Read position (consumer) */
    atomic_size_t count;       /* Current number of elements */
};

/**
 * @brief Check if a number is a power of 2
 */
static bool is_power_of_two(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * @brief Get element pointer at given index
 */
static void* get_element_ptr(queue_t* queue, size_t index) {
    size_t offset = (index & queue->mask) * queue->element_size;
    return (char*)queue->buffer + offset;
}

queue_t* queue_create(size_t capacity, size_t element_size) {
    /* Validate parameters */
    if (capacity == 0 || element_size == 0) {
        return NULL;
    }
    
    if (!is_power_of_two(capacity)) {
        return NULL;
    }
    
    /* Allocate queue structure */
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));
    if (!queue) {
        return NULL;
    }
    
    /* Allocate buffer */
    queue->buffer = malloc(capacity * element_size);
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }
    
    /* Initialize fields */
    queue->capacity = capacity;
    queue->element_size = element_size;
    queue->mask = capacity - 1;
    
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    atomic_init(&queue->count, 0);
    
    /* Zero the buffer */
    memset(queue->buffer, 0, capacity * element_size);
    
    return queue;
}

void queue_destroy(queue_t* queue) {
    if (!queue) {
        return;
    }
    
    if (queue->buffer) {
        free(queue->buffer);
    }
    
    free(queue);
}

paumiot_result_t queue_enqueue(queue_t* queue, const void* element) {
    if (!queue || !element) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    /* Check if queue is full (atomic load) */
    size_t current_count = atomic_load(&queue->count);
    if (current_count >= queue->capacity) {
        return PAUMIOT_ERROR_QUEUE_FULL;
    }
    
    /* Try to increment count atomically */
    size_t expected = current_count;
    while (!atomic_compare_exchange_weak(&queue->count, &expected, expected + 1)) {
        /* If queue became full, fail */
        if (expected >= queue->capacity) {
            return PAUMIOT_ERROR_QUEUE_FULL;
        }
    }
    
    /* Get head position and increment atomically */
    size_t head = atomic_fetch_add(&queue->head, 1);
    
    /* Copy element to buffer */
    void* dest = get_element_ptr(queue, head);
    memcpy(dest, element, queue->element_size);
    
    return PAUMIOT_SUCCESS;
}

paumiot_result_t queue_dequeue(queue_t* queue, void* element) {
    if (!queue || !element) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    /* Check if queue is empty (atomic load) */
    size_t current_count = atomic_load(&queue->count);
    if (current_count == 0) {
        return PAUMIOT_ERROR_QUEUE_EMPTY;
    }
    
    /* Try to decrement count atomically */
    size_t expected = current_count;
    while (!atomic_compare_exchange_weak(&queue->count, &expected, expected - 1)) {
        /* If queue became empty, fail */
        if (expected == 0) {
            return PAUMIOT_ERROR_QUEUE_EMPTY;
        }
    }
    
    /* Get tail position and increment atomically */
    size_t tail = atomic_fetch_add(&queue->tail, 1);
    
    /* Copy element from buffer */
    void* src = get_element_ptr(queue, tail);
    memcpy(element, src, queue->element_size);
    
    /* Zero the slot for security/debugging */
    memset(src, 0, queue->element_size);
    
    return PAUMIOT_SUCCESS;
}

paumiot_result_t queue_peek(queue_t* queue, void* element) {
    if (!queue || !element) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    /* Check if queue is empty */
    size_t current_count = atomic_load(&queue->count);
    if (current_count == 0) {
        return PAUMIOT_ERROR_QUEUE_EMPTY;
    }
    
    /* Read tail position without modifying */
    size_t tail = atomic_load(&queue->tail);
    
    /* Copy element from buffer */
    void* src = get_element_ptr(queue, tail);
    memcpy(element, src, queue->element_size);
    
    return PAUMIOT_SUCCESS;
}

bool queue_is_empty(const queue_t* queue) {
    if (!queue) {
        return true;
    }
    
    return atomic_load(&queue->count) == 0;
}

bool queue_is_full(const queue_t* queue) {
    if (!queue) {
        return false;
    }
    
    return atomic_load(&queue->count) >= queue->capacity;
}

size_t queue_size(const queue_t* queue) {
    if (!queue) {
        return 0;
    }
    
    return atomic_load(&queue->count);
}

size_t queue_capacity(const queue_t* queue) {
    if (!queue) {
        return 0;
    }
    
    return queue->capacity;
}

void queue_clear(queue_t* queue) {
    if (!queue) {
        return;
    }
    
    /* Reset atomics */
    atomic_store(&queue->head, 0);
    atomic_store(&queue->tail, 0);
    atomic_store(&queue->count, 0);
    
    /* Zero the buffer */
    memset(queue->buffer, 0, queue->capacity * queue->element_size);
}
