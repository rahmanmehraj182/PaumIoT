/**
 * @file paumiot_client.h
 * @brief PaumIoT Client Plugin API
 * @details Public API for client applications to interact with PaumIoT middleware
 * 
 * This library provides a simple, protocol-agnostic interface for IoT clients
 * to publish and subscribe to topics via the PaumIoT middleware.
 */

#ifndef PAUMIOT_CLIENT_H
#define PAUMIOT_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version Information */
#define PAUMIOT_CLIENT_VERSION_MAJOR 1
#define PAUMIOT_CLIENT_VERSION_MINOR 0
#define PAUMIOT_CLIENT_VERSION_PATCH 0

/* Return Codes */
typedef enum {
    PAUMIOT_CLIENT_SUCCESS = 0,
    PAUMIOT_CLIENT_ERROR_INVALID_PARAM = -1,
    PAUMIOT_CLIENT_ERROR_OUT_OF_MEMORY = -2,
    PAUMIOT_CLIENT_ERROR_CONNECTION_FAILED = -3,
    PAUMIOT_CLIENT_ERROR_TIMEOUT = -4,
    PAUMIOT_CLIENT_ERROR_NOT_CONNECTED = -5,
    PAUMIOT_CLIENT_ERROR_PROTOCOL = -6,
    PAUMIOT_CLIENT_ERROR_BUFFER_OVERFLOW = -7,
    PAUMIOT_CLIENT_ERROR_AUTHENTICATION_FAILED = -8,
    PAUMIOT_CLIENT_ERROR_AUTHORIZATION_FAILED = -9,
    PAUMIOT_CLIENT_ERROR_ALREADY_SUBSCRIBED = -10,
    PAUMIOT_CLIENT_ERROR_NOT_SUBSCRIBED = -11
} paumiot_client_result_t;

/* Protocol Types */
typedef enum {
    PAUMIOT_PROTOCOL_AUTO = 0,      /* Auto-detect based on configuration */
    PAUMIOT_PROTOCOL_MQTT = 1,      /* MQTT v3.1.1/v5.0 */
    PAUMIOT_PROTOCOL_COAP = 2,      /* CoAP RFC 7252 */
    PAUMIOT_PROTOCOL_HTTP = 3       /* HTTP/REST (future) */
} paumiot_protocol_t;

/* QoS Levels */
typedef enum {
    PAUMIOT_QOS_0 = 0,  /* At most once (Fire and forget) */
    PAUMIOT_QOS_1 = 1,  /* At least once (Acknowledged delivery) */
    PAUMIOT_QOS_2 = 2   /* Exactly once (Assured delivery) */
} paumiot_qos_t;

/* Connection State */
typedef enum {
    PAUMIOT_STATE_DISCONNECTED = 0,
    PAUMIOT_STATE_CONNECTING = 1,
    PAUMIOT_STATE_CONNECTED = 2,
    PAUMIOT_STATE_DISCONNECTING = 3
} paumiot_connection_state_t;

/* Forward Declarations */
typedef struct paumiot_client paumiot_client_t;
typedef struct paumiot_message paumiot_message_t;

/* Message Structure */
struct paumiot_message {
    char *topic;                    /* Topic or URI path */
    uint8_t *payload;               /* Message payload */
    size_t payload_len;             /* Payload length */
    paumiot_qos_t qos;             /* Quality of Service */
    bool retain;                    /* Retain flag (MQTT only) */
    void *user_data;                /* User-defined data */
};

/* Callback Function Types */

/**
 * @brief Callback for incoming messages
 * @param client Client instance
 * @param message Received message (must not be freed by callback)
 * @param user_data User-defined data passed during subscription
 */
typedef void (*paumiot_message_callback_t)(
    paumiot_client_t *client,
    const paumiot_message_t *message,
    void *user_data
);

/**
 * @brief Callback for connection state changes
 * @param client Client instance
 * @param state New connection state
 * @param user_data User-defined data
 */
typedef void (*paumiot_connection_callback_t)(
    paumiot_client_t *client,
    paumiot_connection_state_t state,
    void *user_data
);

/**
 * @brief Callback for publish confirmation
 * @param client Client instance
 * @param message_id Unique message identifier
 * @param success True if publish was successful
 * @param user_data User-defined data
 */
typedef void (*paumiot_publish_callback_t)(
    paumiot_client_t *client,
    const char *message_id,
    bool success,
    void *user_data
);

/* Client Configuration */
typedef struct {
    /* Connection Settings */
    const char *host;               /* Middleware hostname/IP */
    uint16_t port;                  /* Middleware port */
    paumiot_protocol_t protocol;    /* Protocol to use */
    uint32_t connect_timeout_ms;    /* Connection timeout */
    uint32_t keepalive_interval_ms; /* Keep-alive interval */
    
    /* Authentication (optional) */
    const char *client_id;          /* Unique client identifier */
    const char *username;           /* Username (if required) */
    const char *password;           /* Password (if required) */
    
    /* TLS/DTLS Settings (optional) */
    bool use_tls;                   /* Enable TLS/DTLS */
    const char *ca_cert_path;       /* CA certificate path */
    const char *client_cert_path;   /* Client certificate path */
    const char *client_key_path;    /* Client private key path */
    
    /* Buffer Settings */
    size_t send_buffer_size;        /* Send buffer size (bytes) */
    size_t recv_buffer_size;        /* Receive buffer size (bytes) */
    size_t max_inflight_messages;   /* Max messages in flight */
    
    /* Callbacks */
    paumiot_connection_callback_t connection_callback;
    void *connection_callback_data;
    
    /* Advanced Settings */
    bool auto_reconnect;            /* Automatic reconnection */
    uint32_t reconnect_delay_ms;    /* Delay between reconnect attempts */
    uint32_t max_reconnect_attempts; /* Max reconnection attempts (0 = infinite) */
} paumiot_client_config_t;

/* ============================================================================
 * CLIENT LIFECYCLE API
 * ========================================================================= */

/**
 * @brief Create a new client instance
 * @param config Client configuration (must remain valid during client lifetime)
 * @return Client instance or NULL on error
 */
paumiot_client_t *paumiot_client_create(const paumiot_client_config_t *config);

/**
 * @brief Destroy client instance and free all resources
 * @param client Client instance
 */
void paumiot_client_destroy(paumiot_client_t *client);

/**
 * @brief Connect to middleware
 * @param client Client instance
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_connect(paumiot_client_t *client);

/**
 * @brief Disconnect from middleware
 * @param client Client instance
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_disconnect(paumiot_client_t *client);

/**
 * @brief Check if client is connected
 * @param client Client instance
 * @return true if connected, false otherwise
 */
bool paumiot_client_is_connected(const paumiot_client_t *client);

/**
 * @brief Get current connection state
 * @param client Client instance
 * @return Current connection state
 */
paumiot_connection_state_t paumiot_client_get_state(const paumiot_client_t *client);

/* ============================================================================
 * PUBLISH API
 * ========================================================================= */

/**
 * @brief Publish a message (synchronous)
 * @param client Client instance
 * @param topic Topic or URI path
 * @param payload Message payload
 * @param payload_len Payload length
 * @param qos Quality of Service level
 * @param retain Retain flag (MQTT only, ignored for other protocols)
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_publish(
    paumiot_client_t *client,
    const char *topic,
    const uint8_t *payload,
    size_t payload_len,
    paumiot_qos_t qos,
    bool retain
);

/**
 * @brief Publish a message (asynchronous)
 * @param client Client instance
 * @param topic Topic or URI path
 * @param payload Message payload
 * @param payload_len Payload length
 * @param qos Quality of Service level
 * @param retain Retain flag
 * @param callback Callback for publish confirmation
 * @param user_data User-defined data passed to callback
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_publish_async(
    paumiot_client_t *client,
    const char *topic,
    const uint8_t *payload,
    size_t payload_len,
    paumiot_qos_t qos,
    bool retain,
    paumiot_publish_callback_t callback,
    void *user_data
);

/* ============================================================================
 * SUBSCRIBE API
 * ========================================================================= */

/**
 * @brief Subscribe to a topic
 * @param client Client instance
 * @param topic_filter Topic filter (may contain wildcards for MQTT)
 * @param qos Desired Quality of Service level
 * @param callback Callback for received messages
 * @param user_data User-defined data passed to callback
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_subscribe(
    paumiot_client_t *client,
    const char *topic_filter,
    paumiot_qos_t qos,
    paumiot_message_callback_t callback,
    void *user_data
);

/**
 * @brief Unsubscribe from a topic
 * @param client Client instance
 * @param topic_filter Topic filter to unsubscribe from
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_unsubscribe(
    paumiot_client_t *client,
    const char *topic_filter
);

/* ============================================================================
 * MESSAGE HANDLING API
 * ========================================================================= */

/**
 * @brief Process pending events (should be called regularly in main loop)
 * @param client Client instance
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_loop(
    paumiot_client_t *client,
    uint32_t timeout_ms
);

/**
 * @brief Start background thread for event processing
 * @param client Client instance
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 * @note Alternative to calling paumiot_client_loop() manually
 */
paumiot_client_result_t paumiot_client_loop_start(paumiot_client_t *client);

/**
 * @brief Stop background thread
 * @param client Client instance
 * @return PAUMIOT_CLIENT_SUCCESS on success, error code otherwise
 */
paumiot_client_result_t paumiot_client_loop_stop(paumiot_client_t *client);

/* ============================================================================
 * UTILITY API
 * ========================================================================= */

/**
 * @brief Get default client configuration
 * @param config Configuration structure to initialize
 */
void paumiot_client_config_init(paumiot_client_config_t *config);

/**
 * @brief Get error string for result code
 * @param result Result code
 * @return Human-readable error string
 */
const char *paumiot_client_error_string(paumiot_client_result_t result);

/**
 * @brief Get client library version
 * @param major Major version (output)
 * @param minor Minor version (output)
 * @param patch Patch version (output)
 */
void paumiot_client_get_version(int *major, int *minor, int *patch);

/**
 * @brief Enable/disable debug logging
 * @param enable true to enable, false to disable
 */
void paumiot_client_set_debug(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_CLIENT_H */
