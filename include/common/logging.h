/**
 * @file logging.h
 * @brief Logging definitions for PaumIoT (stub for testing)
 */

#ifndef PAUMIOT_LOGGING_H
#define PAUMIOT_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* Log levels */
typedef enum {
    PAUMIOT_LOG_DEBUG = 0,
    PAUMIOT_LOG_INFO,
    PAUMIOT_LOG_WARNING,
    PAUMIOT_LOG_ERROR,
    PAUMIOT_LOG_CRITICAL
} paumiot_log_level_t;

/* Simple logging macros for testing */
#define PAUMIOT_LOG_DEBUG(fmt, ...)   printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define PAUMIOT_LOG_INFO(fmt, ...)    printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
#define PAUMIOT_LOG_WARNING(fmt, ...) printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
#define PAUMIOT_LOG_ERROR(fmt, ...)   printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define PAUMIOT_LOG_CRITICAL(fmt, ...) printf("[CRIT]  " fmt "\n", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_LOGGING_H */