#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Protocol types (enhanced with new protocols)
typedef enum
{
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_HTTP = 3,
    PROTOCOL_DNS = 4,
    PROTOCOL_TLS = 5,
    PROTOCOL_QUIC = 6
} protocol_type_t;

// TLS Content Types
#define TLS_CHANGE_CIPHER_SPEC 20
#define TLS_ALERT              21
#define TLS_HANDSHAKE          22
#define TLS_APPLICATION_DATA   23

// TLS Handshake Types
#define TLS_HELLO_REQUEST       0
#define TLS_CLIENT_HELLO        1
#define TLS_SERVER_HELLO        2
#define TLS_CERTIFICATE         11
#define TLS_SERVER_KEY_EXCHANGE 12
#define TLS_CERTIFICATE_REQUEST 13
#define TLS_SERVER_HELLO_DONE   14
#define TLS_CERTIFICATE_VERIFY  15
#define TLS_CLIENT_KEY_EXCHANGE 16
#define TLS_FINISHED            20

// Enhanced Protocol Detection Configuration
#define STATE_TABLE_SIZE 1024
#define CONNECTION_TIMEOUT 300 // 5 minutes
#define DETECTION_CONFIDENCE_HIGH 90
#define DETECTION_CONFIDENCE_MEDIUM 70
#define DETECTION_CONFIDENCE_LOW 50

// Dynamic Confidence System Configuration
#define MAX_CONFIDENCE_FEATURES 10
#define CONFIDENCE_HISTORY_SIZE 1000
#define ADAPTIVE_LEARNING_RATE 0.1
#define MIN_CONFIDENCE_THRESHOLD 30
#define MAX_CONFIDENCE_THRESHOLD 100

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
    // 7: tls_established (TLS)
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
#define SESSION_FLAG_TLS_ESTABLISHED (1U << 7)

// Enhanced TCP Connection State for Protocol Detection
typedef struct tcp_connection_state {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    protocol_type_t protocol;
    time_t last_seen;
    uint32_t seq_number;
    uint8_t confidence;
    struct tcp_connection_state *next;
} tcp_connection_state_t;

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

    struct
    {
        uint8_t content_type;
        uint16_t version;
        uint16_t record_length;
        uint8_t handshake_type;
        uint8_t cipher_suite[2];
    } tls;

    struct
    {
        uint32_t version;
        uint8_t packet_type;
        uint8_t connection_id_length;
        uint64_t connection_id;
    } quic;

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

    // Enhanced protocol detection
    uint8_t detection_confidence;
    uint32_t detection_attempts;
    time_t last_detection;

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

// Dynamic Confidence Features Structure
typedef struct {
    float entropy_score;           // Packet entropy (0-1)
    float pattern_strength;        // Pattern distinctiveness (0-1)
    float validation_depth;        // Number of validation checks passed (0-1)
    float header_consistency;      // Header field consistency (0-1)
    float payload_structure;       // Payload structure validity (0-1)
    float transport_compatibility; // Transport layer compatibility (0-1)
    float context_relevance;       // Context relevance score (0-1)
    float historical_accuracy;     // Historical detection accuracy (0-1)
    float false_positive_risk;     // False positive risk assessment (0-1)
    float protocol_specificity;    // Protocol-specific features (0-1)
} confidence_features_t;

// Protocol Accuracy Tracking Structure
typedef struct {
    uint32_t total_detections;
    uint32_t correct_detections;
    uint32_t false_positives;
    uint32_t false_negatives;
    float accuracy_rate;
    float precision_rate;
    float recall_rate;
    float f1_score;
    float confidence_adjustment;
    time_t last_update;
} protocol_accuracy_t;

// Confidence History Entry
typedef struct {
    protocol_type_t protocol;
    uint8_t predicted_confidence;
    uint8_t actual_confidence;
    uint8_t was_correct;
    time_t timestamp;
    confidence_features_t features;
} confidence_history_entry_t;

// Enhanced Statistics Structure
typedef struct {
    uint64_t total_packets;
    uint64_t identified_packets;
    uint64_t protocol_count[7]; // PROTOCOL_UNKNOWN to PROTOCOL_QUIC
    uint64_t detection_confidence_high;
    uint64_t detection_confidence_medium;
    uint64_t detection_confidence_low;
    uint64_t tcp_connections_tracked;
    uint64_t udp_packets_processed;
    
    // Dynamic confidence statistics
    protocol_accuracy_t protocol_accuracy[7]; // Per-protocol accuracy tracking
    confidence_history_entry_t confidence_history[CONFIDENCE_HISTORY_SIZE];
    uint32_t confidence_history_index;
    float average_confidence_error;
    float confidence_calibration_factor;
} enhanced_stats_t;

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
    
    // Enhanced protocol detection state
    tcp_connection_state_t *state_table[STATE_TABLE_SIZE];
    pthread_mutex_t detection_mutex;
    enhanced_stats_t enhanced_stats;
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
