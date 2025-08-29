#include "include/core/server_core.h"
#include <signal.h>
#include <stdio.h>

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nShutdown signal received. Gracefully shutting down...\n");
        g_running = 0;
    }
}

int main(int argc, char* argv[]) {
    (void)argc;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("PaumIoT - Protocol-Agnostic Universal Middleware for IoT\n");
    printf("========================================================\n");
    printf("Multi-Protocol Server Supporting HTTP, MQTT, CoAP, DNS\n");
    printf("Features: Event-driven architecture, Congestion control, Session management\n\n");
    
    // Initialize server
    if (server_init() != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    // Run server
    server_run();
    
    // Cleanup
    server_cleanup();
    
    return 0;
}
