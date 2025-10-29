/**
 * @file engine.h
 * @brief Engine Layer - Core orchestration and business logic
 * @details Processes requests, manages traffic, coordinates responses
 */

#ifndef PAUMIOT_ENGINE_H
#define PAUMIOT_ENGINE_H

#include "../paumiot_core.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct engine_context engine_context_t;
typedef struct engine_config engine_config_t;
typedef struct internal_message internal_message_t;

/* Message Priority */
typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH = 2,
    PRIORITY_CRITICAL = 3
} message_priority_t;

/* Internal Message Format (unified across all protocols) */
struct internal_message {
    /* Identification */
    char *message_id;               /* Unique message identifier */
    char *timestamp;                /* ISO8601 timestamp */
    
    /* Routing */
    char *session_id;               /* Client session ID */
    char *topic;                    /* Topic or URI path */
    operation_type_t operation;     /* Operation type */
    
    /* Payload */
    uint8_t *payload;               /* Message payload */
    size_t payload_len;             /* Payload length */
    
    /* Metadata */
    protocol_type_t protocol;       /* Source protocol */
    qos_level_t qos;                /* Quality of Service */
    message_priority_t priority;    /* Message priority */
    bool retain;                    /* Retain flag (MQTT) */
    uint32_t ttl;                   /* Time-to-live (seconds) */
    
    /* Context */
    void *protocol_context;         /* Protocol-specific context */
    void *user_data;                /* User-defined data */
};

/* Engine Configuration */
struct engine_config {
    /* Threading */
    uint32_t worker_threads;        /* Number of worker threads */
    
    /* Queue Settings */
    uint32_t max_queue_size;        /* Maximum queue size */
    uint32_t high_watermark;        /* High watermark for backpressure */
    uint32_t low_watermark;         /* Low watermark for backpressure */
    
    /* Timeouts */
    uint32_t request_timeout_ms;    /* Request processing timeout */
    uint32_t response_timeout_ms;   /* Response delivery timeout */
    
    /* Traffic Management */
    uint32_t rate_limit_window_ms;  /* Rate limiting window */
    uint32_t max_burst_size;        /* Maximum burst size */
    
    /* Resource Limits */
    uint32_t max_subscriptions_per_client;
    uint32_t max_inflight_messages;
    size_t max_payload_size;
    
    /* Policies */
    bool enable_authorization;      /* Enable ACL checks */
    bool enable_transformation;     /* Enable message transformation */
    bool enable_logging;            /* Enable request logging */
};

/* Engine Statistics */
typedef struct {
    uint64_t requests_processed;
    uint64_t requests_pending;
    uint64_t requests_failed;
    uint64_t messages_published;
    uint64_t messages_delivered;
    uint64_t subscriptions_active;
    uint64_t queue_depth;
    uint64_t throttled_requests;
    double avg_latency_ms;
    double p95_latency_ms;
    double p99_latency_ms;
} engine_stats_t;

/* ============================================================================
 * ENGINE API
 * ========================================================================= */

/**
 * @brief Initialize engine
 * @param config Engine configuration
 * @param sensor_mgr Sensor manager context
 * @param state_mgr State management context
 * @return Engine context or NULL on error
 */
engine_context_t *engine_init(
    const engine_config_t *config,
    void *sensor_mgr,
    void *state_mgr
);

/**
 * @brief Start engine workers
 * @param ctx Engine context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_start(engine_context_t *ctx);

/**
 * @brief Stop engine and drain queue
 * @param ctx Engine context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_stop(engine_context_t *ctx);

/**
 * @brief Cleanup engine and free resources
 * @param ctx Engine context
 */
void engine_cleanup(engine_context_t *ctx);

/* ============================================================================
 * MESSAGE PROCESSING API
 * ========================================================================= */

/**
 * @brief Submit message for processing
 * @param ctx Engine context
 * @param message Message to process
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_process_message(
    engine_context_t *ctx,
    internal_message_t *message
);

/**
 * @brief Process message synchronously (blocking)
 * @param ctx Engine context
 * @param message Message to process
 * @param response Response message (output, optional)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_process_message_sync(
    engine_context_t *ctx,
    const internal_message_t *message,
    internal_message_t **response
);

/* ============================================================================
 * PUBLISH/SUBSCRIBE API
 * ========================================================================= */

/**
 * @brief Handle publish operation
 * @param ctx Engine context
 * @param message Publish message
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_handle_publish(
    engine_context_t *ctx,
    const internal_message_t *message
);

/**
 * @brief Handle subscribe operation
 * @param ctx Engine context
 * @param session_id Client session ID
 * @param topic_filter Topic filter
 * @param qos Desired QoS
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_handle_subscribe(
    engine_context_t *ctx,
    const char *session_id,
    const char *topic_filter,
    qos_level_t qos
);

/**
 * @brief Handle unsubscribe operation
 * @param ctx Engine context
 * @param session_id Client session ID
 * @param topic_filter Topic filter
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_handle_unsubscribe(
    engine_context_t *ctx,
    const char *session_id,
    const char *topic_filter
);

/* ============================================================================
 * MESSAGE UTILITIES API
 * ========================================================================= */

/**
 * @brief Create internal message
 * @return Message instance or NULL on error
 */
internal_message_t *internal_message_create(void);

/**
 * @brief Free internal message
 * @param message Message to free
 */
void internal_message_free(internal_message_t *message);

/**
 * @brief Copy internal message
 * @param original Original message
 * @return Copy of message or NULL on error
 */
internal_message_t *internal_message_copy(const internal_message_t *original);

/**
 * @brief Set message payload
 * @param message Message instance
 * @param payload Payload data
 * @param payload_len Payload length
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t internal_message_set_payload(
    internal_message_t *message,
    const uint8_t *payload,
    size_t payload_len
);

/* ============================================================================
 * STATISTICS API
 * ========================================================================= */

/**
 * @brief Get engine statistics
 * @param ctx Engine context
 * @param stats Statistics output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_get_stats(
    engine_context_t *ctx,
    engine_stats_t *stats
);

/**
 * @brief Reset statistics counters
 * @param ctx Engine context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t engine_reset_stats(engine_context_t *ctx);

/* ============================================================================
 * CONFIGURATION API
 * ========================================================================= */

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void engine_config_init(engine_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_ENGINE_H */
