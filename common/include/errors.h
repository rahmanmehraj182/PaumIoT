#ifndef PAUMIOT_ERRORS_H
#define PAUMIOT_ERRORS_H

#include "types.h"

/**
 * Get a human-readable error message for a result code
 * 
 * @param code The result code
 * @return A string describing the error, never NULL
 */
const char *paumiot_error_string(paumiot_result_t code);

/**
 * Check if a result code indicates success
 * 
 * @param code The result code to check
 * @return true if successful, false otherwise
 */
static inline bool paumiot_is_success(paumiot_result_t code) {
    return code == PAUMIOT_SUCCESS;
}

/**
 * Check if a result code indicates an error
 * 
 * @param code The result code to check
 * @return true if error, false otherwise
 */
static inline bool paumiot_is_error(paumiot_result_t code) {
    return code < 0;
}

#endif /* PAUMIOT_ERRORS_H */
