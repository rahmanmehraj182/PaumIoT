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
#include <ctype.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define MAX_SESSION_ID 64
#define SERVER_PORT 8080

// Protocol types - Enhanced with HTTP and DNS
typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_HTTP = 3,
    PROTOCOL_DNS = 4
} protocol_type_t;

// Client session state
typedef enum {
    SESSION_CONNECTED = 0,
    SESSION_AUTHENTICATED = 1,
    SESSION_ACTIVE = 2,
    SESSION_DISCONNECTING = 3,
    SESSION_CLOSED = 4
} session_state_t;

// Session flags using efficient bit manipulation
typedef struct {
    uint32_t flags;
    // Bit layout:
    // 0: is_active
    // 1: authenticated  
    // 2: keep_alive_enabled
    // 3: clean_session (MQTT)
    // 4: observe_active (CoAP)
    // 5: http_keep_alive (HTTP)
    // 6: dns_recursive (DNS)
    // 7: reserved
    // 8-15: protocol_specific_flags
    // 16-31: reserved for future use
} session_flags_t;

// Flag constants for easy manipulation
#define SESSION_FLAG_ACTIVE         (1U << 0)
#define SESSION_FLAG_AUTHENTICATED  (1U << 1) 
#define SESSION_FLAG_KEEPALIVE      (1U << 2)
#define SESSION_FLAG_CLEAN_SESSION  (1U << 3)
#define SESSION_FLAG_OBSERVE_ACTIVE (1U << 4)
#define SESSION_FLAG_HTTP_KEEPALIVE (1U << 5)
#define SESSION_FLAG_DNS_RECURSIVE  (1U << 6)

// Protocol-specific data - Enhanced with HTTP and DNS
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
    
    struct {
        char method[16];        // GET, POST, etc.
        char uri[128];          // Requested URI
        char version[16];       // HTTP/1.1, etc.
        char host[64];          // Host header
        char user_agent[64];    // User-Agent header
        int content_length;     // Content-Length header
        uint8_t connection_close; // Connection: close
    } http;
    
    struct {
        uint16_t transaction_id;
        uint16_t flags;         // DNS flags
        uint16_t questions;     // Question count
        uint16_t answers;       // Answer count
        char query_name[128];   // Queried domain name
        uint16_t query_type;    // A, AAAA, MX, etc.
    } dns;
    
    uint8_t raw_data[256];  // Increased size for HTTP/DNS data
} protocol_data_t;

// Optimized client session structure 
typedef struct {
    char session_id[MAX_SESSION_ID];
    int socket_fd;
    struct sockaddr_in client_addr;
    protocol_type_t protocol;
    session_state_t state;
    session_flags_t session_flags;
    protocol_data_t protocol_data;
    
    // Timing and counters (cache-line aligned)
    time_t last_activity;
    time_t created_at;
    uint32_t message_count;
    uint32_t error_count;
    
    pthread_mutex_t session_mutex;
    
    // Performance: pad to cache line boundary (64 bytes)
    char padding[0] __attribute__((aligned(64)));
} client_session_t;

// STATE MANAGEMENT TABLE
typedef struct {
    client_session_t sessions[MAX_CLIENTS];
    int session_count;
    pthread_mutex_t table_mutex;
    int next_session_index;
} session_table_t;

// Global session table
session_table_t g_session_table = {0};
int g_server_running = 1;

// Function prototypes
protocol_type_t detect_protocol(const uint8_t* data, size_t len);
int detect_mqtt(const uint8_t* data, size_t len);
int detect_coap(const uint8_t* data, size_t len);
int detect_http(const uint8_t* data, size_t len);
int detect_dns(const uint8_t* data, size_t len);
int create_session(int socket_fd, struct sockaddr_in* client_addr, protocol_type_t protocol);
client_session_t* get_session_by_socket(int socket_fd);
void remove_session(int socket_fd);
void handle_mqtt_message(client_session_t* session, const uint8_t* data, size_t len);
void handle_coap_message(client_session_t* session, const uint8_t* data, size_t len);
void handle_http_message(client_session_t* session, const uint8_t* data, size_t len);
void handle_dns_message(client_session_t* session, const uint8_t* data, size_t len, struct sockaddr_in* client_addr);
void* client_handler(void* arg);
void* udp_handler(void* arg);
void print_session_table();
const char* protocol_to_string(protocol_type_t protocol);
const char* state_to_string(session_state_t state);

// Enhanced Protocol Detection Engine
protocol_type_t detect_protocol(const uint8_t* data, size_t len) {
    if (len < 2) {
        printf("[DETECTION] Packet too short for detection (%zu bytes)\n", len);
        return PROTOCOL_UNKNOWN;
    }

    printf("Printing Data : %s\n", (char*)data);
    
    printf("[DETECTION] Analyzing %zu bytes: ", len);
    for (size_t i = 0; i < (len > 16 ? 16 : len); i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    int scores[5] = {0}; // mqtt, coap, http, dns scores
    
    // Try HTTP detection first (most distinctive)
    if (detect_http(data, len)) {
        scores[3] = 15; // High confidence for HTTP
        printf("[DETECTION] HTTP validation: PASS (score: %d)\n", scores[3]);
    }
    
    // Try DNS detection (for UDP packets)
    if (detect_dns(data, len)) {
        scores[4] = 12;
        printf("[DETECTION] DNS validation: PASS (score: %d)\n", scores[4]);
    }
    
    // Try MQTT detection
    if (detect_mqtt(data, len)) {
        scores[1] = 10;
        printf("[DETECTION] MQTT validation: PASS (score: %d)\n", scores[1]);
    }
    
    // Try CoAP detection
    if (detect_coap(data, len)) {
        scores[2] = 10;
        printf("[DETECTION] CoAP validation: PASS (score: %d)\n", scores[2]);
    }
    
    // Determine best match
    int max_score = 0;
    protocol_type_t detected = PROTOCOL_UNKNOWN;
    
    if (scores[3] > max_score) { max_score = scores[3]; detected = PROTOCOL_HTTP; }
    if (scores[4] > max_score) { max_score = scores[4]; detected = PROTOCOL_DNS; }
    if (scores[1] > max_score) { max_score = scores[1]; detected = PROTOCOL_MQTT; }
    if (scores[2] > max_score) { max_score = scores[2]; detected = PROTOCOL_COAP; }
    
    if (detected != PROTOCOL_UNKNOWN) {
        printf("[DETECTION] ✓ %s detected (confidence: %d)\n", 
               protocol_to_string(detected), max_score);
        return detected;
    }
    
    printf("[DETECTION] ✗ MALFORMED PDU - Unknown/invalid protocol\n");
    return PROTOCOL_UNKNOWN;
}

int detect_http(const uint8_t* data, size_t len) {
    if (len < 14) { // Minimum for "GET / HTTP/1.1"
        return 0;
    }
    
    // Check for HTTP methods at the beginning
    const char* methods[] = {"GET ", "POST ", "PUT ", "DELETE ", "HEAD ", 
                           "OPTIONS ", "PATCH ", "TRACE ", "CONNECT "};
    int method_count = sizeof(methods) / sizeof(methods[0]);
    
    int method_found = 0;
    for (int i = 0; i < method_count; i++) {
        size_t method_len = strlen(methods[i]);
        if (len >= method_len && memcmp(data, methods[i], method_len) == 0) {
            printf("[HTTP_DETECT] Method found: %s\n", methods[i]);
            method_found = 1;
            break;
        }
    }
    
    if (!method_found) {
        return 0;
    }
    
    // Look for HTTP version string
    char* data_str = (char*)data;
    char* http_pos = strstr(data_str, "HTTP/");
    if (!http_pos) {
        printf("[HTTP_DETECT] No HTTP version found\n");
        return 0;
    }
    
    // Check if it's within reasonable bounds
    if (http_pos - data_str > len - 8) {
        printf("[HTTP_DETECT] HTTP version position invalid\n");
        return 0;
    }
    
    // Verify HTTP version format (HTTP/x.x)
    if (http_pos[5] != '.' || !isdigit(http_pos[4]) || !isdigit(http_pos[6])) {
        printf("[HTTP_DETECT] Invalid HTTP version format\n");
        return 0;
    }
    
    // Look for CRLF (\r\n) which should be present in HTTP
    if (!strstr(data_str, "\r\n")) {
        printf("[HTTP_DETECT] No CRLF found\n");
        return 0;
    }
    
    printf("[HTTP_DETECT] Valid HTTP request detected\n");
    return 1;
}

int detect_dns(const uint8_t* data, size_t len) {
    if (len < 12) { // DNS header minimum size
        printf("[DNS_DETECT] Packet too short (%zu bytes, minimum 12)\n", len);
        return 0;
    }
    
    // DNS Header structure:
    // 0-1: Transaction ID
    // 2-3: Flags
    // 4-5: Questions
    // 6-7: Answer RRs
    // 8-9: Authority RRs
    // 10-11: Additional RRs
    
    uint16_t flags = (data[2] << 8) | data[3];
    uint16_t questions = (data[4] << 8) | data[5];
    uint16_t answers = (data[6] << 8) | data[7];
    uint16_t authority = (data[8] << 8) | data[9];
    uint16_t additional = (data[10] << 8) | data[11];
    
    // Check QR bit (bit 15 of flags) - 0 for query, 1 for response
    uint8_t qr = (flags >> 15) & 0x01;
    
    // Check OPCODE (bits 11-14) - should be 0-2 for standard queries
    uint8_t opcode = (flags >> 11) & 0x0F;
    if (opcode > 2) {
        printf("[DNS_DETECT] Invalid OPCODE: %d\n", opcode);
        return 0;
    }
    
    // Check RCODE (bits 0-3) for responses - should be 0-5 for common codes
    if (qr == 1) { // Response
        uint8_t rcode = flags & 0x0F;
        if (rcode > 5 && rcode != 9) { // Allow NOTAUTH (9) as well
            printf("[DNS_DETECT] Invalid RCODE: %d\n", rcode);
            return 0;
        }
    }
    
    // Reasonable limits check
    if (questions > 100 || answers > 100 || authority > 100 || additional > 100) {
        printf("[DNS_DETECT] Unreasonable record counts\n");
        return 0;
    }
    
    // For queries, there should be at least one question
    if (qr == 0 && questions == 0) {
        printf("[DNS_DETECT] Query with no questions\n");
        return 0;
    }
    
    // If there are questions, validate the first one
    if (questions > 0 && len > 12) {
        int pos = 12;
        int labels = 0;
        
        // Parse domain name labels
        while (pos < len && labels < 63) { // Max 63 labels in domain name
            uint8_t label_len = data[pos];
            
            if (label_len == 0) { // End of domain name
                pos++;
                break;
            }
            
            if ((label_len & 0xC0) == 0xC0) { // Compression pointer
                pos += 2;
                break;
            }
            
            if (label_len > 63) { // Max label length
                printf("[DNS_DETECT] Invalid label length: %d\n", label_len);
                return 0;
            }
            
            pos += 1 + label_len;
            labels++;
            
            if (pos >= len) {
                printf("[DNS_DETECT] Truncated domain name\n");
                return 0;
            }
        }
        
        // Check for QTYPE and QCLASS (4 bytes after domain name)
        if (pos + 4 > len) {
            printf("[DNS_DETECT] Missing QTYPE/QCLASS\n");
            return 0;
        }
    }
    
    printf("[DNS_DETECT] Valid DNS packet - QR:%d OPCODE:%d Questions:%d Answers:%d\n",
           qr, opcode, questions, answers);
    
    return 1;
}

// Keep existing MQTT and CoAP detection functions (they're already good)
int detect_mqtt(const uint8_t* data, size_t len) {
    if (len < 2) {
        printf("[MQTT_DETECT] Packet too short (%zu bytes)\n", len);
        return 0;
    }
    
    uint8_t first_byte = data[0];
    uint8_t msg_type = (first_byte >> 4) & 0x0F;
    
    if (msg_type < 1 || msg_type > 14) {
        return 0;
    }
    
    uint8_t flags = first_byte & 0x0F;
    switch (msg_type) {
        case 1: case 2: case 4: case 5: case 6: case 7: case 8: 
        case 9: case 10: case 11: case 12: case 13: case 14:
            if (flags != 0 && msg_type != 3) return 0;
            break;
        case 3: // PUBLISH
            if (((flags >> 1) & 0x03) == 3) return 0;
            break;
    }
    
    // Basic remaining length validation
    size_t byte_index = 1;
    int multiplier = 1;
    int remaining_length = 0;
    
    while (byte_index < len && byte_index < 5) {
        uint8_t encoded_byte = data[byte_index];
        remaining_length += (encoded_byte & 0x7F) * multiplier;
        
        if ((encoded_byte & 0x80) == 0) break;
        
        multiplier *= 128;
        byte_index++;
        if (byte_index >= 5) return 0;
    }
    
    return 1;
}

int detect_coap(const uint8_t* data, size_t len) {
    if (len < 4) {
        return 0;
    }
    
    uint8_t first_byte = data[0];
    uint8_t version = (first_byte >> 6) & 0x03;
    if (version != 1) return 0;
    
    uint8_t msg_type = (first_byte >> 4) & 0x03;
    if (msg_type > 3) return 0;
    
    uint8_t token_length = first_byte & 0x0F;
    if (token_length > 8) return 0;
    
    if (len < 4 + token_length) return 0;
    
    uint8_t code = data[1];
    uint8_t code_class = (code >> 5) & 0x07;
    
    switch (code_class) {
        case 0: case 2: case 4: case 5: break;
        case 1: case 3: case 6: case 7: return 0;
        default: return 0;
    }
    
    return 1;
}

// Enhanced Session Management
int create_session(int socket_fd, struct sockaddr_in* client_addr, protocol_type_t protocol) {
    pthread_mutex_lock(&g_session_table.table_mutex);
    
    if (g_session_table.session_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&g_session_table.table_mutex);
        return -1;
    }
    
    int session_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!(g_session_table.sessions[i].session_flags.flags & SESSION_FLAG_ACTIVE)) {
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
    snprintf(session->session_id, MAX_SESSION_ID, "%s_%d_%ld", 
             protocol_to_string(protocol), socket_fd, time(NULL));
    session->socket_fd = socket_fd;
    session->client_addr = *client_addr;
    session->protocol = protocol;
    session->state = SESSION_CONNECTED;
    session->last_activity = time(NULL);
    session->created_at = time(NULL);
    session->message_count = 0;
    
    session->session_flags.flags = SESSION_FLAG_ACTIVE;
    
    // Initialize protocol-specific data
    memset(&session->protocol_data, 0, sizeof(protocol_data_t));
    switch (protocol) {
        case PROTOCOL_MQTT:
            session->protocol_data.mqtt.keep_alive_interval = 60;
            session->protocol_data.mqtt.qos_level = 0;
            break;
        case PROTOCOL_COAP:
            session->protocol_data.coap.message_id_counter = 1;
            session->protocol_data.coap.token_length = 0;
            break;
        case PROTOCOL_HTTP:
            strcpy(session->protocol_data.http.version, "HTTP/1.1");
            session->protocol_data.http.connection_close = 0;
            break;
        case PROTOCOL_DNS:
            session->protocol_data.dns.transaction_id = 0;
            session->protocol_data.dns.query_type = 1; // A record
            break;
        default:
            break;
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

// Enhanced Protocol Handlers
void handle_http_message(client_session_t* session, const uint8_t* data, size_t len) {
    char* request = (char*)data;
    printf("[HTTP] Processing request from %s\n", session->session_id);
    
    // Parse HTTP request line
    char method[16], uri[128], version[16];
    if (sscanf(request, "%15s %127s %15s", method, uri, version) == 3) {
        strcpy(session->protocol_data.http.method, method);
        strcpy(session->protocol_data.http.uri, uri);
        strcpy(session->protocol_data.http.version, version);
        
        printf("[HTTP] %s %s %s\n", method, uri, version);
    }
    
    // Parse headers
    char* header_start = strstr(request, "\r\n");
    if (header_start) {
        header_start += 2;
        char* header_end = strstr(header_start, "\r\n\r\n");
        
        if (header_end) {
            // Look for common headers
            char* host_header = strstr(header_start, "Host: ");
            if (host_header && host_header < header_end) {
                sscanf(host_header + 6, "%63[^\r\n]", session->protocol_data.http.host);
                printf("[HTTP] Host: %s\n", session->protocol_data.http.host);
            }
            
            char* ua_header = strstr(header_start, "User-Agent: ");
            if (ua_header && ua_header < header_end) {
                sscanf(ua_header + 12, "%63[^\r\n]", session->protocol_data.http.user_agent);
                printf("[HTTP] User-Agent: %s\n", session->protocol_data.http.user_agent);
            }
            
            char* conn_header = strstr(header_start, "Connection: ");
            if (conn_header && conn_header < header_end) {
                if (strstr(conn_header, "close")) {
                    session->protocol_data.http.connection_close = 1;
                    printf("[HTTP] Connection: close\n");
                } else if (strstr(conn_header, "keep-alive")) {
                    session->session_flags.flags |= SESSION_FLAG_HTTP_KEEPALIVE;
                    printf("[HTTP] Connection: keep-alive\n");
                }
            }
        }
    }
    
    // Generate HTTP response
    char response[1024];
    const char* status_line = "HTTP/1.1 200 OK\r\n";
    const char* headers = 
        "Content-Type: application/json\r\n"
        "Server: Protocol-Middleware/1.0\r\n"
        "Access-Control-Allow-Origin: *\r\n";
    
    const char* body = "{\n"
        "  \"status\": \"success\",\n"
        "  \"message\": \"Hello from HTTP middleware\",\n"
        "  \"protocol\": \"HTTP\",\n"
        "  \"method\": \"%s\",\n"
        "  \"uri\": \"%s\",\n"
        "  \"timestamp\": %ld\n"
        "}";
    
    char json_body[512];
    snprintf(json_body, sizeof(json_body), body, 
             session->protocol_data.http.method,
             session->protocol_data.http.uri,
             time(NULL));
    
    snprintf(response, sizeof(response),
        "%s%sContent-Length: %zu\r\n\r\n%s",
        status_line, headers, strlen(json_body), json_body);
    
    send(session->socket_fd, response, strlen(response), 0);
    printf("[HTTP] Response sent (%zu bytes)\n", strlen(response));
    
    // Close connection if requested
    if (session->protocol_data.http.connection_close) {
        session->state = SESSION_DISCONNECTING;
    }
    
    update_session_activity(session);
}

void handle_dns_message(client_session_t* session, const uint8_t* data, size_t len, struct sockaddr_in* client_addr) {
    printf("[DNS] Processing query from %s\n", session->session_id);
    
    if (len < 12) {
        printf("[DNS] Query too short\n");
        return;
    }
    
    // Parse DNS header
    uint16_t transaction_id = (data[0] << 8) | data[1];
    uint16_t flags = (data[2] << 8) | data[3];
    uint16_t questions = (data[4] << 8) | data[5];
    
    session->protocol_data.dns.transaction_id = transaction_id;
    session->protocol_data.dns.flags = flags;
    session->protocol_data.dns.questions = questions;
    
    printf("[DNS] Transaction ID: 0x%04X, Questions: %d\n", transaction_id, questions);
    
    // Parse first question (simplified)
    if (questions > 0 && len > 12) {
        int pos = 12;
        char domain_name[256] = {0};
        int domain_pos = 0;
        
        // Parse domain name labels
        while (pos < len && domain_pos < 255) {
            uint8_t label_len = data[pos++];
            
            if (label_len == 0) break; // End of name
            
            if (label_len > 63) break; // Invalid
            
            if (pos + label_len > len) break; // Truncated
            
            if (domain_pos > 0) {
                domain_name[domain_pos++] = '.';
            }
            
            memcpy(domain_name + domain_pos, data + pos, label_len);
            domain_pos += label_len;
            pos += label_len;
        }
        
        if (pos + 4 <= len) {
            uint16_t qtype = (data[pos] << 8) | data[pos + 1];
            uint16_t qclass = (data[pos + 2] << 8) | data[pos + 3];
            
            strncpy(session->protocol_data.dns.query_name, domain_name, 127);
            session->protocol_data.dns.query_type = qtype;
            
            printf("[DNS] Query: %s Type: %d Class: %d\n", domain_name, qtype, qclass);
        }
    }
    
    // Generate DNS response
    uint8_t response[512];
    int resp_pos = 0;
    
    // Copy transaction ID
    response[resp_pos++] = data[0];
    response[resp_pos++] = data[1];
    
    // Set response flags (QR=1, OPCODE=0, AA=0, TC=0, RD=1, RA=1, RCODE=0)
    response[resp_pos++] = 0x81; // QR=1, OPCODE=0, AA=0, TC=0, RD=1
    response[resp_pos++] = 0x80; // RA=1, Z=0, RCODE=0
    
    // Question count (same as query)
    response[resp_pos++] = data[4];
    response[resp_pos++] = data[5];
    
    // Answer count (1 answer)
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x01;
    
    // Authority and Additional (0)
    response[resp_pos++] = 0x00; response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x00; response[resp_pos++] = 0x00;
    
            // Copy question section
    int question_len = 0;
    for (int i = 12; i < len && i < 500; i++) {
        response[resp_pos++] = data[i];
        question_len++;
        // Stop at end of question (after QCLASS)
        if (data[i] == 0 && question_len > 5) {
            // Copy QTYPE and QCLASS (4 bytes)
            if (i + 4 < len) {
                for (int j = 0; j < 4; j++) {
                    response[resp_pos++] = data[i + 1 + j];
                }
                i += 4;
            }
            break;
        }
    }
    
    // Add answer section (simplified A record response)
    // Name (compression pointer to question)
    response[resp_pos++] = 0xC0;
    response[resp_pos++] = 0x0C;
    
    // Type (A record)
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x01;
    
    // Class (IN)
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x01;
    
    // TTL (300 seconds)
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x01;
    response[resp_pos++] = 0x2C;
    
    // Data length (4 bytes for IPv4)
    response[resp_pos++] = 0x00;
    response[resp_pos++] = 0x04;
    
    // IP address (127.0.0.1)
    response[resp_pos++] = 127;
    response[resp_pos++] = 0;
    response[resp_pos++] = 0;
    response[resp_pos++] = 1;
    
    printf("[DNS] Sending response (%d bytes)\n", resp_pos);
    update_session_activity(session);
}

// Keep existing MQTT and CoAP handlers
void handle_mqtt_message(client_session_t* session, const uint8_t* data, size_t len) {
    uint8_t msg_type = (data[0] >> 4) & 0x0F;
    
    printf("[MQTT] Processing message type %d from %s\n", 
           msg_type, session->session_id);
    
    switch (msg_type) {
        case 1: { // CONNECT
            printf("[MQTT] CONNECT received\n");
            session->state = SESSION_AUTHENTICATED;
            session->session_flags.flags |= SESSION_FLAG_AUTHENTICATED;
            
            if (len > 12) {
                uint16_t keep_alive = (data[10] << 8) | data[11];
                session->protocol_data.mqtt.keep_alive_interval = keep_alive;
                printf("[MQTT] Keep-alive interval: %d seconds\n", keep_alive);
            }
            
            uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
            send(session->socket_fd, connack, sizeof(connack), 0);
            printf("[MQTT] CONNACK sent\n");
            break;
        }
        case 3: { // PUBLISH
            printf("[MQTT] PUBLISH received\n");
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
    
    if (code >= 1 && code <= 31) {
        uint8_t response[128];
        response[0] = 0x60; // Version=1, Type=ACK, Token Length=0
        response[1] = 0x45; // Code=2.05 (Content)
        response[2] = data[2]; // Same message ID
        response[3] = data[3];
        
        const char* payload = "Hello from CoAP middleware";
        memcpy(&response[4], payload, strlen(payload));
        
        send(session->socket_fd, response, 4 + strlen(payload), 0);
        printf("[COAP] ACK response sent\n");
    }
    
    update_session_activity(session);
}

// Enhanced UDP handler for CoAP and DNS
void* udp_handler(void* arg) {
    int udp_socket = *(int*)arg;
    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    printf("[UDP] Handler started for CoAP and DNS\n");
    
    while (g_server_running) {
        ssize_t received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr*)&client_addr, &addr_len);
        
        if (received <= 0) {
            if (g_server_running) {
                perror("UDP recvfrom error");
            }
            continue;
        }
        
        printf("[UDP] Received %zd bytes from %s:%d\n", 
               received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        protocol_type_t protocol = detect_protocol(buffer, received);
        
        if (protocol == PROTOCOL_DNS) {
            printf("[UDP/DNS] Processing DNS query\n");
            
            // Create a temporary session for DNS (stateless)
            client_session_t temp_session = {0};
            snprintf(temp_session.session_id, MAX_SESSION_ID, "DNS_%ld", time(NULL));
            temp_session.client_addr = client_addr;
            temp_session.protocol = PROTOCOL_DNS;
            temp_session.socket_fd = udp_socket; // Use UDP socket
            
            // Handle DNS message
            handle_dns_message(&temp_session, buffer, received, &client_addr);
            
            // Send DNS response
            uint8_t response[512];
            int resp_pos = 0;
            
            // Transaction ID
            response[resp_pos++] = buffer[0];
            response[resp_pos++] = buffer[1];
            
            // Flags (response)
            response[resp_pos++] = 0x81;
            response[resp_pos++] = 0x80;
            
            // Questions (copy from query)
            response[resp_pos++] = buffer[4];
            response[resp_pos++] = buffer[5];
            
            // Answers (1)
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x01;
            
            // Authority and Additional (0)
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x00;
            
            // Copy question section
            int i = 12;
            while (i < received && resp_pos < 400) {
                response[resp_pos++] = buffer[i];
                if (buffer[i] == 0) {
                    // Copy QTYPE and QCLASS
                    if (i + 4 < received) {
                        response[resp_pos++] = buffer[i + 1];
                        response[resp_pos++] = buffer[i + 2];
                        response[resp_pos++] = buffer[i + 3];
                        response[resp_pos++] = buffer[i + 4];
                    }
                    break;
                }
                i++;
            }
            
            // Answer section
            response[resp_pos++] = 0xC0; response[resp_pos++] = 0x0C; // Name ptr
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x01; // Type A
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x01; // Class IN
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x00; // TTL
            response[resp_pos++] = 0x01; response[resp_pos++] = 0x2C; // TTL (300)
            response[resp_pos++] = 0x00; response[resp_pos++] = 0x04; // Data len
            response[resp_pos++] = 127; response[resp_pos++] = 0;     // IP
            response[resp_pos++] = 0;   response[resp_pos++] = 1;     // 127.0.0.1
            
            sendto(udp_socket, response, resp_pos, 0,
                   (struct sockaddr*)&client_addr, addr_len);
            printf("[DNS] Response sent (%d bytes)\n", resp_pos);
        }
        else if (protocol == PROTOCOL_COAP) {
            printf("[UDP/COAP] Processing CoAP request\n");
            
            uint8_t version = (buffer[0] >> 6) & 0x03;
            uint8_t msg_type = (buffer[0] >> 4) & 0x03;
            uint8_t token_len = buffer[0] & 0x0F;
            uint8_t code = buffer[1];
            uint16_t message_id = (buffer[2] << 8) | buffer[3];
            
            printf("[COAP/UDP] Ver:%d Type:%d Token:%d Code:0x%02X ID:%d\n",
                   version, msg_type, token_len, code, message_id);
            
            if (code >= 1 && code <= 31) {
                uint8_t response[256];
                int resp_len = 4;
                
                response[0] = 0x60; // ACK
                response[1] = 0x45; // 2.05 Content
                response[2] = buffer[2]; // Same message ID
                response[3] = buffer[3];
                
                if (token_len > 0 && token_len <= 8) {
                    response[0] = 0x60 | token_len;
                    memcpy(&response[4], &buffer[4], token_len);
                    resp_len += token_len;
                }
                
                // Add Content-Format option (application/json)
                response[resp_len++] = 0x12; // Option: Content-Format, length 2
                response[resp_len++] = 0x00;
                response[resp_len++] = 0x32; // Value: 50 (application/json)
                
                response[resp_len++] = 0xFF; // Payload marker
                
                const char* json_payload = "{\"message\":\"Hello from CoAP\",\"status\":\"success\"}";
                memcpy(&response[resp_len], json_payload, strlen(json_payload));
                resp_len += strlen(json_payload);
                
                sendto(udp_socket, response, resp_len, 0,
                       (struct sockaddr*)&client_addr, addr_len);
                printf("[COAP/UDP] Response sent (%d bytes)\n", resp_len);
            }
        }
        else if (protocol == PROTOCOL_MQTT) {
            printf("[UDP/MQTT] ⚠️  MQTT over UDP detected - NON-STANDARD!\n");
            const char* error_msg = "ERROR: MQTT requires TCP connection";
            sendto(udp_socket, error_msg, strlen(error_msg), 0,
                   (struct sockaddr*)&client_addr, addr_len);
        }
        else {
            printf("[UDP] Unknown protocol - sending error\n");
            const char* error_msg = "UNSUPPORTED: Use CoAP/DNS for UDP or MQTT/HTTP for TCP";
            sendto(udp_socket, error_msg, strlen(error_msg), 0,
                   (struct sockaddr*)&client_addr, addr_len);
        }
    }
    
    return NULL;
}

// Enhanced TCP client handler
void* client_handler(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);
    
    printf("[TCP] New client: %s:%d (socket: %d)\n",
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port),
           client_socket);
    
    uint8_t buffer[BUFFER_SIZE];
    ssize_t received;
    
    // Wait for first packet
    received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (received <= 0) {
        printf("[TCP] No data received\n");
        close(client_socket);
        return NULL;
    }
    
    // Detect protocol
    protocol_type_t protocol = detect_protocol(buffer, received);
    if (protocol == PROTOCOL_UNKNOWN) {
        printf("[TCP] Unknown protocol, closing connection\n");
        const char* error_response = "HTTP/1.1 400 Bad Request\r\n\r\nUnsupported protocol";
        send(client_socket, error_response, strlen(error_response), 0);
        close(client_socket);
        return NULL;
    }
    
    // Create session
    int session_index = create_session(client_socket, &client_addr, protocol);
    if (session_index == -1) {
        printf("[TCP] Failed to create session\n");
        close(client_socket);
        return NULL;
    }
    
    client_session_t* session = &g_session_table.sessions[session_index];
    
    // Process first message
    switch (protocol) {
        case PROTOCOL_MQTT:
            handle_mqtt_message(session, buffer, received);
            break;
        case PROTOCOL_HTTP:
            handle_http_message(session, buffer, received);
            break;
        case PROTOCOL_COAP:
            handle_coap_message(session, buffer, received);
            break;
        default:
            printf("[TCP] Unsupported protocol for TCP\n");
            break;
    }
    
    // Continue handling messages (except for HTTP which might close)
    while (g_server_running && session->state != SESSION_DISCONNECTING) {
        received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            printf("[TCP] Connection closed\n");
            break;
        }
        
        switch (protocol) {
            case PROTOCOL_MQTT:
                handle_mqtt_message(session, buffer, received);
                break;
            case PROTOCOL_HTTP:
                handle_http_message(session, buffer, received);
                break;
            case PROTOCOL_COAP:
                handle_coap_message(session, buffer, received);
                break;
            default:
                break;
        }
    }
    
    remove_session(client_socket);
    close(client_socket);
    printf("[TCP] Client handler finished\n");
    
    return NULL;
}

// Utility functions
const char* protocol_to_string(protocol_type_t protocol) {
    switch (protocol) {
        case PROTOCOL_MQTT: return "MQTT";
        case PROTOCOL_COAP: return "CoAP";
        case PROTOCOL_HTTP: return "HTTP";
        case PROTOCOL_DNS: return "DNS";
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
    
    printf("\n=== ENHANCED SESSION STATE TABLE ===\n");
    printf("Active sessions: %d/%d\n", g_session_table.session_count, MAX_CLIENTS);
    printf("%-15s %-8s %-10s %-8s %-15s %-8s %-8s %-20s\n", 
           "Session ID", "Socket", "Protocol", "State", "Client IP", "Messages", "Uptime", "Protocol Data");
    printf("----------------------------------------------------------------------------------------------------\n");
    
    time_t now = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_session_table.sessions[i].session_flags.flags & SESSION_FLAG_ACTIVE) {
            client_session_t* s = &g_session_table.sessions[i];
            
            char protocol_info[64] = {0};
            switch (s->protocol) {
                case PROTOCOL_MQTT:
                    snprintf(protocol_info, sizeof(protocol_info), "KA:%ds QoS:%d", 
                            s->protocol_data.mqtt.keep_alive_interval,
                            s->protocol_data.mqtt.qos_level);
                    break;
                case PROTOCOL_HTTP:
                    snprintf(protocol_info, sizeof(protocol_info), "%s %s", 
                            s->protocol_data.http.method,
                            s->protocol_data.http.uri);
                    break;
                case PROTOCOL_COAP:
                    snprintf(protocol_info, sizeof(protocol_info), "MsgID:%d Token:%d", 
                            s->protocol_data.coap.message_id_counter,
                            s->protocol_data.coap.token_length);
                    break;
                case PROTOCOL_DNS:
                    snprintf(protocol_info, sizeof(protocol_info), "Query:%s Type:%d", 
                            s->protocol_data.dns.query_name[0] ? s->protocol_data.dns.query_name : "N/A",
                            s->protocol_data.dns.query_type);
                    break;
                default:
                    strcpy(protocol_info, "N/A");
            }
            
            printf("%-15s %-8d %-10s %-8s %-15s %-8d %-8lds %-20s\n",
                   s->session_id,
                   s->socket_fd,
                   protocol_to_string(s->protocol),
                   state_to_string(s->state),
                   inet_ntoa(s->client_addr.sin_addr),
                   s->message_count,
                   now - s->created_at,
                   protocol_info);
        }
    }
    printf("=====================================================================================================\n\n");
    
    pthread_mutex_unlock(&g_session_table.table_mutex);
}

// Test functions for the new protocols
void test_protocols() {
    printf("\n=== PROTOCOL TEST EXAMPLES ===\n");
    
    // HTTP test
    printf("HTTP Test Request:\n");
    printf("GET /api/status HTTP/1.1\\r\\n");
    printf("Host: localhost:8080\\r\\n");
    printf("User-Agent: Test-Client/1.0\\r\\n");
    printf("Connection: keep-alive\\r\\n\\r\\n\n");
    
    // DNS test
    printf("DNS Test Query (hex):\n");
    printf("12 34 01 00 00 01 00 00 00 00 00 00\n");
    printf("07 65 78 61 6D 70 6C 65 03 63 6F 6D 00\n");
    printf("00 01 00 01\n");
    printf("(Query for example.com A record)\n\n");
    
    printf("Test with:\n");
    printf("TCP (HTTP/MQTT): telnet localhost 8080\n");
    printf("UDP (DNS/CoAP): nc -u localhost 8080\n");
    printf("==============================\n\n");
}

int main() {
    printf("Enhanced Protocol-Agnostic Middleware in C\n");
    printf("==========================================\n");
    printf("Supported Protocols:\n");
    printf("  TCP: MQTT, HTTP\n");
    printf("  UDP: CoAP, DNS\n");
    printf("==========================================\n");
    
    // Initialize session table
    pthread_mutex_init(&g_session_table.table_mutex, NULL);
    memset(&g_session_table, 0, sizeof(session_table_t));
    
    // Create sockets
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (tcp_socket == -1 || udp_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind sockets
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1 ||
        bind(udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        return 1;
    }
    
    if (listen(tcp_socket, 10) == -1) {
        perror("Listen failed");
        return 1;
    }
    
    printf("Server listening on port %d\n", SERVER_PORT);
    printf("TCP: MQTT, HTTP\n");
    printf("UDP: CoAP, DNS\n");
    printf("Waiting for clients...\n\n");
    
    // Show test examples
    test_protocols();
    
    // Start UDP handler thread
    pthread_t udp_thread;
    if (pthread_create(&udp_thread, NULL, udp_handler, &udp_socket) != 0) {
        perror("UDP thread creation failed");
        return 1;
    }
    
    // Main TCP server loop
    while (g_server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_socket = accept(tcp_socket, 
                                 (struct sockaddr*)&client_addr, 
                                 &client_addr_len);
        
        if (client_socket == -1) {
            if (g_server_running) {
                perror("Accept failed");
            }
            continue;
        }
        
        pthread_t client_thread;
        int* socket_arg = (int*)malloc(sizeof(int));
        *socket_arg = client_socket;
        
        if (pthread_create(&client_thread, NULL, client_handler, socket_arg) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(socket_arg);
            continue;
        }
        
        pthread_detach(client_thread);
        
        // Print session table periodically
        static int connection_count = 0;
        if (++connection_count % 3 == 0) {
            print_session_table();
        }
    }
    
    // Cleanup
    close(tcp_socket);
    close(udp_socket);
    pthread_mutex_destroy(&g_session_table.table_mutex);
    
    printf("Enhanced middleware shutdown complete\n");
    return 0;
}



// # Compile
// gcc -pthread -o middleware all_pro.cpp

// # Test HTTP (TCP)
// curl http://localhost:8080/api/test
// # or
// telnet localhost 8080
// GET /status HTTP/1.1
// Host: localhost

// # Test DNS (UDP)  
// dig @localhost -p 8080 example.com
// # or
// nc -u localhost 8080
// # (send DNS query bytes)

// # Test MQTT (TCP) - use mosquitto client
// mosquitto_pub -h localhost -p 8080 -t test/topic -m "hello"

// # Test CoAP (UDP) - use coap-client
// coap-client -m get coap://localhost:8080/status




// This is shaping up into a nice multi-protocol detector library 👌 — HTTP, DNS, MQTT, CoAP all in one place.
// Let me walk through what you have, check for pitfalls, and suggest where it could misclassify random UDP/TCP payloads.

// ✅ Strengths in your implementation

// HTTP

// Looks for a known method string at start (GET, POST, etc.).

// Ensures HTTP/x.x appears.

// Requires CRLF (\r\n).

// Good: rules out most non-HTTP text traffic.

// DNS

// Verifies header size (12 bytes).

// Checks OPCODE, RCODE.

// Parses QNAME with compression pointers and max label rules.

// Ensures QTYPE/QCLASS present.

// This is already a solid heuristic DNS validator.

// MQTT

// Validates first byte control packet type (1–14).

// Flags check against type.

// Parses Remaining Length correctly with continuation encoding.

// This weeds out most garbage.

// CoAP

// Checks version = 1 (bits 7–6).

// Token length ≤ 8.

// Header + token length fits in packet.

// Validates Code Class.

// Already enough for quick filtering.

// ⚠️ Weaknesses & Possible False Positives

// HTTP

// Casting data to char* and using strstr is unsafe if payload has no null terminator (\0).

// Right now, strstr(data_str, "HTTP/") might overrun beyond len.

// ➝ You need a bounded search (e.g., memmem or manual loop limited by len).

// DNS

// Record count sanity check (>100) is arbitrary. Some DNS servers return large responses (EDNS0, DNSSEC).

// Might reject valid but large DNS packets.

// MQTT

// Remaining Length parsing: you don’t validate remaining_length vs len. So a short/truncated MQTT message could pass initial checks.

// CoAP

// No option parsing yet, so random data could slip through if first byte happens to look like CoAP.

// Doesn’t check payload marker (0xFF).

// 🔧 Improvements

// HTTP: Replace strstr with a safe bounded search:

// const char* http_pos = NULL;
// for (size_t i = 0; i + 5 < len; i++) {
//     if (memcmp(data + i, "HTTP/", 5) == 0) {
//         http_pos = (const char*)(data + i);
//         break;
//     }
// }
// if (!http_pos) return 0;


// DNS: Consider allowing more records (instead of hard cutoff at 100). Maybe check len consistency instead.

// MQTT: After decoding Remaining Length, ensure byte_index + remaining_length <= len.

// CoAP: Add Option + Payload parsing like we discussed earlier (to avoid false positives).

// 🚦 Conclusion

// Right now:

// HTTP & DNS detectors are pretty strong but HTTP one is unsafe due to strstr.

// MQTT detector works but needs stricter remaining_length vs packet length check.

// CoAP detector is minimal, good for fast filtering but weaker against random traffic — option parsing would strengthen it.

// 👉 Do you want me to unify all four detectors into a single dispatcher function, e.g. detect_protocol(data, len) that returns an enum {HTTP, DNS, MQTT, COAP, UNKNOWN}? That way, you can throw arbitrary UDP/TCP packets at it and reliably classify.