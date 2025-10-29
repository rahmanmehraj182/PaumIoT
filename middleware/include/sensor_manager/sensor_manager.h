/**
 * @file sensor_manager.h
 * @brief Sensor Manager - Sensor registry and data management
 * @details Manages sensor registry, data caching, and aggregation
 */

#ifndef PAUMIOT_SENSOR_MANAGER_H
#define PAUMIOT_SENSOR_MANAGER_H

#include "../paumiot_core.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declarations */
typedef struct sensor_manager sensor_manager_t;
typedef struct sensor_manager_config sensor_manager_config_t;
typedef struct sensor_entry sensor_entry_t;
typedef struct sensor_data sensor_data_t;
typedef struct sensor_capabilities sensor_capabilities_t;

/* Sensor Type */
typedef enum {
    SENSOR_TYPE_TEMPERATURE = 0,
    SENSOR_TYPE_HUMIDITY = 1,
    SENSOR_TYPE_PRESSURE = 2,
    SENSOR_TYPE_LIGHT = 3,
    SENSOR_TYPE_MOTION = 4,
    SENSOR_TYPE_PROXIMITY = 5,
    SENSOR_TYPE_ACCELEROMETER = 6,
    SENSOR_TYPE_GYROSCOPE = 7,
    SENSOR_TYPE_GPS = 8,
    SENSOR_TYPE_CAMERA = 9,
    SENSOR_TYPE_CUSTOM = 100
} sensor_type_t;

/* Sensor Status */
typedef enum {
    SENSOR_STATUS_UNKNOWN = 0,
    SENSOR_STATUS_ONLINE = 1,
    SENSOR_STATUS_OFFLINE = 2,
    SENSOR_STATUS_ERROR = 3,
    SENSOR_STATUS_MAINTENANCE = 4
} sensor_status_t;

/* Data Format */
typedef enum {
    DATA_FORMAT_RAW = 0,        /* Raw binary data */
    DATA_FORMAT_JSON = 1,       /* JSON formatted */
    DATA_FORMAT_CBOR = 2,       /* CBOR encoded */
    DATA_FORMAT_PROTOBUF = 3    /* Protobuf encoded */
} data_format_t;

/* Sensor Capabilities */
struct sensor_capabilities {
    bool supports_streaming;        /* Supports continuous streaming */
    bool supports_polling;          /* Supports polling mode */
    bool supports_historical;       /* Has historical data */
    uint32_t min_interval_ms;       /* Minimum sampling interval */
    uint32_t max_interval_ms;       /* Maximum sampling interval */
    data_format_t data_format;      /* Data format */
    size_t max_payload_size;        /* Maximum payload size */
};

/* Sensor Entry */
struct sensor_entry {
    char *sensor_id;                /* Unique sensor identifier */
    char *name;                     /* Human-readable name */
    sensor_type_t type;             /* Sensor type */
    char *topic_pattern;            /* Topic pattern (e.g., "sensors/temp/{id}") */
    char *location;                 /* Physical location */
    sensor_status_t status;         /* Current status */
    sensor_capabilities_t capabilities; /* Capabilities */
    uint64_t last_update_ts;        /* Last update timestamp */
    void *metadata;                 /* Additional metadata */
};

/* Sensor Data */
struct sensor_data {
    char *sensor_id;                /* Source sensor ID */
    char *topic;                    /* Data topic */
    uint8_t *payload;               /* Data payload */
    size_t payload_len;             /* Payload length */
    data_format_t format;           /* Data format */
    uint64_t timestamp;             /* Data timestamp */
    qos_level_t qos;                /* QoS level */
};

/* Sensor Manager Configuration */
struct sensor_manager_config {
    /* Cache Settings */
    size_t cache_size;              /* Maximum cached entries */
    uint32_t cache_ttl_ms;          /* Cache TTL */
    bool enable_cache;              /* Enable caching */
    
    /* Data Aggregation */
    bool enable_aggregation;        /* Enable data aggregation */
    uint32_t aggregation_window_ms; /* Aggregation window */
    
    /* Health Monitoring */
    bool enable_health_monitoring;  /* Enable health monitoring */
    uint32_t health_check_interval_ms; /* Health check interval */
    uint32_t offline_threshold_ms;  /* Offline threshold */
    
    /* Storage */
    bool enable_historical;         /* Enable historical storage */
    const char *storage_path;       /* Storage path for historical data */
    uint32_t retention_days;        /* Data retention period */
};

/* Sensor Manager Statistics */
typedef struct {
    uint64_t total_sensors;
    uint64_t online_sensors;
    uint64_t offline_sensors;
    uint64_t data_updates;
    uint64_t cache_hits;
    uint64_t cache_misses;
    size_t cache_memory_usage;
    uint64_t health_checks;
} sensor_manager_stats_t;

/* Callback Types */

/**
 * @brief Callback for sensor data updates
 * @param sensor_id Sensor identifier
 * @param data Sensor data
 * @param user_data User-defined data
 */
typedef void (*sensor_data_callback_t)(
    const char *sensor_id,
    const sensor_data_t *data,
    void *user_data
);

/**
 * @brief Callback for sensor status changes
 * @param sensor_id Sensor identifier
 * @param old_status Previous status
 * @param new_status New status
 * @param user_data User-defined data
 */
typedef void (*sensor_status_callback_t)(
    const char *sensor_id,
    sensor_status_t old_status,
    sensor_status_t new_status,
    void *user_data
);

/* ============================================================================
 * SENSOR MANAGER API
 * ========================================================================= */

/**
 * @brief Initialize sensor manager
 * @param config Sensor manager configuration
 * @return Sensor manager instance or NULL on error
 */
sensor_manager_t *sensor_manager_init(const sensor_manager_config_t *config);

/**
 * @brief Start sensor manager (background tasks)
 * @param sm Sensor manager instance
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_start(sensor_manager_t *sm);

/**
 * @brief Stop sensor manager
 * @param sm Sensor manager instance
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_stop(sensor_manager_t *sm);

/**
 * @brief Cleanup sensor manager
 * @param sm Sensor manager instance
 */
void sensor_manager_cleanup(sensor_manager_t *sm);

/* ============================================================================
 * SENSOR REGISTRY API
 * ========================================================================= */

/**
 * @brief Register a sensor
 * @param sm Sensor manager instance
 * @param sensor Sensor entry
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_register(
    sensor_manager_t *sm,
    const sensor_entry_t *sensor
);

/**
 * @brief Unregister a sensor
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_unregister(
    sensor_manager_t *sm,
    const char *sensor_id
);

/**
 * @brief Get sensor information
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier
 * @param sensor Sensor entry output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_get_sensor(
    sensor_manager_t *sm,
    const char *sensor_id,
    sensor_entry_t *sensor
);

/**
 * @brief Update sensor status
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier
 * @param status New status
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_update_status(
    sensor_manager_t *sm,
    const char *sensor_id,
    sensor_status_t status
);

/**
 * @brief List all sensors
 * @param sm Sensor manager instance
 * @param sensors Array of sensor entries (output, must be freed)
 * @param count Number of sensors (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_list_sensors(
    sensor_manager_t *sm,
    sensor_entry_t **sensors,
    size_t *count
);

/**
 * @brief Find sensors matching topic
 * @param sm Sensor manager instance
 * @param topic Topic to match
 * @param sensors Array of matching sensors (output, must be freed)
 * @param count Number of matches (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_find_by_topic(
    sensor_manager_t *sm,
    const char *topic,
    sensor_entry_t **sensors,
    size_t *count
);

/* ============================================================================
 * DATA MANAGEMENT API
 * ========================================================================= */

/**
 * @brief Get sensor data (from cache or direct query)
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier
 * @param data Sensor data output (must be freed)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_get_data(
    sensor_manager_t *sm,
    const char *sensor_id,
    sensor_data_t **data
);

/**
 * @brief Update sensor data (cache and notify subscribers)
 * @param sm Sensor manager instance
 * @param data Sensor data
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_update_data(
    sensor_manager_t *sm,
    const sensor_data_t *data
);

/**
 * @brief Subscribe to sensor data updates
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier (or NULL for all sensors)
 * @param callback Callback for data updates
 * @param user_data User-defined data
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_subscribe_data(
    sensor_manager_t *sm,
    const char *sensor_id,
    sensor_data_callback_t callback,
    void *user_data
);

/**
 * @brief Subscribe to sensor status changes
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier (or NULL for all sensors)
 * @param callback Callback for status changes
 * @param user_data User-defined data
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_subscribe_status(
    sensor_manager_t *sm,
    const char *sensor_id,
    sensor_status_callback_t callback,
    void *user_data
);

/* ============================================================================
 * HISTORICAL DATA API
 * ========================================================================= */

/**
 * @brief Query historical sensor data
 * @param sm Sensor manager instance
 * @param sensor_id Sensor identifier
 * @param start_time Start timestamp (Unix epoch milliseconds)
 * @param end_time End timestamp
 * @param data Array of sensor data (output, must be freed)
 * @param count Number of data points (output)
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_query_historical(
    sensor_manager_t *sm,
    const char *sensor_id,
    uint64_t start_time,
    uint64_t end_time,
    sensor_data_t **data,
    size_t *count
);

/* ============================================================================
 * STATISTICS API
 * ========================================================================= */

/**
 * @brief Get sensor manager statistics
 * @param sm Sensor manager instance
 * @param stats Statistics output
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_get_stats(
    sensor_manager_t *sm,
    sensor_manager_stats_t *stats
);

/**
 * @brief Reset statistics counters
 * @param sm Sensor manager instance
 * @return PAUMIOT_SUCCESS on success, error code otherwise
 */
paumiot_result_t sensor_manager_reset_stats(sensor_manager_t *sm);

/* ============================================================================
 * CONFIGURATION API
 * ========================================================================= */

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void sensor_manager_config_init(sensor_manager_config_t *config);

/* ============================================================================
 * UTILITY API
 * ========================================================================= */

/**
 * @brief Free sensor entry
 * @param sensor Sensor entry to free
 */
void sensor_entry_free(sensor_entry_t *sensor);

/**
 * @brief Free sensor data
 * @param data Sensor data to free
 */
void sensor_data_free(sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_SENSOR_MANAGER_H */
