#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Protocol types
typedef enum
{
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_HTTP = 3,
    PROTOCOL_DNS = 4
} protocol_type_t;

// Connection states
typedef enum
{
    CONN_LISTENING = 0,
    CONN_CONNECTED = 1,
    CONN_READING = 2,
    CONN_WRITING = 3,
    CONN_THROTTLED = 4,
    CONN_CLOSING = 5
} connection_state_t;

// Session states
typedef enum
{
    SESSION_CONNECTED = 0,
    SESSION_AUTHENTICATED = 1,
    SESSION_ACTIVE = 2,
    SESSION_DISCONNECTING = 3,
    SESSION_CLOSED = 4
} session_state_t;

// Session flags using efficient bit manipulation
typedef struct
{
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
#define SESSION_FLAG_ACTIVE (1U << 0)
#define SESSION_FLAG_AUTHENTICATED (1U << 1)
#define SESSION_FLAG_KEEPALIVE (1U << 2)
#define SESSION_FLAG_CLEAN_SESSION (1U << 3)
#define SESSION_FLAG_OBSERVE_ACTIVE (1U << 4)
#define SESSION_FLAG_HTTP_KEEPALIVE (1U << 5)
#define SESSION_FLAG_DNS_RECURSIVE (1U << 6)

// Protocol-specific data
typedef union
{
    struct
    {
        uint8_t qos_level;
        uint16_t keep_alive_interval;
        uint8_t protocol_version;
        char client_id[32];
    } mqtt;

    struct
    {
        uint16_t message_id_counter;
        uint8_t token[8];
        uint8_t token_length;
        uint32_t observe_sequence;
    } coap;

    struct
    {
        char method[16];
        char uri[128];
        char version[16];
        char host[64];
        char user_agent[64];
        int content_length;
        uint8_t connection_close;
    } http;

    struct
    {
        uint16_t transaction_id;
        uint16_t flags;
        uint16_t questions;
        uint16_t answers;
        char query_name[128];
        uint16_t query_type;
    } dns;

    uint8_t raw_data[256]; // Increased size for HTTP/DNS data
} protocol_data_t;

// Congestion control state
typedef struct
{
    uint32_t msgs_in_window;
    time_t window_start;
    uint32_t queue_depth;
    uint32_t congestion_window;
    uint32_t slow_start_threshold;
    uint32_t consecutive_drops;
    double backoff_factor;
    uint8_t in_slow_start;
} congestion_ctrl_t;

// Connection structure (unified from all three implementations)
typedef struct connection
{
    int fd;
    connection_state_t state;
    protocol_type_t protocol;
    struct sockaddr_in addr;

    // Session information
    char session_id[64];
    session_state_t session_state;
    session_flags_t session_flags;
    protocol_data_t protocol_data;

    // Buffers
    uint8_t read_buffer[4096];
    uint8_t write_buffer[4096];
    size_t read_pos;
    size_t write_pos;
    size_t bytes_to_write;

    // Congestion control
    congestion_ctrl_t congestion;

    // Timing and statistics
    time_t last_activity;
    time_t created_at;
    uint32_t message_count;
    uint32_t error_count;
    uint32_t total_messages;

    pthread_mutex_t session_mutex;

    // Linked list for cleanup
    struct connection *next;

    // Performance: pad to cache line boundary (64 bytes)
    char padding[0] __attribute__((aligned(64)));
} connection_t;

// Server state
typedef struct
{
    int epoll_fd;
    int tcp_socket;
    int udp_socket;
    connection_t connections[10000]; // MAX_CLIENTS
    connection_t *free_list;
    uint32_t active_connections;
    uint64_t total_bytes_processed;
    uint32_t system_load; // 0-100
    int session_count;
    pthread_mutex_t table_mutex;
    int next_session_index;
} server_state_t;

// Constants
#define MAX_EVENTS 1000
#define MAX_CLIENTS 10000
#define BUFFER_SIZE 4096
#define MAX_SESSION_ID 64
#define EPOLL_TIMEOUT 1000
#define SERVER_PORT 8080

// Congestion control parameters
#define RATE_LIMIT_WINDOW_SEC 1
#define MAX_MSGS_PER_SEC 100
#define MAX_QUEUE_DEPTH 1000
#define SLOW_START_THRESHOLD 64
#define CONGESTION_BACKOFF 0.5
#define MAX_RETRIES 3

#endif // TYPES_H
