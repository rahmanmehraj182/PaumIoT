// Enhanced state management structures
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>

#define MAX_EVENTS 1000
#define MAX_CLIENTS 10000
#define BUFFER_SIZE 4096
#define EPOLL_TIMEOUT 1000
#define SERVER_PORT 8080
#define CONNECTION_TIMEOUT 300  // 5 minutes
#define STATE_TRANSITION_LOG 1

// State transition matrix for validation
typedef enum {
    CONN_INVALID = -1,
    CONN_FREE = 0,
    CONN_LISTENING = 1,
    CONN_CONNECTED = 2,
    CONN_READING = 3,
    CONN_WRITING = 4,
    CONN_THROTTLED = 5,
    CONN_CLOSING = 6,
    CONN_CLOSED = 7,
    CONN_STATE_MAX
} connection_state_t;

// Protocol types
typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_HTTP = 3,
    PROTOCOL_DNS = 4
} protocol_type_t;

// Connection state transition table
static const int valid_transitions[CONN_STATE_MAX][CONN_STATE_MAX] = {
    // FROM\TO:  FREE LSTN CONN READ WRIT THRT CLSG CLSD
    [CONN_FREE]      = {1, 1, 0, 0, 0, 0, 0, 0},
    [CONN_LISTENING] = {1, 1, 1, 0, 0, 0, 1, 0},
    [CONN_CONNECTED] = {0, 0, 1, 1, 1, 1, 1, 0},
    [CONN_READING]   = {0, 0, 1, 1, 1, 1, 1, 0},
    [CONN_WRITING]   = {0, 0, 1, 1, 1, 1, 1, 0},
    [CONN_THROTTLED] = {0, 0, 1, 1, 1, 1, 1, 0},
    [CONN_CLOSING]   = {0, 0, 0, 0, 0, 0, 1, 1},
    [CONN_CLOSED]    = {1, 0, 0, 0, 0, 0, 0, 1}
};

// Enhanced connection structure with proper state management
typedef struct connection {
    // Connection identity
    uint32_t id;                    // Unique connection ID
    int fd;
    connection_state_t state;
    connection_state_t prev_state;  // For debugging transitions
    protocol_type_t protocol;
    struct sockaddr_in addr;
    
    // State transition tracking
    uint32_t state_changes;
    time_t last_state_change;
    char state_history[16];         // Ring buffer of last 16 states
    uint8_t state_history_pos;
    
    // Timing and activity
    time_t created_at;
    time_t last_activity;
    time_t last_read;
    time_t last_write;
    
    // Buffer management
    uint8_t read_buffer[BUFFER_SIZE];
    uint8_t write_buffer[BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t bytes_to_write;
    
    // Statistics
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint32_t total_messages;
    uint32_t error_count;
    
    // Congestion control (simplified)
    uint32_t msgs_in_window;
    time_t window_start;
    uint32_t consecutive_drops;
    
    // List management
    struct connection* next;        // Free list or cleanup list
    struct connection* prev;        // Double-linked for active list
    uint8_t in_active_list;        // Flag to prevent double-linking
    
    // Validation
    uint32_t magic;                // Magic number for validation
} connection_t;

#define CONNECTION_MAGIC 0xDEADBEEF

// Enhanced connection hash table for O(1) lookups
typedef struct {
    connection_t* buckets[1024];   // Hash table for fd -> connection
    uint32_t bucket_counts[1024];
} connection_hash_t;

// Global server state with proper state management
typedef struct {
    int epoll_fd;
    int tcp_socket;
    int udp_socket;
    
    // Connection management
    connection_t connections[MAX_CLIENTS];
    connection_hash_t hash_table;
    connection_t* free_list;
    connection_t* active_list;      // Double-linked list of active connections
    
    // ID management
    uint32_t next_conn_id;
    uint32_t active_connections;
    uint32_t total_connections_created;
    
    // Statistics
    uint64_t total_bytes_processed;
    uint32_t state_transition_errors;
    uint32_t cleanup_operations;
    
    // Performance metrics
    time_t last_cleanup;
    time_t last_stats;
    uint32_t events_processed;
    
} server_state_t;

server_state_t g_server = {0};
static const char* state_names[] = {
    "FREE", "LISTENING", "CONNECTED", "READING", 
    "WRITING", "THROTTLED", "CLOSING", "CLOSED"
};

// Function prototypes
int validate_connection(connection_t* conn);
int transition_connection_state(connection_t* conn, connection_state_t new_state);
uint32_t hash_fd(int fd);
connection_t* lookup_connection(int fd);
void insert_connection_hash(connection_t* conn);
void remove_connection_hash(connection_t* conn);
void add_to_active_list(connection_t* conn);
void remove_from_active_list(connection_t* conn);
connection_t* allocate_connection_enhanced(void);
void free_connection_enhanced(connection_t* conn);
void cleanup_connection_state(connection_t* conn);
void print_connection_state(connection_t* conn);
void validate_server_state(void);

// Hash function for file descriptors
uint32_t hash_fd(int fd) {
    return ((uint32_t)fd * 2654435761U) % 1024;
}

// Connection validation
int validate_connection(connection_t* conn) {
    if (!conn) return 0;
    if (conn->magic != CONNECTION_MAGIC) return 0;
    if (conn->state < CONN_FREE || conn->state >= CONN_STATE_MAX) return 0;
    if (conn->fd < 0 && conn->state != CONN_FREE && conn->state != CONN_CLOSED) return 0;
    return 1;
}

// State transition with validation and logging
int transition_connection_state(connection_t* conn, connection_state_t new_state) {
    if (!validate_connection(conn)) {
        printf("[ERROR] Invalid connection in state transition\n");
        return -1;
    }
    
    connection_state_t old_state = conn->state;
    
    // Validate transition
    if (!valid_transitions[old_state][new_state]) {
        printf("[ERROR] Invalid state transition fd=%d: %s -> %s\n",
               conn->fd, state_names[old_state], state_names[new_state]);
        g_server.state_transition_errors++;
        return -1;
    }
    
    // Update state
    conn->prev_state = old_state;
    conn->state = new_state;
    conn->state_changes++;
    conn->last_state_change = time(NULL);
    
    // Update state history ring buffer
    conn->state_history[conn->state_history_pos] = (char)new_state;
    conn->state_history_pos = (conn->state_history_pos + 1) % 16;
    
    // State-specific actions
    switch (new_state) {
        case CONN_CONNECTED:
            conn->last_activity = time(NULL);
            break;
        case CONN_READING:
            conn->last_read = time(NULL);
            break;
        case CONN_WRITING:
            conn->last_write = time(NULL);
            break;
        case CONN_THROTTLED:
            conn->consecutive_drops++;
            break;
        case CONN_CLOSING:
        case CONN_CLOSED:
            conn->last_activity = time(NULL);
            break;
        default:
            break;
    }
    
#if STATE_TRANSITION_LOG
    printf("[STATE] fd=%d id=%u: %s -> %s (changes: %u)\n",
           conn->fd, conn->id, state_names[old_state], 
           state_names[new_state], conn->state_changes);
#endif
    
    return 0;
}

// Hash table operations
connection_t* lookup_connection(int fd) {
    uint32_t hash = hash_fd(fd);
    connection_t* conn = g_server.hash_table.buckets[hash];
    
    while (conn) {
        if (conn->fd == fd && validate_connection(conn)) {
            return conn;
        }
        conn = conn->next;
    }
    return NULL;
}

void insert_connection_hash(connection_t* conn) {
    if (!validate_connection(conn)) return;
    
    uint32_t hash = hash_fd(conn->fd);
    conn->next = g_server.hash_table.buckets[hash];
    g_server.hash_table.buckets[hash] = conn;
    g_server.hash_table.bucket_counts[hash]++;
}

void remove_connection_hash(connection_t* conn) {
    if (!validate_connection(conn)) return;
    
    uint32_t hash = hash_fd(conn->fd);
    connection_t** current = &g_server.hash_table.buckets[hash];
    
    while (*current) {
        if (*current == conn) {
            *current = conn->next;
            g_server.hash_table.bucket_counts[hash]--;
            conn->next = NULL;
            return;
        }
        current = &(*current)->next;
    }
}

// Active connection list management
void add_to_active_list(connection_t* conn) {
    if (!validate_connection(conn) || conn->in_active_list) return;
    
    conn->next = g_server.active_list;
    conn->prev = NULL;
    if (g_server.active_list) {
        g_server.active_list->prev = conn;
    }
    g_server.active_list = conn;
    conn->in_active_list = 1;
}

void remove_from_active_list(connection_t* conn) {
    if (!validate_connection(conn) || !conn->in_active_list) return;
    
    if (conn->prev) {
        conn->prev->next = conn->next;
    } else {
        g_server.active_list = conn->next;
    }
    
    if (conn->next) {
        conn->next->prev = conn->prev;
    }
    
    conn->next = conn->prev = NULL;
    conn->in_active_list = 0;
}

// Enhanced connection allocation
connection_t* allocate_connection_enhanced(void) {
    connection_t* conn = NULL;
    
    // Try free list first
    if (g_server.free_list) {
        conn = g_server.free_list;
        g_server.free_list = conn->next;
    } else {
        // Find free slot in array
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_server.connections[i].state == CONN_FREE) {
                conn = &g_server.connections[i];
                break;
            }
        }
    }
    
    if (!conn) {
        printf("[ERROR] No free connections available\n");
        return NULL;
    }
    
    // Initialize connection
    memset(conn, 0, sizeof(connection_t));
    conn->magic = CONNECTION_MAGIC;
    conn->id = ++g_server.next_conn_id;
    conn->state = CONN_FREE;
    conn->created_at = time(NULL);
    conn->last_activity = time(NULL);
    
    g_server.total_connections_created++;
    return conn;
}

// Enhanced connection cleanup
void cleanup_connection_state(connection_t* conn) {
    if (!validate_connection(conn)) return;
    
    // Remove from hash table
    if (conn->fd > 0) {
        remove_connection_hash(conn);
    }
    
    // Remove from active list
    remove_from_active_list(conn);
    
    // Close file descriptor and remove from epoll
    if (conn->fd > 0) {
        epoll_ctl(g_server.epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
        close(conn->fd);
        conn->fd = -1;
    }
    
    // Transition to closed state
    transition_connection_state(conn, CONN_CLOSED);
    
    // Update statistics
    if (conn->state != CONN_FREE) {
        g_server.active_connections--;
    }
    
    printf("[CLEANUP] Connection id=%u cleaned (msgs=%u, bytes_r=%llu, bytes_w=%llu, errors=%u)\n",
           conn->id, conn->total_messages, conn->bytes_read, 
           conn->bytes_written, conn->error_count);
}

void free_connection_enhanced(connection_t* conn) {
    if (!validate_connection(conn)) return;
    
    cleanup_connection_state(conn);
    
    // Transition to free state
    transition_connection_state(conn, CONN_FREE);
    
    // Add to free list
    conn->next = g_server.free_list;
    g_server.free_list = conn;
    
    // Clear sensitive data but keep magic for validation
    memset(conn, 0, sizeof(connection_t));
    conn->magic = CONNECTION_MAGIC;
    conn->state = CONN_FREE;
    
    g_server.cleanup_operations++;
}

// Print connection state for debugging
void print_connection_state(connection_t* conn) {
    if (!validate_connection(conn)) {
        printf("[DEBUG] Invalid connection pointer\n");
        return;
    }
    
    printf("[DEBUG] Connection id=%u fd=%d state=%s prev=%s changes=%u\n",
           conn->id, conn->fd, state_names[conn->state], 
           state_names[conn->prev_state], conn->state_changes);
    
    printf("        Activity: created=%ld last=%ld read=%ld write=%ld\n",
           conn->created_at, conn->last_activity, conn->last_read, conn->last_write);
    
    printf("        Stats: msgs=%u bytes_r=%llu bytes_w=%llu errors=%u drops=%u\n",
           conn->total_messages, conn->bytes_read, conn->bytes_written, 
           conn->error_count, conn->consecutive_drops);
    
    // Print state history
    printf("        History: ");
    for (int i = 0; i < 16; i++) {
        int idx = (conn->state_history_pos + i) % 16;
        if (conn->state_history[idx] > 0 && conn->state_history[idx] < CONN_STATE_MAX) {
            printf("%s ", state_names[conn->state_history[idx]]);
        }
    }
    printf("\n");
}

// Validate entire server state
void validate_server_state(void) {
    uint32_t active_count = 0;
    uint32_t hash_conflicts = 0;
    uint32_t invalid_connections = 0;
    
    // Check all connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connection_t* conn = &g_server.connections[i];
        
        if (!validate_connection(conn)) {
            invalid_connections++;
            continue;
        }
        
        if (conn->state != CONN_FREE && conn->state != CONN_CLOSED) {
            active_count++;
        }
    }
    
    // Check hash table distribution
    for (int i = 0; i < 1024; i++) {
        if (g_server.hash_table.bucket_counts[i] > 10) {
            hash_conflicts++;
        }
    }
    
    printf("[VALIDATION] Active: %u/%u Invalid: %u Hash conflicts: %u\n",
           active_count, g_server.active_connections, invalid_connections, hash_conflicts);
    
    if (active_count != g_server.active_connections) {
        printf("[WARNING] Active connection count mismatch!\n");
    }
}

// Enhanced statistics with state information
void print_enhanced_server_stats(void) {
    printf("\n=== ENHANCED SERVER STATISTICS ===\n");
    printf("Connections: %u active, %u total created\n", 
           g_server.active_connections, g_server.total_connections_created);
    printf("State transitions: %u errors occurred\n", g_server.state_transition_errors);
    printf("Cleanup operations: %u performed\n", g_server.cleanup_operations);
    printf("Events processed: %u\n", g_server.events_processed);
    printf("Data processed: %llu MB\n", g_server.total_bytes_processed / (1024*1024));
    
    // Per-state breakdown
    uint32_t state_counts[CONN_STATE_MAX] = {0};
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connection_t* conn = &g_server.connections[i];
        if (validate_connection(conn)) {
            state_counts[conn->state]++;
        }
    }
    
    printf("State distribution:\n");
    for (int i = 0; i < CONN_STATE_MAX; i++) {
        if (state_counts[i] > 0) {
            printf("  %s: %u\n", state_names[i], state_counts[i]);
        }
    }
    
    // Hash table efficiency
    uint32_t max_bucket = 0, empty_buckets = 0;
    for (int i = 0; i < 1024; i++) {
        if (g_server.hash_table.bucket_counts[i] > max_bucket) {
            max_bucket = g_server.hash_table.bucket_counts[i];
        }
        if (g_server.hash_table.bucket_counts[i] == 0) {
            empty_buckets++;
        }
    }
    
    printf("Hash table: max bucket=%u, empty=%u/1024\n", max_bucket, empty_buckets);
    printf("===================================\n\n");
    
    // Run validation
    validate_server_state();
}

// Example usage in main event loop
int handle_tcp_accept_enhanced(void) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(g_server.tcp_socket, 
                          (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return -1;
    }
    
    // Allocate connection with enhanced state management
    connection_t* conn = allocate_connection_enhanced();
    if (!conn) {
        close(client_fd);
        return -1;
    }
    
    // Initialize connection properly
    conn->fd = client_fd;
    conn->addr = client_addr;
    
    // Transition through proper states
    if (transition_connection_state(conn, CONN_CONNECTED) != 0) {
        free_connection_enhanced(conn);
        close(client_fd);
        return -1;
    }
    
    // Add to hash table and active list
    insert_connection_hash(conn);
    add_to_active_list(conn);
    g_server.active_connections++;
    
    // Set up epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = conn;
    
    if (epoll_ctl(g_server.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        free_connection_enhanced(conn);
        return -1;
    }
    
    printf("[ACCEPT] New connection id=%u fd=%d from %s:%d\n",
           conn->id, client_fd, inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
    
    return 0;
}