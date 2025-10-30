/**
 * @file errors.c
 * @brief Error handling implementation for PaumIoT middleware
 */

#include "common/errors.h"

/**
 * @brief Convert error code to human-readable string
 */
const char *paumiot_error_string(paumiot_result_t error_code) {
    switch (error_code) {
        case PAUMIOT_SUCCESS:
            return "Success";
            
        /* General errors */
        case PAUMIOT_ERROR_GENERAL:
            return "General error";
        case PAUMIOT_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case PAUMIOT_ERROR_NULL_POINTER:
            return "Null pointer";
        case PAUMIOT_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case PAUMIOT_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case PAUMIOT_ERROR_TIMEOUT:
            return "Timeout";
        case PAUMIOT_ERROR_NOT_INITIALIZED:
            return "Not initialized";
        case PAUMIOT_ERROR_ALREADY_INITIALIZED:
            return "Already initialized";
        case PAUMIOT_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case PAUMIOT_ERROR_PERMISSION_DENIED:
            return "Permission denied";
            
        /* Network errors */
        case PAUMIOT_ERROR_NETWORK:
            return "Network error";
        case PAUMIOT_ERROR_CONNECTION_FAILED:
            return "Connection failed";
        case PAUMIOT_ERROR_CONNECTION_LOST:
            return "Connection lost";
        case PAUMIOT_ERROR_CONNECTION_REFUSED:
            return "Connection refused";
        case PAUMIOT_ERROR_SOCKET_ERROR:
            return "Socket error";
        case PAUMIOT_ERROR_BIND_FAILED:
            return "Bind failed";
        case PAUMIOT_ERROR_LISTEN_FAILED:
            return "Listen failed";
        case PAUMIOT_ERROR_ACCEPT_FAILED:
            return "Accept failed";
            
        /* Protocol errors */
        case PAUMIOT_ERROR_PROTOCOL:
            return "Protocol error";
        case PAUMIOT_ERROR_PROTOCOL_UNKNOWN:
            return "Unknown protocol";
        case PAUMIOT_ERROR_PROTOCOL_UNSUPPORTED:
            return "Unsupported protocol";
        case PAUMIOT_ERROR_PACKET_MALFORMED:
            return "Malformed packet";
        case PAUMIOT_ERROR_PACKET_TOO_LARGE:
            return "Packet too large";
        case PAUMIOT_ERROR_INVALID_PACKET_TYPE:
            return "Invalid packet type";
        case PAUMIOT_ERROR_PROTOCOL_VIOLATION:
            return "Protocol violation";
            
        /* MQTT specific errors */
        case PAUMIOT_ERROR_MQTT:
            return "MQTT error";
        case PAUMIOT_ERROR_MQTT_INVALID_PROTOCOL:
            return "Invalid MQTT protocol";
        case PAUMIOT_ERROR_MQTT_INVALID_CLIENT_ID:
            return "Invalid MQTT client ID";
        case PAUMIOT_ERROR_MQTT_INVALID_TOPIC:
            return "Invalid MQTT topic";
        case PAUMIOT_ERROR_MQTT_INVALID_QOS:
            return "Invalid MQTT QoS";
        case PAUMIOT_ERROR_MQTT_SUBSCRIPTION_FAILED:
            return "MQTT subscription failed";
        case PAUMIOT_ERROR_MQTT_PUBLISH_FAILED:
            return "MQTT publish failed";
        case PAUMIOT_ERROR_MQTT_AUTH_FAILED:
            return "MQTT authentication failed";
            
        /* CoAP specific errors */
        case PAUMIOT_ERROR_COAP:
            return "CoAP error";
        case PAUMIOT_ERROR_COAP_INVALID_METHOD:
            return "Invalid CoAP method";
        case PAUMIOT_ERROR_COAP_INVALID_OPTION:
            return "Invalid CoAP option";
        case PAUMIOT_ERROR_COAP_INVALID_URI:
            return "Invalid CoAP URI";
        case PAUMIOT_ERROR_COAP_BLOCKWISE_ERROR:
            return "CoAP blockwise error";
        case PAUMIOT_ERROR_COAP_OBSERVE_ERROR:
            return "CoAP observe error";
            
        /* Data layer errors */
        case PAUMIOT_ERROR_DATA:
            return "Data error";
        case PAUMIOT_ERROR_DATABASE_CONNECTION:
            return "Database connection error";
        case PAUMIOT_ERROR_DATABASE_QUERY:
            return "Database query error";
        case PAUMIOT_ERROR_SENSOR_ERROR:
            return "Sensor error";
        case PAUMIOT_ERROR_DEVICE_NOT_FOUND:
            return "Device not found";
        case PAUMIOT_ERROR_DATA_VALIDATION:
            return "Data validation error";
            
        /* Security errors */
        case PAUMIOT_ERROR_SECURITY:
            return "Security error";
        case PAUMIOT_ERROR_TLS_INIT:
            return "TLS initialization error";
        case PAUMIOT_ERROR_TLS_HANDSHAKE:
            return "TLS handshake error";
        case PAUMIOT_ERROR_CERTIFICATE_ERROR:
            return "Certificate error";
        case PAUMIOT_ERROR_AUTH_FAILED:
            return "Authentication failed";
        case PAUMIOT_ERROR_ENCRYPTION_FAILED:
            return "Encryption failed";
            
        /* Configuration errors */
        case PAUMIOT_ERROR_CONFIG:
            return "Configuration error";
        case PAUMIOT_ERROR_CONFIG_FILE_NOT_FOUND:
            return "Configuration file not found";
        case PAUMIOT_ERROR_CONFIG_PARSE_ERROR:
            return "Configuration parse error";
        case PAUMIOT_ERROR_CONFIG_VALIDATION_ERROR:
            return "Configuration validation error";
            
        /* Layer specific errors */
        case PAUMIOT_ERROR_SMP:
            return "State Management Plane error";
        case PAUMIOT_ERROR_PDL:
            return "Protocol Demultiplexing Layer error";
        case PAUMIOT_ERROR_PAL:
            return "Protocol Adaptation Layer error";
        case PAUMIOT_ERROR_BLL:
            return "Business Logic Layer error";
        case PAUMIOT_ERROR_DAL:
            return "Data Acquisition Layer error";
            
        default:
            return "Unknown error";
    }
}