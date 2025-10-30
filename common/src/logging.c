#include "../include/logging.h"
#include <stdarg.h>
#include <time.h>
#include <string.h>

/* Global log level - default to INFO */
static log_level_t global_log_level = LOG_LEVEL_INFO;

/**
 * Set the global log level
 */
void log_set_level(log_level_t level) {
    global_log_level = level;
}

/**
 * Get the current global log level
 */
log_level_t log_get_level(void) {
    return global_log_level;
}

/**
 * Get the string representation of a log level
 */
static const char *log_level_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_NONE:  return "NONE";
        default:              return "UNKNOWN";
    }
}

/**
 * Get just the filename from a full path
 */
static const char *basename_simple(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }
    return path;
}

/**
 * Format a timestamp
 */
static void format_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * Log a message with level, file, and line information
 */
void log_message(log_level_t level, const char *file, int line, 
                 const char *fmt, ...) {
    /* Check if we should log this message */
    if (level < global_log_level || level == LOG_LEVEL_NONE) {
        return;
    }
    
    /* Format timestamp */
    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp));
    
    /* Get just the filename */
    const char *filename = basename_simple(file);
    
    /* Print the log header */
    fprintf(stderr, "[%s] [%5s] [%s:%d] ", 
            timestamp,
            log_level_string(level),
            filename,
            line);
    
    /* Print the actual message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fflush(stderr);
}
