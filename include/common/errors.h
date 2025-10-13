/**
 * @file errors.h
 * @brief Error codes and error handling for PaumIoT middleware
 */

#ifndef PAUMIOT_COMMON_ERRORS_H
#define PAUMIOT_COMMON_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Result type for function returns */
typedef enum {
    PAUMIOT_SUCCESS = 0,
    
    /* General errors */
    PAUMIOT_ERROR_GENERAL = -1000,
    PAUMIOT_ERROR_INVALID_PARAM,
    PAUMIOT_ERROR_NULL_POINTER,
    PAUMIOT_ERROR_OUT_OF_MEMORY,
    PAUMIOT_ERROR_BUFFER_OVERFLOW,
    PAUMIOT_ERROR_TIMEOUT,
    PAUMIOT_ERROR_NOT_INITIALIZED,
    PAUMIOT_ERROR_ALREADY_INITIALIZED,
    PAUMIOT_ERROR_NOT_SUPPORTED,
    PAUMIOT_ERROR_PERMISSION_DENIED,
    
    /* Network errors */
    PAUMIOT_ERROR_NETWORK = -2000,
    PAUMIOT_ERROR_CONNECTION_FAILED,
    PAUMIOT_ERROR_CONNECTION_LOST,
    PAUMIOT_ERROR_CONNECTION_REFUSED,
    PAUMIOT_ERROR_SOCKET_ERROR,
    PAUMIOT_ERROR_BIND_FAILED,
    PAUMIOT_ERROR_LISTEN_FAILED,
    PAUMIOT_ERROR_ACCEPT_FAILED,
    
    /* Protocol errors */
    PAUMIOT_ERROR_PROTOCOL = -3000,
    PAUMIOT_ERROR_PROTOCOL_UNKNOWN,
    PAUMIOT_ERROR_PROTOCOL_UNSUPPORTED,
    PAUMIOT_ERROR_PACKET_MALFORMED,
    PAUMIOT_ERROR_PACKET_TOO_LARGE,
    PAUMIOT_ERROR_INVALID_PACKET_TYPE,
    PAUMIOT_ERROR_PROTOCOL_VIOLATION,
    
    /* MQTT specific errors */
    PAUMIOT_ERROR_MQTT = -4000,
    PAUMIOT_ERROR_MQTT_INVALID_PROTOCOL,
    PAUMIOT_ERROR_MQTT_INVALID_CLIENT_ID,
    PAUMIOT_ERROR_MQTT_INVALID_TOPIC,
    PAUMIOT_ERROR_MQTT_INVALID_QOS,
    PAUMIOT_ERROR_MQTT_SUBSCRIPTION_FAILED,
    PAUMIOT_ERROR_MQTT_PUBLISH_FAILED,
    PAUMIOT_ERROR_MQTT_AUTH_FAILED,
    
    /* CoAP specific errors */
    PAUMIOT_ERROR_COAP = -5000,
    PAUMIOT_ERROR_COAP_INVALID_METHOD,
    PAUMIOT_ERROR_COAP_INVALID_OPTION,
    PAUMIOT_ERROR_COAP_INVALID_URI,
    PAUMIOT_ERROR_COAP_BLOCKWISE_ERROR,
    PAUMIOT_ERROR_COAP_OBSERVE_ERROR,
    
    /* Data layer errors */
    PAUMIOT_ERROR_DATA = -6000,
    PAUMIOT_ERROR_DATABASE_CONNECTION,
    PAUMIOT_ERROR_DATABASE_QUERY,
    PAUMIOT_ERROR_SENSOR_ERROR,
    PAUMIOT_ERROR_DEVICE_NOT_FOUND,
    PAUMIOT_ERROR_DATA_VALIDATION,
    
    /* Security errors */
    PAUMIOT_ERROR_SECURITY = -7000,
    PAUMIOT_ERROR_TLS_INIT,
    PAUMIOT_ERROR_TLS_HANDSHAKE,
    PAUMIOT_ERROR_CERTIFICATE_ERROR,
    PAUMIOT_ERROR_AUTH_FAILED,
    PAUMIOT_ERROR_ENCRYPTION_FAILED,
    
    /* Configuration errors */
    PAUMIOT_ERROR_CONFIG = -8000,
    PAUMIOT_ERROR_CONFIG_FILE_NOT_FOUND,
    PAUMIOT_ERROR_CONFIG_PARSE_ERROR,
    PAUMIOT_ERROR_CONFIG_VALIDATION_ERROR,
    
    /* Layer specific errors */
    PAUMIOT_ERROR_SMP = -9000,
    PAUMIOT_ERROR_PDL = -10000,
    PAUMIOT_ERROR_PAL = -11000,
    PAUMIOT_ERROR_BLL = -12000,
    PAUMIOT_ERROR_DAL = -13000
    
} paumiot_result_t;

/**
 * @brief Convert error code to human-readable string
 * 
 * @param error_code Error code
 * @return Error description string
 */
const char *paumiot_error_string(paumiot_result_t error_code);

/**
 * @brief Check if result indicates success
 * 
 * @param result Result code
 * @return true if success, false otherwise
 */
static inline bool paumiot_is_success(paumiot_result_t result) {
    return result == PAUMIOT_SUCCESS;
}

/**
 * @brief Check if result indicates failure
 * 
 * @param result Result code
 * @return true if failure, false otherwise
 */
static inline bool paumiot_is_error(paumiot_result_t result) {
    return result != PAUMIOT_SUCCESS;
}

/* Error handling macros */
#define PAUMIOT_CHECK_NULL(ptr) \
    do { if (!(ptr)) return PAUMIOT_ERROR_NULL_POINTER; } while(0)

#define PAUMIOT_CHECK_RESULT(result) \
    do { if (paumiot_is_error(result)) return result; } while(0)

#define PAUMIOT_RETURN_IF_NULL(ptr, ret) \
    do { if (!(ptr)) return ret; } while(0)

#define PAUMIOT_GOTO_IF_ERROR(result, label) \
    do { if (paumiot_is_error(result)) goto label; } while(0)

/* Debug assertions */
#ifdef DEBUG
#include <assert.h>
#define PAUMIOT_ASSERT(condition) assert(condition)
#else
#define PAUMIOT_ASSERT(condition) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_COMMON_ERRORS_H */