/**
 * @file test_integration.c
 * @brief Integration tests for common utilities
 * 
 * Tests all components working together in realistic scenarios:
 * - Types, errors, logging, memory pool, and queue
 */

#include "types.h"
#include "errors.h"
#include "logging.h"
#include "memory_pool.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

/* Test message structure for integration scenarios */
typedef struct {
    int id;
    operation_type_t op_type;
    protocol_type_t protocol;
    qos_level_t qos;
    char payload[64];
} message_t;

/* Shared data for multi-threaded integration test */
typedef struct {
    memory_pool_t* pool;
    queue_t* queue;
    int message_count;
    int* processed_count;
    pthread_mutex_t* count_mutex;
} worker_data_t;

/* ========================================
 * Scenario 1: Error Handling with Logging
 * ======================================== */

static void test_error_logging_integration(void) {
    printf("Testing error handling with logging integration...\n");
    
    log_set_level(LOG_LEVEL_INFO);
    
    /* Simulate various error conditions and log them */
    paumiot_result_t results[] = {
        PAUMIOT_SUCCESS,
        PAUMIOT_ERROR_INVALID_PARAM,
        PAUMIOT_ERROR_OUT_OF_MEMORY,
        PAUMIOT_ERROR_QUEUE_FULL,
        PAUMIOT_ERROR_CONNECTION_LOST,
        PAUMIOT_ERROR_TIMEOUT
    };
    
    for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
        paumiot_result_t result = results[i];
        
        if (paumiot_is_error(result)) {
            LOG_ERROR("Operation failed: %s (code: %d)", 
                     paumiot_error_string(result), result);
        } else {
            LOG_INFO("Operation succeeded: %s", paumiot_error_string(result));
        }
        
        /* Verify helper functions */
        if (result == PAUMIOT_SUCCESS) {
            assert(paumiot_is_success(result));
            assert(!paumiot_is_error(result));
        } else {
            assert(!paumiot_is_success(result));
            assert(paumiot_is_error(result));
        }
    }
    
    printf("  ✓ Error logging integration test passed\n");
}

/* ========================================
 * Scenario 2: Memory Pool with Queue
 * ======================================== */

static void test_memory_pool_queue_integration(void) {
    printf("Testing memory pool with queue integration...\n");
    
    /* Create memory pool for message allocation */
    memory_pool_t* pool = pool_create(32, sizeof(message_t));
    assert(pool != NULL);
    LOG_INFO("Created memory pool: capacity=%zu, block_size=%zu",
             pool_capacity(pool), pool_block_size(pool));
    
    /* Create queue for message passing */
    queue_t* queue = queue_create(16, sizeof(message_t*));
    assert(queue != NULL);
    LOG_INFO("Created queue: capacity=%zu", queue_capacity(queue));
    
    /* Allocate messages from pool and enqueue pointers */
    message_t* messages[10];
    for (int i = 0; i < 10; i++) {
        messages[i] = (message_t*)pool_alloc(pool);
        assert(messages[i] != NULL);
        
        /* Fill message */
        messages[i]->id = i;
        messages[i]->op_type = OP_TYPE_PUBLISH;
        messages[i]->protocol = PROTOCOL_TYPE_MQTT;
        messages[i]->qos = QOS_LEVEL_1;
        snprintf(messages[i]->payload, sizeof(messages[i]->payload), 
                "Message %d", i);
        
        /* Enqueue pointer to message */
        paumiot_result_t result = queue_enqueue(queue, &messages[i]);
        assert(result == PAUMIOT_SUCCESS);
        LOG_DEBUG("Enqueued message %d", i);
    }
    
    assert(queue_size(queue) == 10);
    assert(pool_allocated(pool) == 10);
    
    /* Dequeue and process messages */
    for (int i = 0; i < 10; i++) {
        message_t* msg;
        paumiot_result_t result = queue_dequeue(queue, &msg);
        assert(result == PAUMIOT_SUCCESS);
        assert(msg == messages[i]);
        assert(msg->id == i);
        
        LOG_INFO("Processing message %d: protocol=%s, qos=%d, payload='%s'",
                msg->id,
                msg->protocol == PROTOCOL_TYPE_MQTT ? "MQTT" : "Unknown",
                msg->qos,
                msg->payload);
        
        /* Free back to pool */
        pool_free(pool, msg);
    }
    
    assert(queue_is_empty(queue));
    assert(pool_allocated(pool) == 0);
    
    LOG_INFO("All messages processed, pool freed: available=%zu", 
             pool_available(pool));
    
    queue_destroy(queue);
    pool_destroy(pool);
    
    printf("  ✓ Memory pool + queue integration test passed\n");
}

/* ========================================
 * Scenario 3: Multi-threaded Message Pipeline
 * ======================================== */

static void* producer_worker(void* arg) {
    worker_data_t* data = (worker_data_t*)arg;
    
    for (int i = 0; i < data->message_count; i++) {
        /* Allocate message from pool */
        message_t* msg = NULL;
        while (msg == NULL) {
            msg = (message_t*)pool_alloc(data->pool);
            if (msg == NULL) {
                LOG_WARN("Pool exhausted, waiting...");
                usleep(1000);
            }
        }
        
        /* Fill message */
        msg->id = i;
        msg->op_type = OP_TYPE_PUBLISH;
        msg->protocol = PROTOCOL_TYPE_COAP;
        msg->qos = QOS_LEVEL_0;
        snprintf(msg->payload, sizeof(msg->payload), "Data %d", i);
        
        /* Enqueue message pointer */
        paumiot_result_t result;
        while ((result = queue_enqueue(data->queue, &msg)) != PAUMIOT_SUCCESS) {
            if (result == PAUMIOT_ERROR_QUEUE_FULL) {
                usleep(100);
            } else {
                LOG_ERROR("Enqueue failed: %s", paumiot_error_string(result));
                break;
            }
        }
    }
    
    return NULL;
}

static void* consumer_worker(void* arg) {
    worker_data_t* data = (worker_data_t*)arg;
    
    for (int i = 0; i < data->message_count; i++) {
        /* Dequeue message pointer */
        message_t* msg;
        paumiot_result_t result;
        while ((result = queue_dequeue(data->queue, &msg)) != PAUMIOT_SUCCESS) {
            if (result == PAUMIOT_ERROR_QUEUE_EMPTY) {
                usleep(100);
            } else {
                LOG_ERROR("Dequeue failed: %s", paumiot_error_string(result));
                break;
            }
        }
        
        /* Process message (simulate work) */
        assert(msg != NULL);
        
        /* Free back to pool */
        pool_free(data->pool, msg);
        
        /* Update counter */
        pthread_mutex_lock(data->count_mutex);
        (*data->processed_count)++;
        pthread_mutex_unlock(data->count_mutex);
    }
    
    return NULL;
}

static void test_multithreaded_pipeline(void) {
    printf("Testing multi-threaded message pipeline...\n");
    
    log_set_level(LOG_LEVEL_WARN); /* Reduce noise for this test */
    
    /* Create shared resources */
    memory_pool_t* pool = pool_create(32, sizeof(message_t));
    assert(pool != NULL);
    
    queue_t* queue = queue_create(16, sizeof(message_t*));
    assert(queue != NULL);
    
    int processed_count = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    const int num_producers = 2;
    const int num_consumers = 2;
    const int messages_per_producer = 250;
    
    worker_data_t producer_data = {
        .pool = pool,
        .queue = queue,
        .message_count = messages_per_producer,
        .processed_count = NULL,
        .count_mutex = NULL
    };
    
    worker_data_t consumer_data = {
        .pool = pool,
        .queue = queue,
        .message_count = messages_per_producer,
        .processed_count = &processed_count,
        .count_mutex = &count_mutex
    };
    
    pthread_t producers[num_producers];
    pthread_t consumers[num_consumers];
    
    LOG_INFO("Starting pipeline: %d producers, %d consumers, %d messages each",
            num_producers, num_consumers, messages_per_producer);
    
    /* Start consumers first */
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumers[i], NULL, consumer_worker, &consumer_data);
    }
    
    /* Start producers */
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producers[i], NULL, producer_worker, &producer_data);
    }
    
    /* Wait for all producers */
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    /* Wait for all consumers */
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    /* Verify all messages processed */
    int expected_total = num_producers * messages_per_producer;
    assert(processed_count == expected_total);
    assert(queue_is_empty(queue));
    assert(pool_allocated(pool) == 0);
    
    LOG_INFO("Pipeline complete: %d messages processed", processed_count);
    
    pthread_mutex_destroy(&count_mutex);
    queue_destroy(queue);
    pool_destroy(pool);
    
    log_set_level(LOG_LEVEL_INFO); /* Restore default */
    
    printf("  ✓ Multi-threaded pipeline test passed\n");
}

/* ========================================
 * Scenario 4: State Machine with All Types
 * ======================================== */

static void test_state_machine_integration(void) {
    printf("Testing state machine with all type enums...\n");
    
    /* Simulate session lifecycle */
    session_state_t session_states[] = {
        SESSION_STATE_DISCONNECTED,
        SESSION_STATE_CONNECTING,
        SESSION_STATE_CONNECTED,
        SESSION_STATE_ACTIVE,
        SESSION_STATE_DISCONNECTING,
        SESSION_STATE_DISCONNECTED
    };
    
    const char* state_names[] = {
        "DISCONNECTED", "CONNECTING", "CONNECTED", 
        "ACTIVE", "DISCONNECTING", "DISCONNECTED"
    };
    
    protocol_type_t protocol = PROTOCOL_TYPE_MQTT;
    qos_level_t qos = QOS_LEVEL_1;
    
    LOG_INFO("Starting session state machine test");
    LOG_INFO("Protocol: %s, QoS: %d", 
            protocol == PROTOCOL_TYPE_MQTT ? "MQTT" : "Unknown", qos);
    
    for (size_t i = 0; i < sizeof(session_states) / sizeof(session_states[0]); i++) {
        session_state_t state = session_states[i];
        LOG_INFO("Session state transition: %s -> %s",
                i > 0 ? state_names[i-1] : "INITIAL",
                state_names[i]);
        
        /* Simulate operations for each state */
        switch (state) {
            case SESSION_STATE_CONNECTING:
                LOG_DEBUG("Executing operation: CONNECT");
                break;
            case SESSION_STATE_ACTIVE:
                LOG_DEBUG("Executing operation: PUBLISH");
                break;
            case SESSION_STATE_DISCONNECTING:
                LOG_DEBUG("Executing operation: DISCONNECT");
                break;
            default:
                LOG_DEBUG("Executing operation: PING");
                break;
        }
        
        /* Verify state is valid enum value */
        assert(state >= SESSION_STATE_DISCONNECTED && 
               state <= SESSION_STATE_ERROR);
    }
    
    /* Test sensor states */
    sensor_state_t sensor_states[] = {
        SENSOR_STATE_UNKNOWN,
        SENSOR_STATE_ONLINE,
        SENSOR_STATE_DEGRADED,
        SENSOR_STATE_OFFLINE
    };
    
    const char* sensor_state_names[] = {
        "UNKNOWN", "ONLINE", "DEGRADED", "OFFLINE"
    };
    
    LOG_INFO("Testing sensor health monitoring");
    for (size_t i = 0; i < sizeof(sensor_states) / sizeof(sensor_states[0]); i++) {
        sensor_state_t state = sensor_states[i];
        LOG_INFO("Sensor state: %s", sensor_state_names[i]);
        
        if (state == SENSOR_STATE_DEGRADED || state == SENSOR_STATE_OFFLINE) {
            LOG_WARN("Sensor health issue detected: %s", sensor_state_names[i]);
        }
    }
    
    printf("  ✓ State machine integration test passed\n");
}

/* ========================================
 * Scenario 5: Error Recovery Workflow
 * ======================================== */

static void test_error_recovery_workflow(void) {
    printf("Testing error recovery workflow...\n");
    
    memory_pool_t* pool = pool_create(4, sizeof(int));
    assert(pool != NULL);
    
    queue_t* queue = queue_create(4, sizeof(int));
    assert(queue != NULL);
    
    LOG_INFO("Testing pool exhaustion recovery");
    
    /* Fill pool completely */
    void* blocks[4];
    for (int i = 0; i < 4; i++) {
        blocks[i] = pool_alloc(pool);
        assert(blocks[i] != NULL);
    }
    
    /* Try to allocate when full - should fail gracefully */
    void* overflow = pool_alloc(pool);
    if (overflow == NULL) {
        LOG_WARN("Pool allocation failed (expected): %s", 
                paumiot_error_string(PAUMIOT_ERROR_OUT_OF_MEMORY));
    }
    assert(overflow == NULL);
    
    /* Free one block and retry - should succeed */
    pool_free(pool, blocks[0]);
    overflow = pool_alloc(pool);
    assert(overflow != NULL);
    LOG_INFO("Recovery successful: allocation succeeded after free");
    
    LOG_INFO("Testing queue overflow recovery");
    
    /* Fill queue completely */
    for (int i = 0; i < 4; i++) {
        paumiot_result_t result = queue_enqueue(queue, &i);
        assert(result == PAUMIOT_SUCCESS);
    }
    
    /* Try to enqueue when full - should fail gracefully */
    int overflow_val = 999;
    paumiot_result_t result = queue_enqueue(queue, &overflow_val);
    if (result != PAUMIOT_SUCCESS) {
        LOG_WARN("Queue enqueue failed (expected): %s", 
                paumiot_error_string(result));
    }
    assert(result == PAUMIOT_ERROR_QUEUE_FULL);
    
    /* Dequeue one and retry - should succeed */
    int dequeued;
    result = queue_dequeue(queue, &dequeued);
    assert(result == PAUMIOT_SUCCESS);
    
    result = queue_enqueue(queue, &overflow_val);
    assert(result == PAUMIOT_SUCCESS);
    LOG_INFO("Recovery successful: enqueue succeeded after dequeue");
    
    queue_destroy(queue);
    pool_destroy(pool);
    
    printf("  ✓ Error recovery workflow test passed\n");
}

/* ========================================
 * Main Test Runner
 * ======================================== */

int main(void) {
    printf("\n========================================\n");
    printf("Running integration tests...\n");
    printf("========================================\n\n");
    
    /* Initialize logging */
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("Integration test suite started");
    
    /* Run integration scenarios */
    test_error_logging_integration();
    test_memory_pool_queue_integration();
    test_multithreaded_pipeline();
    test_state_machine_integration();
    test_error_recovery_workflow();
    
    LOG_INFO("Integration test suite completed successfully");
    
    printf("\n========================================\n");
    printf("✅ All integration tests passed!\n");
    printf("========================================\n\n");
    
    return 0;
}
