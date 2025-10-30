/**
 * @file mqtt_to_coap_bridge.c
 * @brief Example application demonstrating MQTT to CoAP message bridging
 * 
 * This example shows how to use PaumIoT middleware to bridge MQTT and CoAP
 * protocols, converting messages between the two formats.
 */

#include "../include/paumiot.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static volatile bool running = true;

/* Signal handler for graceful shutdown */
void signal_handler(int signal) {
    (void)signal;
    printf("\nShutdown requested...\n");
    running = false;
}

/* Message handler callback */
void message_handler(const message_t *msg, void *user_data) {
    (void)user_data;
    
    printf("Received message:\n");
    printf("  Source: %s\n", msg->source ? msg->source : "unknown");
    printf("  Destination: %s\n", msg->destination ? msg->destination : "unknown");
    printf("  Protocol: %s\n", 
           msg->metadata.protocol == PROTOCOL_TYPE_MQTT ? "MQTT" :
           msg->metadata.protocol == PROTOCOL_TYPE_COAP ? "CoAP" : "Unknown");
    printf("  QoS: %d\n", msg->metadata.qos);
    printf("  Payload (%zu bytes): %.*s\n", 
           msg->payload_len, (int)msg->payload_len, msg->payload);
    printf("\n");
}

/* Error handler callback */
void error_handler(int error_code, const char *error_msg, void *user_data) {
    (void)user_data;
    printf("Error %d: %s\n", error_code, error_msg);
}

int main(int argc, char *argv[]) {
    printf("PaumIoT MQTT-CoAP Bridge Example\n");
    printf("===============================\n\n");
    
    /* Set up signal handling */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize configuration with defaults */
    paumiot_config_t config = {
        /* Network configuration */
        .network = {
            .bind_address = "0.0.0.0",
            .mqtt_port = 1883,
            .coap_port = 5683,
            .max_connections = 100
        },
        
        /* Enable both MQTT and CoAP */
        .protocols = {
            .mqtt_enabled = true,
            .coap_enabled = true
        },
        
        /* Logging configuration */
        .logging = {
            .level = LOG_LEVEL_INFO,
            .console_output = true
        },
        
        /* Bridge configuration */
        .bridge = {
            .enabled = true,
            .mqtt_to_coap = true,
            .coap_to_mqtt = true
        }
    };
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mqtt-port") == 0 && i + 1 < argc) {
            config.network.mqtt_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--coap-port") == 0 && i + 1 < argc) {
            config.network.coap_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--debug") == 0) {
            config.logging.level = LOG_LEVEL_DEBUG;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --mqtt-port PORT    MQTT port (default: 1883)\n");
            printf("  --coap-port PORT    CoAP port (default: 5683)\n");
            printf("  --debug            Enable debug logging\n");
            printf("  --help             Show this help\n");
            return 0;
        }
    }
    
    printf("Configuration:\n");
    printf("  MQTT Port: %d\n", config.network.mqtt_port);
    printf("  CoAP Port: %d\n", config.network.coap_port);
    printf("  Log Level: %s\n", 
           config.logging.level == LOG_LEVEL_DEBUG ? "DEBUG" : "INFO");
    printf("\n");
    
    /* Initialize the middleware */
    printf("Initializing PaumIoT middleware...\n");
    paumiot_context_t *ctx = paumiot_init(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to initialize PaumIoT middleware\n");
        return 1;
    }
    
    /* Register message and error handlers */
    // Note: These would be actual API calls in the real implementation
    // paumiot_set_message_handler(ctx, message_handler, NULL);
    // paumiot_set_error_handler(ctx, error_handler, NULL);
    
    /* Start the middleware */
    printf("Starting middleware services...\n");
    paumiot_result_t result = paumiot_start(ctx);
    if (result != PAUMIOT_SUCCESS) {
        fprintf(stderr, "Failed to start middleware: %s\n", 
                paumiot_error_string(result));
        paumiot_cleanup(ctx);
        return 1;
    }
    
    printf("PaumIoT bridge is running!\n");
    printf("- MQTT broker listening on port %d\n", config.network.mqtt_port);
    printf("- CoAP server listening on port %d\n", config.network.coap_port);
    printf("- Message bridging enabled between protocols\n");
    printf("Press Ctrl+C to stop...\n\n");
    
    /* Example: Send a test message */
    printf("Sending test message...\n");
    message_t *test_msg = message_create();
    if (test_msg) {
        test_msg->source = strdup("bridge_example");
        test_msg->destination = strdup("test/topic");
        message_set_payload(test_msg, (const uint8_t*)"Hello from PaumIoT!", 19);
        test_msg->metadata.protocol = PROTOCOL_TYPE_MQTT;
        test_msg->metadata.qos = QOS_LEVEL_0;
        
        /* In real implementation, this would send through the middleware */
        message_handler(test_msg, NULL);
        message_free(test_msg);
    }
    
    /* Main event loop */
    while (running) {
        /* In a real implementation, this would process events */
        sleep(1);
        
        /* Print periodic status */
        static int counter = 0;
        if (++counter % 30 == 0) {
            printf("Bridge status: Running (uptime: %d seconds)\n", counter);
        }
    }
    
    /* Shutdown sequence */
    printf("\nShutting down PaumIoT middleware...\n");
    
    result = paumiot_stop(ctx);
    if (result != PAUMIOT_SUCCESS) {
        fprintf(stderr, "Warning: Error during shutdown: %s\n",
                paumiot_error_string(result));
    }
    
    paumiot_cleanup(ctx);
    printf("Shutdown complete.\n");
    
    return 0;
}

/* Example usage instructions:

Compile:
gcc -I../include examples/mqtt_to_coap_bridge.c -L../build -lpaumiot -o bridge

Run:
./bridge --mqtt-port 1883 --coap-port 5683

Test with MQTT:
mosquitto_pub -h localhost -t "test/topic" -m "Hello MQTT"

Test with CoAP:
echo -n "Hello CoAP" | coap-client -m put coap://localhost/test/topic -f -

The bridge will convert messages between the protocols automatically.

*/