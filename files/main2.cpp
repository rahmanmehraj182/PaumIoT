#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_SESSION_ID 64
#define SERVER_PORT 8080

// Protocol types
typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2
} protocol_type_t;

// Client session state
typedef enum {
    SESSION_CONNECTED = 0,
    SESSION_AUTHENTICATED = 1,
    SESSION_ACTIVE = 2,
    SESSION_DISCONNECTING = 3,
    SESSION_CLOSED = 4
} session_state_t;

// Session flags using efficient bit manipulation (better than bitfields)
typedef struct {
    uint32_t flags;  // All flags in one atomic uint32_t
    // Bit layout:
    // 0: is_active
    // 1: authenticated  
    // 2: keep_alive_enabled
    // 3: clean_session (MQTT)
    // 4: observe_active (CoAP)
    // 5-7: reserved
    // 8-15: protocol_specific_flags
    // 16-31: reserved for future use
} session_flags_t;

// Flag constants for easy manipulation
#define SESSION_FLAG_ACTIVE         (1U << 0)
#define SESSION_FLAG_AUTHENTICATED  (1U << 1) 
#define SESSION_FLAG_KEEPALIVE      (1U << 2)
#define SESSION_FLAG_CLEAN_SESSION  (1U << 3)
#define SESSION_FLAG_OBSERVE_ACTIVE (1U << 4)

// Protocol-specific data
typedef union {
    struct {
        uint8_t qos_level;
        uint16_t keep_alive_interval;
        uint8_t protocol_version;
        char client_id[32];
    } mqtt;
    
    struct {
        uint16_t message_id_counter;
        uint8_t token[8];
        uint8_t token_length;
        uint32_t observe_sequence;
    } coap;
    
    uint8_t raw_data[64];  // Ensure consistent size
} protocol_data_t;

// Optimized client session structure 
typedef struct {
    char session_id[MAX_SESSION_ID];
    int socket_fd;
    struct sockaddr_in client_addr;
    protocol_type_t protocol;
    session_state_t state;
    session_flags_t session_flags;     // Atomic flag operations
    protocol_data_t protocol_data;     // Protocol-specific data
    
    // Timing and counters (cache-line aligned)
    time_t last_activity;
    time_t created_at;
    uint32_t message_count;
    uint32_t error_count;
    
    pthread_mutex_t session_mutex;
    
    // Performance: pad to cache line boundary (64 bytes)
    char padding[0] __attribute__((aligned(64)));
} client_session_t;

// STATE MANAGEMENT TABLE - Connection state table
typedef struct {
    client_session_t sessions[MAX_CLIENTS];
    int session_count;
    pthread_mutex_t table_mutex;
    int next_session_index;
} session_table_t;

// Protocol detection results
typedef struct {
    protocol_type_t protocol;
    int confidence;
    char details[128];
} detection_result_t;

// Global session table (State Management Table)
session_table_t g_session_table = {0};
int g_server_running = 1;

// Function prototypes
protocol_type_t detect_protocol(const uint8_t* data, size_t len);
int detect_mqtt(const uint8_t* data, size_t len);
int detect_coap(const uint8_t* data, size_t len);
int create_session(int socket_fd, struct sockaddr_in* client_addr, protocol_type_t protocol);
client_session_t* get_session_by_socket(int socket_fd);
void remove_session(int socket_fd);
void handle_mqtt_message(client_session_t* session, const uint8_t* data, size_t len);
void handle_coap_message(client_session_t* session, const uint8_t* data, size_t len);
void* client_handler(void* arg);
void print_session_table();
const char* protocol_to_string(protocol_type_t protocol);
const char* state_to_string(session_state_t state);

// Protocol Detection Engine
protocol_type_t detect_protocol(const uint8_t* data, size_t len) {
    if (len < 2) {
        return PROTOCOL_UNKNOWN;
    }
    
    printf("[DETECTION] Analyzing %zu bytes: ", len);
    for (size_t i = 0; i < (len > 8 ? 8 : len); i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Try MQTT detection first
    if (detect_mqtt(data, len)) {
        printf("[DETECTION] ✓ MQTT detected\n");
        return PROTOCOL_MQTT;
    }
    
    // Try CoAP detection
    if (detect_coap(data, len)) {
        printf("[DETECTION] ✓ CoAP detected\n");
        return PROTOCOL_COAP;
    }
    
    printf("[DETECTION] ✗ Unknown protocol\n");
    return PROTOCOL_UNKNOWN;
}

int detect_mqtt(const uint8_t* data, size_t len) {
    if (len < 2) return 0;
    
    uint8_t first_byte = data[0];
    
    // Extract message type (bits 4-7)
    uint8_t msg_type = (first_byte >> 4) & 0x0F;
    
    // Valid MQTT message types: 1-14
    if (msg_type < 1 || msg_type > 14) {
        return 0;
    }
    
    // Check remaining length encoding
    size_t byte_index = 1;
    int multiplier = 1;
    int remaining_length = 0;
    
    while (byte_index < len && byte_index < 5) { // Max 4 bytes for remaining length
        uint8_t encoded_byte = data[byte_index];
        remaining_length += (encoded_byte & 0x7F) * multiplier;
        
        if ((encoded_byte & 0x80) == 0) {
            break;
        }
        
        multiplier *= 128;
        byte_index++;
    }
    
    // For CONNECT messages, check protocol name
    if (msg_type == 1 && len > 10) { // CONNECT
        // Skip to protocol name length
        if (byte_index + 1 + 2 < len) {
            uint16_t protocol_name_len = (data[byte_index + 1] << 8) | data[byte_index + 2];
            if (protocol_name_len == 4 && byte_index + 1 + 2 + 4 < len) {
                if (memcmp(&data[byte_index + 3], "MQTT", 4) == 0) {
                    return 1;
                }
            }
        }
    }
    
    // Basic structure validation passed
    return 1;
}

int detect_coap(const uint8_t* data, size_t len) {
    if (len < 4) return 0; // CoAP header minimum 4 bytes
    
    uint8_t first_byte = data[0];
    
    // Extract version (bits 6-7) - should be 1
    uint8_t version = (first_byte >> 6) & 0x03;
    if (version != 1) return 0;
    
    // Extract message type (bits 4-5)
    uint8_t msg_type = (first_byte >> 4) & 0x03;
    if (msg_type > 3) return 0; // 0=CON, 1=NON, 2=ACK, 3=RST
    
    // Extract token length (bits 0-3)
    uint8_t token_length = first_byte & 0x0F;
    if (token_length > 8) return 0; // Max 8 bytes
    
    // Check code (second byte)
    uint8_t code = data[1];
    uint8_t code_class = (code >> 5) & 0x07;
    if (code_class != 0 && code_class != 2 && code_class != 4 && code_class != 5) {
        return 0;
    }
    
    return 1;
}

// STATE MANAGEMENT TABLE Operations
int create_session(int socket_fd, struct sockaddr_in* client_addr, protocol_type_t protocol) {
    pthread_mutex_lock(&g_session_table.table_mutex);
    
    if (g_session_table.session_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&g_session_table.table_mutex);
        return -1;
    }
    
    // Find free slot
    int session_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_session_table.sessions[i].is_active) {
            session_index = i;
            break;
        }
    }
    
    if (session_index == -1) {
        pthread_mutex_unlock(&g_session_table.table_mutex);
        return -1;
    }
    
    client_session_t* session = &g_session_table.sessions[session_index];
    
    // Initialize session
    snprintf(session->session_id, MAX_SESSION_ID, "sess_%d_%ld", 
             socket_fd, time(NULL));
    session->socket_fd = socket_fd;
    session->client_addr = *client_addr;
    session->protocol = protocol;
    session->state = SESSION_CONNECTED;
    session->last_activity = time(NULL);
    session->created_at = time(NULL);
    session->message_count = 0;
    
    // Set flags atomically (better than bitfields)
    session->session_flags.flags = SESSION_FLAG_ACTIVE;
    
    // Initialize protocol-specific data
    memset(&session->protocol_data, 0, sizeof(protocol_data_t));
    if (protocol == PROTOCOL_MQTT) {
        session->protocol_data.mqtt.keep_alive_interval = 60;
        session->protocol_data.mqtt.qos_level = 0;
    } else if (protocol == PROTOCOL_COAP) {
        session->protocol_data.coap.message_id_counter = 1;
        session->protocol_data.coap.token_length = 0;
    }
    
    pthread_mutex_init(&session->session_mutex, NULL);
    
    g_session_table.session_count++;
    
    pthread_mutex_unlock(&g_session_table.table_mutex);
    
    printf("[STATE TABLE] Created session %s for %s:%d (Protocol: %s)\n",
           session->session_id,
           inet_ntoa(client_addr->sin_addr),
           ntohs(client_addr->sin_port),
           protocol_to_string(protocol));
    
    return session_index;
}

client_session_t* get_session_by_socket(int socket_fd) {
    pthread_mutex_lock(&g_session_table.table_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if ((g_session_table.sessions[i].session_flags.flags & SESSION_FLAG_ACTIVE) && 
            g_session_table.sessions[i].socket_fd == socket_fd) {
            pthread_mutex_unlock(&g_session_table.table_mutex);
            return &g_session_table.sessions[i];
        }
    }
    
    pthread_mutex_unlock(&g_session_table.table_mutex);
    return NULL;
}

void remove_session(int socket_fd) {
    pthread_mutex_lock(&g_session_table.table_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if ((g_session_table.sessions[i].session_flags.flags & SESSION_FLAG_ACTIVE) && 
            g_session_table.sessions[i].socket_fd == socket_fd) {
            
            client_session_t* session = &g_session_table.sessions[i];
            
            printf("[STATE TABLE] Removing session %s\n", session->session_id);
            
            pthread_mutex_destroy(&session->session_mutex);
            memset(session, 0, sizeof(client_session_t));
            // Clear active flag
            session->session_flags.flags = 0;
            g_session_table.session_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_session_table.table_mutex);
}

void update_session_activity(client_session_t* session) {
    pthread_mutex_lock(&session->session_mutex);
    session->last_activity = time(NULL);
    session->message_count++;
    pthread_mutex_unlock(&session->session_mutex);
}

// Protocol Handlers
void handle_mqtt_message(client_session_t* session, const uint8_t* data, size_t len) {
    uint8_t msg_type = (data[0] >> 4) & 0x0F;
    
    printf("[MQTT] Processing message type %d from %s\n", 
           msg_type, session->session_id);
    
    switch (msg_type) {
        case 1: { // CONNECT
            printf("[MQTT] CONNECT received\n");
            session->state = SESSION_AUTHENTICATED;
            
            // Send CONNACK
            uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
            send(session->socket_fd, connack, sizeof(connack), 0);
            printf("[MQTT] CONNACK sent\n");
            break;
        }
        case 3: { // PUBLISH
            printf("[MQTT] PUBLISH received\n");
            // Parse topic and payload (simplified)
            if (len > 4) {
                uint16_t topic_len = (data[2] << 8) | data[3];
                if (topic_len < len - 4) {
                    printf("[MQTT] Topic length: %d\n", topic_len);
                }
            }
            break;
        }
        case 12: { // PINGREQ
            printf("[MQTT] PINGREQ received\n");
            // Send PINGRESP
            uint8_t pingresp[] = {0xD0, 0x00};
            send(session->socket_fd, pingresp, sizeof(pingresp), 0);
            printf("[MQTT] PINGRESP sent\n");
            break;
        }
        case 14: { // DISCONNECT
            printf("[MQTT] DISCONNECT received\n");
            session->state = SESSION_DISCONNECTING;
            break;
        }
        default:
            printf("[MQTT] Unhandled message type: %d\n", msg_type);
    }
    
    update_session_activity(session);
}

void handle_coap_message(client_session_t* session, const uint8_t* data, size_t len) {
    uint8_t msg_type = (data[0] >> 4) & 0x03;
    uint8_t code = data[1];
    uint16_t message_id = (data[2] << 8) | data[3];
    
    printf("[COAP] Processing - Type: %d, Code: %d, ID: %d from %s\n",
           msg_type, code, message_id, session->session_id);
    
    // If it's a request, send ACK response
    if (code >= 1 && code <= 31) {
        uint8_t response[64];
        response[0] = 0x60; // Version=1, Type=ACK, Token Length=0
        response[1] = 0x45; // Code=2.05 (Content)
        response[2] = data[2]; // Same message ID
        response[3] = data[3];
        
        // Simple payload
        const char* payload = "Hello from CoAP middleware";
        memcpy(&response[4], payload, strlen(payload));
        
        send(session->socket_fd, response, 4 + strlen(payload), 0);
        printf("[COAP] ACK response sent\n");
    }
    
    update_session_activity(session);
}

// Client handler thread
void* client_handler(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);
    
    printf("[CONNECTION] New client: %s:%d (socket: %d)\n",
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port),
           client_socket);
    
    uint8_t buffer[BUFFER_SIZE];
    ssize_t received;
    
    // Wait for first packet to detect protocol
    received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (received <= 0) {
        printf("[CONNECTION] No data received from client\n");
        close(client_socket);
        return NULL;
    }
    
    // Detect protocol
    protocol_type_t protocol = detect_protocol(buffer, received);
    if (protocol == PROTOCOL_UNKNOWN) {
        printf("[CONNECTION] Unknown protocol, closing connection\n");
        close(client_socket);
        return NULL;
    }
    
    // Create session in state table
    int session_index = create_session(client_socket, &client_addr, protocol);
    if (session_index == -1) {
        printf("[CONNECTION] Failed to create session\n");
        close(client_socket);
        return NULL;
    }
    
    client_session_t* session = &g_session_table.sessions[session_index];
    
    // Process first message
    if (protocol == PROTOCOL_MQTT) {
        handle_mqtt_message(session, buffer, received);
    } else if (protocol == PROTOCOL_COAP) {
        handle_coap_message(session, buffer, received);
    }
    
    // Continue handling messages
    while (g_server_running && session->state != SESSION_DISCONNECTING) {
        received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            if (received == 0) {
                printf("[CONNECTION] Client disconnected normally\n");
            } else {
                printf("[CONNECTION] Receive error: %s\n", strerror(errno));
            }
            break;
        }
        
        // Process message based on protocol
        if (protocol == PROTOCOL_MQTT) {
            handle_mqtt_message(session, buffer, received);
        } else if (protocol == PROTOCOL_COAP) {
            handle_coap_message(session, buffer, received);
        }
    }
    
    // Cleanup
    remove_session(client_socket);
    close(client_socket);
    printf("[CONNECTION] Client handler finished\n");
    
    return NULL;
}

// Utility functions
const char* protocol_to_string(protocol_type_t protocol) {
    switch (protocol) {
        case PROTOCOL_MQTT: return "MQTT";
        case PROTOCOL_COAP: return "CoAP";
        case PROTOCOL_UNKNOWN: default: return "Unknown";
    }
}

const char* state_to_string(session_state_t state) {
    switch (state) {
        case SESSION_CONNECTED: return "Connected";
        case SESSION_AUTHENTICATED: return "Authenticated";
        case SESSION_ACTIVE: return "Active";
        case SESSION_DISCONNECTING: return "Disconnecting";
        case SESSION_CLOSED: return "Closed";
        default: return "Unknown";
    }
}

void print_session_table() {
    pthread_mutex_lock(&g_session_table.table_mutex);
    
    printf("\n=== SESSION STATE TABLE ===\n");
    printf("Active sessions: %d/%d\n", g_session_table.session_count, MAX_CLIENTS);
    printf("%-12s %-8s %-10s %-8s %-12s %-8s %-8s\n", 
           "Session ID", "Socket", "Protocol", "State", "Client IP", "Messages", "Uptime");
    printf("--------------------------------------------------------------------------------\n");
    
    time_t now = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_session_table.sessions[i].is_active) {
            client_session_t* s = &g_session_table.sessions[i];
            printf("%-12s %-8d %-10s %-8s %-12s %-8d %-8lds\n",
                   s->session_id,
                   s->socket_fd,
                   protocol_to_string(s->protocol),
                   state_to_string(s->state),
                   inet_ntoa(s->client_addr.sin_addr),
                   s->message_count,
                   now - s->created_at);
        }
    }
    printf("================================================================================\n\n");
    
    pthread_mutex_unlock(&g_session_table.table_mutex);
}

// Test client functions
void create_mqtt_test_packet(uint8_t* buffer, size_t* len) {
    // MQTT CONNECT packet
    uint8_t mqtt_connect[] = {
        0x10, 0x12, // Fixed header: CONNECT, remaining length=18
        0x00, 0x04, 'M', 'Q', 'T', 'T', // Protocol name
        0x04, // Protocol version
        0x02, // Connect flags (clean session)
        0x00, 0x3C, // Keep alive (60 seconds)
        0x00, 0x06, 't', 'e', 's', 't', 'e', 'r' // Client ID "tester"
    };
    memcpy(buffer, mqtt_connect, sizeof(mqtt_connect));
    *len = sizeof(mqtt_connect);
}

void create_coap_test_packet(uint8_t* buffer, size_t* len) {
    // CoAP GET request
    uint8_t coap_get[] = {
        0x40, // Version=1, Type=CON, Token length=0
        0x01, // Code=0.01 (GET)
        0x30, 0x39, // Message ID
    };
    memcpy(buffer, coap_get, sizeof(coap_get));
    *len = sizeof(coap_get);
}

int main() {
    printf("Protocol-Agnostic Middleware in C\n");
    printf("=================================\n");
    
    // Initialize session table
    pthread_mutex_init(&g_session_table.table_mutex, NULL);
    memset(&g_session_table, 0, sizeof(session_table_t));
    
    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        return 1;
    }
    
    // Listen for connections
    if (listen(server_socket, 10) == -1) {
        perror("Listen failed");
        close(server_socket);
        return 1;
    }
    
    printf("Server listening on port %d\n", SERVER_PORT);
    printf("Waiting for clients...\n\n");
    
    // Main server loop
    while (g_server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, 
                                 (struct sockaddr*)&client_addr, 
                                 &client_addr_len);
        
        if (client_socket == -1) {
            if (g_server_running) {
                perror("Accept failed");
            }
            continue;
        }
        
        // Create thread to handle client
        pthread_t client_thread;
        int* socket_arg = malloc(sizeof(int));
        *socket_arg = client_socket;
        
        if (pthread_create(&client_thread, NULL, client_handler, socket_arg) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(socket_arg);
            continue;
        }
        
        pthread_detach(client_thread);
        
        // Print session table every few connections
        static int connection_count = 0;
        if (++connection_count % 2 == 0) {
            print_session_table();
        }
    }
    
    // Cleanup
    close(server_socket);
    pthread_mutex_destroy(&g_session_table.table_mutex);
    
    printf("Server shutdown complete\n");
    return 0;
}