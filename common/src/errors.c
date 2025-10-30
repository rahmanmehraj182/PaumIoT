#include "../include/errors.h"
#include <stddef.h>

/**
 * Get a human-readable error message for a result code
 */
const char *paumiot_error_string(paumiot_result_t code) {
    switch (code) {
        case PAUMIOT_SUCCESS:
            return "Success";
        
        /* Parameter errors */
        case PAUMIOT_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case PAUMIOT_ERROR_NULL_POINTER:
            return "Null pointer";
        
        /* Memory errors */
        case PAUMIOT_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case PAUMIOT_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        
        /* Queue errors */
        case PAUMIOT_ERROR_QUEUE_FULL:
            return "Queue is full";
        case PAUMIOT_ERROR_QUEUE_EMPTY:
            return "Queue is empty";
        
        /* State errors */
        case PAUMIOT_ERROR_NOT_FOUND:
            return "Not found";
        case PAUMIOT_ERROR_ALREADY_EXISTS:
            return "Already exists";
        case PAUMIOT_ERROR_NOT_INITIALIZED:
            return "Not initialized";
        case PAUMIOT_ERROR_INVALID_STATE:
            return "Invalid state";
        
        /* Connection errors */
        case PAUMIOT_ERROR_CONNECTION_LOST:
            return "Connection lost";
        case PAUMIOT_ERROR_CONNECTION_REFUSED:
            return "Connection refused";
        case PAUMIOT_ERROR_TIMEOUT:
            return "Timeout";
        
        /* Protocol errors */
        case PAUMIOT_ERROR_PROTOCOL_ERROR:
            return "Protocol error";
        case PAUMIOT_ERROR_PROTOCOL_VERSION:
            return "Protocol version not supported";
        case PAUMIOT_ERROR_MALFORMED_PACKET:
            return "Malformed packet";
        
        /* Operation errors */
        case PAUMIOT_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case PAUMIOT_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case PAUMIOT_ERROR_OPERATION_FAILED:
            return "Operation failed";
        
        /* Internal errors */
        case PAUMIOT_ERROR_INTERNAL:
            return "Internal error";
        case PAUMIOT_ERROR_UNKNOWN:
            return "Unknown error";
        
        default:
            return "Unrecognized error code";
    }
}
