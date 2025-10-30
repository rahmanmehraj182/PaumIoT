#ifndef PAUMIOT_TYPES_H
#define PAUMIOT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Result codes - all errors are negative, success is 0
 */
typedef enum {
    PAUMIOT_SUCCESS = 0,
    
    /* Parameter errors */
    PAUMIOT_ERROR_INVALID_PARAM = -1,
    PAUMIOT_ERROR_NULL_POINTER = -2,
    
    /* Memory errors */
    PAUMIOT_ERROR_OUT_OF_MEMORY = -3,
    PAUMIOT_ERROR_BUFFER_TOO_SMALL = -4,
    
    /* Queue errors */
    PAUMIOT_ERROR_QUEUE_FULL = -5,
    PAUMIOT_ERROR_QUEUE_EMPTY = -6,
    
    /* State errors */
    PAUMIOT_ERROR_NOT_FOUND = -7,
    PAUMIOT_ERROR_ALREADY_EXISTS = -8,
    PAUMIOT_ERROR_NOT_INITIALIZED = -9,
    PAUMIOT_ERROR_INVALID_STATE = -10,
    
    /* Connection errors */
    PAUMIOT_ERROR_CONNECTION_LOST = -11,
    PAUMIOT_ERROR_CONNECTION_REFUSED = -12,
    PAUMIOT_ERROR_TIMEOUT = -13,
    
    /* Protocol errors */
    PAUMIOT_ERROR_PROTOCOL_ERROR = -14,
    PAUMIOT_ERROR_PROTOCOL_VERSION = -15,
    PAUMIOT_ERROR_MALFORMED_PACKET = -16,
    
    /* Operation errors */
    PAUMIOT_ERROR_NOT_SUPPORTED = -17,
    PAUMIOT_ERROR_PERMISSION_DENIED = -18,
    PAUMIOT_ERROR_OPERATION_FAILED = -19,
    
    /* Internal errors */
    PAUMIOT_ERROR_INTERNAL = -20,
    PAUMIOT_ERROR_UNKNOWN = -21
} paumiot_result_t;

/**
 * Protocol types supported by the middleware
 */
typedef enum {
    PROTOCOL_TYPE_UNKNOWN = 0,
    PROTOCOL_TYPE_MQTT = 1,
    PROTOCOL_TYPE_COAP = 2,
    PROTOCOL_TYPE_HTTP = 3
} protocol_type_t;

/**
 * QoS (Quality of Service) levels
 */
typedef enum {
    QOS_LEVEL_0 = 0,  /* At most once - fire and forget */
    QOS_LEVEL_1 = 1,  /* At least once - acknowledged delivery */
    QOS_LEVEL_2 = 2   /* Exactly once - assured delivery */
} qos_level_t;

/**
 * Operation types for internal message routing
 */
typedef enum {
    OP_TYPE_CONNECT = 1,
    OP_TYPE_DISCONNECT = 2,
    OP_TYPE_PUBLISH = 3,
    OP_TYPE_SUBSCRIBE = 4,
    OP_TYPE_UNSUBSCRIBE = 5,
    OP_TYPE_PING = 6,
    OP_TYPE_RESPONSE = 7
} operation_type_t;

/**
 * Session states
 */
typedef enum {
    SESSION_STATE_DISCONNECTED = 0,
    SESSION_STATE_CONNECTING = 1,
    SESSION_STATE_CONNECTED = 2,
    SESSION_STATE_ACTIVE = 3,
    SESSION_STATE_DISCONNECTING = 4,
    SESSION_STATE_ERROR = 5
} session_state_t;

/**
 * Sensor health states
 */
typedef enum {
    SENSOR_STATE_UNKNOWN = 0,
    SENSOR_STATE_ONLINE = 1,
    SENSOR_STATE_OFFLINE = 2,
    SENSOR_STATE_ERROR = 3,
    SENSOR_STATE_DEGRADED = 4
} sensor_state_t;

#endif /* PAUMIOT_TYPES_H */
