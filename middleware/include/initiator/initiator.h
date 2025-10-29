/**
 * @file initiator.h
 * @brief Initiator Layer - Entry point for all client connections
 * @details Handles connection acceptance, protocol detection, and request routing
 */

#ifndef PAUMIOT_INITIATOR_H
#define PAUMIOT_INITIATOR_H

#include "../paumiot_core.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct initiator_context initiator_context_t;
typedef struct initiator_config initiator_config_t;
typedef struct connection_info connection_info_t;

/* Connection State */
typedef enum {
    CONNECTION_STATE_INIT = 0,
    CONNECTION_STATE_CONNECTED = 1,
    CONNECTION_STATE_AUTHENTICATED = 2,
    CONNECTION_STATE_ACTIVE = 3,
    CONNECTION_STATE_CLOSING = 4,
    CONNECTION_STATE_CLOSED = 5
} connection_state_t;

/* Transport Type */
typedef enum {
    TRANSPORT_TCP = 0,
    TRANSPORT_UDP = 1,
    TRANSPORT_TLS = 2,
    TRANSPORT_DTLS = 3,
    TRANSPORT_WEBSOCKET = 4
} transport_type_t;

/* Connection Information */
struct connection_info {
    char *connection_id;            /* Unique connection identifier */
    char *client_address;           /* Client IP address */
    uint16_t client_port;           /* Client port */
    protocol_type_t protocol;       /* Detected protocol type */
    transport_type_t transport;     /* Transport layer type */
    connection_state_t state;       /* Connection state */
    uint64_t connected_at;          /* Connection timestamp */
    uint64_t last_activity;         /* Last activity timestamp */
    uint64_t bytes_received;        /* Total bytes received */
    uint64_t bytes_sent;            /* Total bytes sent */
};

/* Initiator Configuration */
struct initiator_config {
    /* Network Settings */
    const char *bind_address;
    uint16_t mqtt_port;
    uint16_t coap_port;
    
    /* Threading */
    uint32_t io_threads;            /* Number of I/O threads */
    uint32_t backlog;               /* Listen backlog */
    
    /* Connection Management */
    uint32_t max_connections;       /* Maximum simultaneous connections */
    uint32_t connection_timeout_ms; /* Connection timeout */
    uint32_t idle_timeout_ms;       /* Idle connection timeout */
    
    /* Buffer Settings */
    size_t recv_buffer_size;        /* Receive buffer size per connection */
    size_t send_buffer_size;        /* Send buffer size per connection */
    
    /* Rate Limiting */
    uint32_t global_rate_limit;     /* Global requests per second */
    uint32_t per_client_rate_limit; /* Per-client requests per second */
    
    /* Protocol Detection */
    bool fast_protocol_detect;      /* Enable fast first-byte detection */
    uint32_t detect_timeout_ms;     /* Protocol detection timeout */
    
    /* Load Balancing */
    bool enable_load_balancing;     /* Enable load balancing */
    const char *lb_algorithm;       /* "round-robin", "least-connections", "random" */
};

/* Statistics */
typedef struct {
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t rejected_connections;
    uint64_t packets_received;
    uint64_t packets_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t protocol_errors;
    uint64_t rate_limited;
} initiator_stats_t;

/* ============================================================================
 * INITIATOR API
 * ========================================================================= */

/**
 * @brief Initialize initiator layer
 * @param config Initiator configuration
 * @param pal_ctx Protocol Adaptation Layer context (for forwarding)
 * @return Initiator context or NULL on error
 */
initiator_context_t *initiator_init(
    const initiator_config_t *config,
    void *pal_ctx
);

/**
 * @brief Start accepting connections
 * @param ctx Initiator context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_start(initiator_context_t *ctx);

/**
 * @brief Stop accepting connections and close existing ones
 * @param ctx Initiator context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_stop(initiator_context_t *ctx);

/**
 * @brief Cleanup initiator and free resources
 * @param ctx Initiator context
 */
void initiator_cleanup(initiator_context_t *ctx);

/* ============================================================================
 * CONNECTION MANAGEMENT API
 * ========================================================================= */

/**
 * @brief Get active connection information
 * @param ctx Initiator context
 * @param connection_id Connection identifier
 * @param info Connection information output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_get_connection_info(
    initiator_context_t *ctx,
    const char *connection_id,
    connection_info_t *info
);

/**
 * @brief Close a specific connection
 * @param ctx Initiator context
 * @param connection_id Connection identifier
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_close_connection(
    initiator_context_t *ctx,
    const char *connection_id
);

/**
 * @brief Get list of active connections
 * @param ctx Initiator context
 * @param connections Array of connection IDs (output)
 * @param count Number of connections (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_list_connections(
    initiator_context_t *ctx,
    char ***connections,
    size_t *count
);

/* ============================================================================
 * PROTOCOL DETECTION API
 * ========================================================================= */

/**
 * @brief Detect protocol from packet data
 * @param packet Packet data
 * @param packet_len Packet length
 * @param protocol Detected protocol (output)
 * @return PAUMIOT_SUCCESS if detected, error code otherwise
 */
paumiot_result_t initiator_detect_protocol(
    const uint8_t *packet,
    size_t packet_len,
    protocol_type_t *protocol
);

/* ============================================================================
 * STATISTICS API
 * ========================================================================= */

/**
 * @brief Get initiator statistics
 * @param ctx Initiator context
 * @param stats Statistics output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_get_stats(
    initiator_context_t *ctx,
    initiator_stats_t *stats
);

/**
 * @brief Reset statistics counters
 * @param ctx Initiator context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t initiator_reset_stats(initiator_context_t *ctx);

/* ============================================================================
 * CONFIGURATION API
 * ========================================================================= */

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void initiator_config_init(initiator_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_INITIATOR_H */
