#ifndef PAUMIOT_LOGGING_H
#define PAUMIOT_LOGGING_H

#include <stdio.h>
#include <stdbool.h>

/**
 * Log levels
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4  /* Disable all logging */
} log_level_t;

/**
 * Set the global log level
 * Only messages at or above this level will be logged
 */
void log_set_level(log_level_t level);

/**
 * Get the current global log level
 */
log_level_t log_get_level(void);

/**
 * Internal logging function (not called directly)
 */
void log_message(log_level_t level, const char *file, int line, 
                 const char *fmt, ...) __attribute__((format(printf, 4, 5)));

/**
 * Logging macros with file and line information
 */
#define LOG_DEBUG(fmt, ...) \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* PAUMIOT_LOGGING_H */
