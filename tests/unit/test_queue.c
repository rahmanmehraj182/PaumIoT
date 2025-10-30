/**
 * @file test_queue.c
 * @brief Unit tests for lock-free queue
 */

#include "queue.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

/* Test message structure */
typedef struct {
    int id;
    char message[32];
} test_message_t;

/* Test counters for concurrent tests */
typedef struct {
    queue_t* queue;
    int num_items;
    int* produced;
    int* consumed;
} thread_data_t;

/* ========================================
 * Basic Functionality Tests
 * ======================================== */

static void test_queue_create_destroy(void) {
    printf("Testing queue create/destroy...\n");
    
    /* Valid creation */
    queue_t* queue = queue_create(8, sizeof(int));
    assert(queue != NULL);
    assert(queue_capacity(queue) == 8);
    assert(queue_size(queue) == 0);
    assert(queue_is_empty(queue));
    assert(!queue_is_full(queue));
    queue_destroy(queue);
    
    /* NULL destroy should not crash */
    queue_destroy(NULL);
    
    printf("  ✓ Queue create/destroy test passed\n");
}

static void test_queue_invalid_params(void) {
    printf("Testing queue with invalid parameters...\n");
    
    /* Invalid capacity (0) */
    queue_t* queue1 = queue_create(0, sizeof(int));
    assert(queue1 == NULL);
    
    /* Invalid element size (0) */
    queue_t* queue2 = queue_create(8, 0);
    assert(queue2 == NULL);
    
    /* Invalid capacity (not power of 2) */
    queue_t* queue3 = queue_create(7, sizeof(int));
    assert(queue3 == NULL);
    
    queue_t* queue4 = queue_create(10, sizeof(int));
    assert(queue4 == NULL);
    
    /* Valid power of 2 capacities should work */
    queue_t* queue5 = queue_create(4, sizeof(int));
    assert(queue5 != NULL);
    queue_destroy(queue5);
    
    queue_t* queue6 = queue_create(16, sizeof(int));
    assert(queue6 != NULL);
    queue_destroy(queue6);
    
    printf("  ✓ Invalid parameters test passed\n");
}

static void test_queue_enqueue_dequeue(void) {
    printf("Testing queue enqueue/dequeue...\n");
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    /* Enqueue items */
    int value1 = 100;
    int value2 = 200;
    int value3 = 300;
    
    assert(queue_enqueue(queue, &value1) == PAUMIOT_SUCCESS);
    assert(queue_size(queue) == 1);
    assert(!queue_is_empty(queue));
    
    assert(queue_enqueue(queue, &value2) == PAUMIOT_SUCCESS);
    assert(queue_size(queue) == 2);
    
    assert(queue_enqueue(queue, &value3) == PAUMIOT_SUCCESS);
    assert(queue_size(queue) == 3);
    
    /* Dequeue items in FIFO order */
    int result;
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 100);
    assert(queue_size(queue) == 2);
    
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 200);
    assert(queue_size(queue) == 1);
    
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 300);
    assert(queue_size(queue) == 0);
    assert(queue_is_empty(queue));
    
    queue_destroy(queue);
    printf("  ✓ Queue enqueue/dequeue test passed\n");
}

static void test_queue_full_empty(void) {
    printf("Testing queue full/empty conditions...\n");
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    /* Fill queue to capacity */
    for (int i = 0; i < 4; i++) {
        assert(queue_enqueue(queue, &i) == PAUMIOT_SUCCESS);
    }
    assert(queue_is_full(queue));
    assert(queue_size(queue) == 4);
    
    /* Try to enqueue when full */
    int overflow = 999;
    assert(queue_enqueue(queue, &overflow) == PAUMIOT_ERROR_QUEUE_FULL);
    assert(queue_size(queue) == 4);
    
    /* Empty the queue */
    int result;
    for (int i = 0; i < 4; i++) {
        assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
        assert(result == i);
    }
    assert(queue_is_empty(queue));
    
    /* Try to dequeue when empty */
    assert(queue_dequeue(queue, &result) == PAUMIOT_ERROR_QUEUE_EMPTY);
    
    queue_destroy(queue);
    printf("  ✓ Queue full/empty test passed\n");
}

static void test_queue_peek(void) {
    printf("Testing queue peek...\n");
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    /* Peek on empty queue should fail */
    int result;
    assert(queue_peek(queue, &result) == PAUMIOT_ERROR_QUEUE_EMPTY);
    
    /* Enqueue items */
    int value1 = 111;
    int value2 = 222;
    assert(queue_enqueue(queue, &value1) == PAUMIOT_SUCCESS);
    assert(queue_enqueue(queue, &value2) == PAUMIOT_SUCCESS);
    
    /* Peek should see front item without removing */
    assert(queue_peek(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 111);
    assert(queue_size(queue) == 2); /* Size unchanged */
    
    /* Peek again should see same item */
    assert(queue_peek(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 111);
    
    /* Dequeue should remove the peeked item */
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 111);
    
    /* Now peek should see next item */
    assert(queue_peek(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 222);
    
    queue_destroy(queue);
    printf("  ✓ Queue peek test passed\n");
}

static void test_queue_wrap_around(void) {
    printf("Testing queue wrap-around...\n");
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    /* Fill and empty multiple times to test circular behavior */
    for (int cycle = 0; cycle < 3; cycle++) {
        /* Fill */
        for (int i = 0; i < 4; i++) {
            int value = cycle * 100 + i;
            assert(queue_enqueue(queue, &value) == PAUMIOT_SUCCESS);
        }
        
        /* Empty */
        for (int i = 0; i < 4; i++) {
            int result;
            int expected = cycle * 100 + i;
            assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
            assert(result == expected);
        }
        assert(queue_is_empty(queue));
    }
    
    queue_destroy(queue);
    printf("  ✓ Queue wrap-around test passed\n");
}

static void test_queue_clear(void) {
    printf("Testing queue clear...\n");
    
    queue_t* queue = queue_create(8, sizeof(int));
    assert(queue != NULL);
    
    /* Add some items */
    for (int i = 0; i < 5; i++) {
        assert(queue_enqueue(queue, &i) == PAUMIOT_SUCCESS);
    }
    assert(queue_size(queue) == 5);
    
    /* Clear queue */
    queue_clear(queue);
    assert(queue_size(queue) == 0);
    assert(queue_is_empty(queue));
    
    /* Should be able to use queue normally after clear */
    int value = 999;
    assert(queue_enqueue(queue, &value) == PAUMIOT_SUCCESS);
    int result;
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result == 999);
    
    queue_destroy(queue);
    printf("  ✓ Queue clear test passed\n");
}

static void test_queue_null_operations(void) {
    printf("Testing queue NULL operations...\n");
    
    int value = 42;
    int result;
    
    /* All operations with NULL queue should handle gracefully */
    assert(queue_enqueue(NULL, &value) == PAUMIOT_ERROR_INVALID_PARAM);
    assert(queue_dequeue(NULL, &result) == PAUMIOT_ERROR_INVALID_PARAM);
    assert(queue_peek(NULL, &result) == PAUMIOT_ERROR_INVALID_PARAM);
    assert(queue_is_empty(NULL) == true);
    assert(queue_is_full(NULL) == false);
    assert(queue_size(NULL) == 0);
    assert(queue_capacity(NULL) == 0);
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    /* Operations with NULL element should fail */
    assert(queue_enqueue(queue, NULL) == PAUMIOT_ERROR_INVALID_PARAM);
    assert(queue_dequeue(queue, NULL) == PAUMIOT_ERROR_INVALID_PARAM);
    assert(queue_peek(queue, NULL) == PAUMIOT_ERROR_INVALID_PARAM);
    
    queue_destroy(queue);
    printf("  ✓ Queue NULL operations test passed\n");
}

static void test_queue_struct_elements(void) {
    printf("Testing queue with struct elements...\n");
    
    queue_t* queue = queue_create(4, sizeof(test_message_t));
    assert(queue != NULL);
    
    /* Enqueue struct messages */
    test_message_t msg1 = {1, "Hello"};
    test_message_t msg2 = {2, "World"};
    test_message_t msg3 = {3, "Queue"};
    
    assert(queue_enqueue(queue, &msg1) == PAUMIOT_SUCCESS);
    assert(queue_enqueue(queue, &msg2) == PAUMIOT_SUCCESS);
    assert(queue_enqueue(queue, &msg3) == PAUMIOT_SUCCESS);
    
    /* Dequeue and verify */
    test_message_t result;
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result.id == 1);
    assert(strcmp(result.message, "Hello") == 0);
    
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result.id == 2);
    assert(strcmp(result.message, "World") == 0);
    
    assert(queue_dequeue(queue, &result) == PAUMIOT_SUCCESS);
    assert(result.id == 3);
    assert(strcmp(result.message, "Queue") == 0);
    
    queue_destroy(queue);
    printf("  ✓ Queue struct elements test passed\n");
}

/* ========================================
 * Concurrent Tests
 * ======================================== */

static void* producer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_items; i++) {
        int value = i;
        
        /* Keep trying until enqueue succeeds */
        while (queue_enqueue(data->queue, &value) != PAUMIOT_SUCCESS) {
            usleep(1); /* Brief sleep on full queue */
        }
        
        (*data->produced)++;
    }
    
    return NULL;
}

static void* consumer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_items; i++) {
        int value;
        
        /* Keep trying until dequeue succeeds */
        while (queue_dequeue(data->queue, &value) != PAUMIOT_SUCCESS) {
            usleep(1); /* Brief sleep on empty queue */
        }
        
        (*data->consumed)++;
    }
    
    return NULL;
}

static void test_queue_single_producer_consumer(void) {
    printf("Testing queue with concurrent single producer/consumer...\n");
    
    queue_t* queue = queue_create(16, sizeof(int));
    assert(queue != NULL);
    
    int produced = 0;
    int consumed = 0;
    int num_items = 1000;
    
    thread_data_t data = {
        .queue = queue,
        .num_items = num_items,
        .produced = &produced,
        .consumed = &consumed
    };
    
    pthread_t producer, consumer;
    
    /* Start threads */
    pthread_create(&producer, NULL, producer_thread, &data);
    pthread_create(&consumer, NULL, consumer_thread, &data);
    
    /* Wait for completion */
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    /* Verify all items were processed */
    assert(produced == num_items);
    assert(consumed == num_items);
    assert(queue_is_empty(queue));
    
    queue_destroy(queue);
    printf("  ✓ Single producer/consumer test passed\n");
}

static void test_queue_multiple_producers_consumers(void) {
    printf("Testing queue with multiple producers/consumers...\n");
    
    queue_t* queue = queue_create(32, sizeof(int));
    assert(queue != NULL);
    
    const int num_producers = 3;
    const int num_consumers = 3;
    const int items_per_thread = 500;
    
    int produced[num_producers];
    int consumed[num_consumers];
    thread_data_t producer_data[num_producers];
    thread_data_t consumer_data[num_consumers];
    pthread_t producers[num_producers];
    pthread_t consumers[num_consumers];
    
    /* Initialize data */
    for (int i = 0; i < num_producers; i++) {
        produced[i] = 0;
        producer_data[i].queue = queue;
        producer_data[i].num_items = items_per_thread;
        producer_data[i].produced = &produced[i];
        producer_data[i].consumed = NULL;
    }
    
    for (int i = 0; i < num_consumers; i++) {
        consumed[i] = 0;
        consumer_data[i].queue = queue;
        consumer_data[i].num_items = items_per_thread;
        consumer_data[i].produced = NULL;
        consumer_data[i].consumed = &consumed[i];
    }
    
    /* Start all threads */
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producers[i], NULL, producer_thread, &producer_data[i]);
    }
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumers[i], NULL, consumer_thread, &consumer_data[i]);
    }
    
    /* Wait for all threads */
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    /* Verify totals */
    int total_produced = 0;
    int total_consumed = 0;
    for (int i = 0; i < num_producers; i++) {
        total_produced += produced[i];
    }
    for (int i = 0; i < num_consumers; i++) {
        total_consumed += consumed[i];
    }
    
    assert(total_produced == num_producers * items_per_thread);
    assert(total_consumed == num_consumers * items_per_thread);
    assert(queue_is_empty(queue));
    
    queue_destroy(queue);
    printf("  ✓ Multiple producers/consumers test passed\n");
}

/* ========================================
 * Main Test Runner
 * ======================================== */

int main(void) {
    printf("\n========================================\n");
    printf("Running queue.h tests...\n");
    printf("========================================\n\n");
    
    /* Basic functionality tests */
    test_queue_create_destroy();
    test_queue_invalid_params();
    test_queue_enqueue_dequeue();
    test_queue_full_empty();
    test_queue_peek();
    test_queue_wrap_around();
    test_queue_clear();
    test_queue_null_operations();
    test_queue_struct_elements();
    
    /* Concurrent tests */
    test_queue_single_producer_consumer();
    test_queue_multiple_producers_consumers();
    
    printf("\n========================================\n");
    printf("✅ All tests passed successfully!\n");
    printf("========================================\n\n");
    
    return 0;
}
