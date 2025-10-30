/**
 * @file config.h
 * @brief Configuration definitions for PaumIoT
 */

#ifndef PAUMIOT_CONFIG_H
#define PAUMIOT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Main configuration structure for PaumIoT
 */
struct paumiot_config {
    /* Protocol Adaptation Layer config */
    struct {
        int max_adapters;
        size_t max_message_size;
        int timeout_ms;
    } pal;
    
    /* Network configuration */
    struct {
        int mqtt_port;
        int coap_port;
        const char *bind_address;
    } network;
    
    /* Logging configuration */
    struct {
        int log_level;
        const char *log_file;
    } logging;
    
};

/* Default configuration values */
#define PAUMIOT_DEFAULT_MAX_ADAPTERS 16
#define PAUMIOT_DEFAULT_MAX_MESSAGE_SIZE 4096
#define PAUMIOT_DEFAULT_TIMEOUT_MS 5000
#define PAUMIOT_DEFAULT_MQTT_PORT 1883
#define PAUMIOT_DEFAULT_COAP_PORT 5683

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_CONFIG_H */