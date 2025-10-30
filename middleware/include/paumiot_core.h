/**
 * @file paumiot_core.h
 * @brief PaumIoT Middleware Core API
 * @details Main entry point and coordination for the middleware
 */

#ifndef PAUMIOT_CORE_H
#define PAUMIOT_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct paumiot_context paumiot_context_t;
typedef struct paumiot_config paumiot_config_t;

/* Result Codes */
typedef enum {
    PAUMIOT_SUCCESS = 0,
    PAUMIOT_ERROR_INVALID_PARAM = -1,
    PAUMIOT_ERROR_OUT_OF_MEMORY = -2,
    PAUMIOT_ERROR_NOT_INITIALIZED = -3,
    PAUMIOT_ERROR_ALREADY_INITIALIZED = -4,
    PAUMIOT_ERROR_OPERATION_FAILED = -5,
    PAUMIOT_ERROR_TIMEOUT = -6,
    PAUMIOT_ERROR_NOT_SUPPORTED = -7,
    
    /* Layer-specific errors */
    PAUMIOT_ERROR_INITIATOR_BASE = -1000,
    PAUMIOT_ERROR_PAL_BASE = -2000,
    PAUMIOT_ERROR_ENGINE_BASE = -3000,
    PAUMIOT_ERROR_SENSOR_BASE = -4000,
    PAUMIOT_ERROR_STATE_BASE = -5000
} paumiot_result_t;

/* Protocol Types */
typedef enum {
    PROTOCOL_TYPE_UNKNOWN = 0,
    PROTOCOL_TYPE_MQTT = 1,
    PROTOCOL_TYPE_COAP = 2,
    PROTOCOL_TYPE_HTTP = 3
} protocol_type_t;

/* QoS Levels */
typedef enum {
    QOS_LEVEL_0 = 0,    /* At most once */
    QOS_LEVEL_1 = 1,    /* At least once */
    QOS_LEVEL_2 = 2     /* Exactly once */
} qos_level_t;

/* Operation Types */
typedef enum {
    OPERATION_SUBSCRIBE = 0,
    OPERATION_UNSUBSCRIBE = 1,
    OPERATION_PUBLISH = 2,
    OPERATION_GET = 3,      /* CoAP GET */
    OPERATION_POST = 4,     /* CoAP POST */
    OPERATION_PUT = 5,      /* CoAP PUT */
    OPERATION_DELETE = 6    /* CoAP DELETE */
} operation_type_t;

/* Server Configuration */
struct paumiot_config {
    /* Network Settings */
    const char *host;
    uint16_t mqtt_port;
    uint16_t coap_port;
    uint16_t http_port;
    
    /* Threading */
    uint32_t io_threads;
    uint32_t worker_threads;
    
    /* Engine Settings */
    uint32_t max_queue_size;
    uint32_t request_timeout_ms;
    
    /* State Management */
    const char *state_backend;      /* "memory", "redis", "persistent" */
    bool state_persistence;
    const char *state_db_path;
    
    /* Sensor Manager */
    uint32_t sensor_cache_size;
    uint32_t sensor_cache_ttl_ms;
    
    /* Rate Limiting */
    uint32_t global_rate_limit;     /* Requests per second */
    uint32_t per_client_rate_limit; /* Requests per second per client */
    
    /* Logging */
    const char *log_level;          /* "debug", "info", "warn", "error" */
    const char *log_format;         /* "text", "json" */
    const char *log_file;
    
    /* TLS/DTLS */
    bool enable_tls;
    const char *cert_file;
    const char *key_file;
    const char *ca_file;
};

/* Statistics */
typedef struct {
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t active_sessions;
    uint64_t active_subscriptions;
    uint64_t errors;
    uint64_t dropped_messages;
} paumiot_stats_t;

/* ============================================================================
 * MIDDLEWARE LIFECYCLE API
 * ========================================================================= */

/**
 * @brief Initialize middleware with configuration
 * @param config Middleware configuration
 * @return Middleware context or NULL on error
 */
paumiot_context_t *paumiot_init(const paumiot_config_t *config);

/**
 * @brief Start middleware (begin accepting connections)
 * @param ctx Middleware context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t paumiot_start(paumiot_context_t *ctx);

/**
 * @brief Stop middleware (graceful shutdown)
 * @param ctx Middleware context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t paumiot_stop(paumiot_context_t *ctx);

/**
 * @brief Cleanup middleware and free resources
 * @param ctx Middleware context
 */
void paumiot_cleanup(paumiot_context_t *ctx);

/* ============================================================================
 * MONITORING API
 * ========================================================================= */

/**
 * @brief Get middleware statistics
 * @param ctx Middleware context
 * @param stats Statistics output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t paumiot_get_stats(paumiot_context_t *ctx, paumiot_stats_t *stats);

/**
 * @brief Reset statistics counters
 * @param ctx Middleware context
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t paumiot_reset_stats(paumiot_context_t *ctx);

/* ============================================================================
 * CONFIGURATION API
 * ========================================================================= */

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void paumiot_config_init(paumiot_config_t *config);

/**
 * @brief Load configuration from YAML file
 * @param path Path to configuration file
 * @param config Configuration output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t paumiot_config_load(const char *path, paumiot_config_t *config);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return PAUMIOT_SUCCESS if valid, error code otherwise
 */
paumiot_result_t paumiot_config_validate(const paumiot_config_t *config);

/* ============================================================================
 * UTILITY API
 * ========================================================================= */

/**
 * @brief Get error string for result code
 * @param result Result code
 * @return Human-readable error string
 */
const char *paumiot_error_string(paumiot_result_t result);

/**
 * @brief Get middleware version
 * @param major Major version (output)
 * @param minor Minor version (output)
 * @param patch Patch version (output)
 */
void paumiot_get_version(int *major, int *minor, int *patch);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_CORE_H */
