/**
 * @file state_management.h
 * @brief State Management - Centralized state storage
 * @details Tracks sessions, subscriptions, protocol state, and transactions
 */

#ifndef PAUMIOT_STATE_MANAGEMENT_H
#define PAUMIOT_STATE_MANAGEMENT_H

#include "../paumiot_core.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct state_context state_context_t;
typedef struct state_config state_config_t;
typedef struct session_entry session_entry_t;
typedef struct subscription_entry subscription_entry_t;
typedef struct protocol_state_entry protocol_state_entry_t;

/* Session State */
typedef enum {
    SESSION_STATE_CONNECTING = 0,
    SESSION_STATE_CONNECTED = 1,
    SESSION_STATE_ACTIVE = 2,
    SESSION_STATE_DISCONNECTING = 3,
    SESSION_STATE_DISCONNECTED = 4
} session_state_t;

/* Storage Backend Type */
typedef enum {
    STORAGE_MEMORY = 0,         /* In-memory only (fast, no persistence) */
    STORAGE_PERSISTENT = 1,     /* Persistent file-based storage */
    STORAGE_REDIS = 2          /* Distributed Redis storage */
} storage_backend_t;

/* Session Entry */
struct session_entry {
    char *session_id;               /* Unique session identifier */
    char *connection_id;            /* Associated connection ID */
    protocol_type_t protocol;       /* Protocol type */
    char *client_address;           /* Client IP address */
    session_state_t state;          /* Session state */
    uint64_t connected_at;          /* Connection timestamp */
    uint64_t last_activity;         /* Last activity timestamp */
    uint32_t keepalive_interval;    /* Keep-alive interval (ms) */
    void *protocol_data;            /* Protocol-specific data */
};

/* Subscription Entry */
struct subscription_entry {
    char *subscription_id;          /* Unique subscription identifier */
    char *session_id;               /* Associated session ID */
    char *topic_filter;             /* Topic filter or pattern */
    qos_level_t qos;                /* Desired QoS level */
    uint64_t subscribed_at;         /* Subscription timestamp */
    uint32_t message_count;         /* Number of messages delivered */
};

/* Protocol State Entry */
struct protocol_state_entry {
    char *session_id;               /* Associated session ID */
    protocol_type_t protocol;       /* Protocol type */
    uint16_t next_packet_id;        /* Next packet ID (MQTT) */
    uint16_t next_message_id;       /* Next message ID (CoAP) */
    void *protocol_specific_data;   /* Protocol-specific state */
};

/* State Configuration */
struct state_config {
    /* Storage Backend */
    storage_backend_t backend;      /* Storage backend type */
    const char *db_path;            /* Database path (for persistent) */
    const char *redis_host;         /* Redis host (for Redis backend) */
    uint16_t redis_port;            /* Redis port */
    
    /* Persistence */
    bool enable_persistence;        /* Enable write-ahead logging */
    uint32_t sync_interval_ms;      /* Sync interval for persistence */
    uint32_t snapshot_interval_ms;  /* Snapshot interval */
    
    /* Cache Settings */
    size_t session_cache_size;      /* Max sessions in cache */
    size_t subscription_cache_size; /* Max subscriptions in cache */
    uint32_t cache_ttl_ms;          /* Cache TTL */
    
    /* Cleanup */
    uint32_t cleanup_interval_ms;   /* Cleanup interval */
    uint32_t session_ttl_ms;        /* Session TTL (inactive) */
};

/* State Statistics */
typedef struct {
    uint64_t total_sessions;
    uint64_t active_sessions;
    uint64_t total_subscriptions;
    uint64_t active_subscriptions;
    uint64_t state_updates;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t persistence_ops;
    size_t memory_usage;
} state_stats_t;

/* ============================================================================
 * STATE MANAGEMENT API
 * ========================================================================= */

/**
 * @brief Initialize state management
 * @param config State configuration
 * @return State context or NULL on error
 */
state_context_t *state_init(const state_config_t *config);

/**
 * @brief Start state management (background tasks)
 * @param ctx State context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_start(state_context_t *ctx);

/**
 * @brief Stop state management
 * @param ctx State context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_stop(state_context_t *ctx);

/**
 * @brief Cleanup state management
 * @param ctx State context
 */
void state_cleanup(state_context_t *ctx);

/* ============================================================================
 * SESSION MANAGEMENT API
 * ========================================================================= */

/**
 * @brief Create a new session
 * @param ctx State context
 * @param session Session entry
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_session_create(
    state_context_t *ctx,
    const session_entry_t *session
);

/**
 * @brief Get session by ID
 * @param ctx State context
 * @param session_id Session identifier
 * @param session Session entry output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_session_get(
    state_context_t *ctx,
    const char *session_id,
    session_entry_t *session
);

/**
 * @brief Update session
 * @param ctx State context
 * @param session Session entry with updates
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_session_update(
    state_context_t *ctx,
    const session_entry_t *session
);

/**
 * @brief Delete session
 * @param ctx State context
 * @param session_id Session identifier
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_session_delete(
    state_context_t *ctx,
    const char *session_id
);

/**
 * @brief List all active sessions
 * @param ctx State context
 * @param sessions Array of session IDs (output, must be freed)
 * @param count Number of sessions (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_session_list(
    state_context_t *ctx,
    char ***sessions,
    size_t *count
);

/* ============================================================================
 * SUBSCRIPTION MANAGEMENT API
 * ========================================================================= */

/**
 * @brief Add subscription
 * @param ctx State context
 * @param subscription Subscription entry
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_subscription_add(
    state_context_t *ctx,
    const subscription_entry_t *subscription
);

/**
 * @brief Remove subscription
 * @param ctx State context
 * @param subscription_id Subscription identifier
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_subscription_remove(
    state_context_t *ctx,
    const char *subscription_id
);

/**
 * @brief Find subscriptions matching topic
 * @param ctx State context
 * @param topic Topic to match
 * @param subscriptions Array of matching subscriptions (output, must be freed)
 * @param count Number of matches (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_subscription_match(
    state_context_t *ctx,
    const char *topic,
    subscription_entry_t **subscriptions,
    size_t *count
);

/**
 * @brief Get subscriptions for session
 * @param ctx State context
 * @param session_id Session identifier
 * @param subscriptions Array of subscriptions (output, must be freed)
 * @param count Number of subscriptions (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_subscription_get_by_session(
    state_context_t *ctx,
    const char *session_id,
    subscription_entry_t **subscriptions,
    size_t *count
);

/* ============================================================================
 * PROTOCOL STATE API
 * ========================================================================= */

/**
 * @brief Set protocol state
 * @param ctx State context
 * @param state Protocol state entry
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_protocol_set(
    state_context_t *ctx,
    const protocol_state_entry_t *state
);

/**
 * @brief Get protocol state
 * @param ctx State context
 * @param session_id Session identifier
 * @param state Protocol state output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_protocol_get(
    state_context_t *ctx,
    const char *session_id,
    protocol_state_entry_t *state
);

/**
 * @brief Get and increment packet ID (atomic)
 * @param ctx State context
 * @param session_id Session identifier
 * @param packet_id Next packet ID (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_protocol_next_packet_id(
    state_context_t *ctx,
    const char *session_id,
    uint16_t *packet_id
);

/* ============================================================================
 * STATISTICS API
 * ========================================================================= */

/**
 * @brief Get state statistics
 * @param ctx State context
 * @param stats Statistics output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_get_stats(
    state_context_t *ctx,
    state_stats_t *stats
);

/**
 * @brief Reset statistics counters
 * @param ctx State context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t state_reset_stats(state_context_t *ctx);

/* ============================================================================
 * CONFIGURATION API
 * ========================================================================= */

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void state_config_init(state_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_STATE_MANAGEMENT_H */
