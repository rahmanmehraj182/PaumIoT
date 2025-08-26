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

#define MAX_EVENTS 1000
#define MAX_CLIENTS 10000
#define BUFFER_SIZE 4096
#define EPOLL_TIMEOUT 1000
#define SERVER_PORT 8080

// Congestion control parameters
#define RATE_LIMIT_WINDOW_SEC 1
#define MAX_MSGS_PER_SEC 100
#define MAX_QUEUE_DEPTH 1000
#define SLOW_START_THRESHOLD 64
#define CONGESTION_BACKOFF 0.5
#define MAX_RETRIES 3

typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_HTTP = 3,
    PROTOCOL_DNS = 4
} protocol_type_t;

typedef enum {
    CONN_LISTENING = 0,
    CONN_CONNECTED = 1,
    CONN_READING = 2,
    CONN_WRITING = 3,
    CONN_THROTTLED = 4,
    CONN_CLOSING = 5
} connection_state_t;

// Congestion control state
typedef struct {
    uint32_t msgs_in_window;
    time_t window_start;
    uint32_t queue_depth;
    uint32_t congestion_window;
    uint32_t slow_start_threshold;
    uint32_t consecutive_drops;
    double backoff_factor;
    uint8_t in_slow_start;
} congestion_ctrl_t;

// Per-connection state
typedef struct connection {
    int fd;
    connection_state_t state;
    protocol_type_t protocol;
    struct sockaddr_in addr;
    
    // Buffers
    uint8_t read_buffer[BUFFER_SIZE];
    uint8_t write_buffer[BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t bytes_to_write;
    
    // Congestion control
    congestion_ctrl_t congestion;
    
    // Timing
    time_t last_activity;
    time_t created_at;
    uint32_t total_messages;
    
    // Linked list for cleanup
    struct connection* next;
} connection_t;

// Global state
typedef struct {
    int epoll_fd;
    int tcp_socket;
    int udp_socket;
    connection_t connections[MAX_CLIENTS];
    connection_t* free_list;
    uint32_t active_connections;
    uint64_t total_bytes_processed;
    uint32_t system_load; // 0-100
} server_state_t;

server_state_t g_server = {0};
volatile int g_running = 1;

// Function prototypes
int make_socket_nonblocking(int fd);
connection_t* allocate_connection(void);
void free_connection(connection_t* conn);
int add_to_epoll(int epoll_fd, int fd, uint32_t events, void* data);
int handle_tcp_accept(void);
int handle_udp_message(void);
protocol_type_t detect_protocol_fast(const uint8_t* data, size_t len);
int apply_congestion_control(connection_t* conn);
void update_congestion_window(connection_t* conn, int success);
int handle_connection_read(connection_t* conn);
int handle_connection_write(connection_t* conn);
void process_protocol_message(connection_t* conn, const uint8_t* data, size_t len);
void cleanup_stale_connections(void);
void print_server_stats(void);

// Initialize congestion control
void init_congestion_control(congestion_ctrl_t* cc) {
    cc->msgs_in_window = 0;
    cc->window_start = time(NULL);
    cc->queue_depth = 0;
    cc->congestion_window = 1; // Start with 1
    cc->slow_start_threshold = SLOW_START_THRESHOLD;
    cc->consecutive_drops = 0;
    cc->backoff_factor = 1.0;
    cc->in_slow_start = 1;
}

// Congestion control: check if we should accept new message
int apply_congestion_control(connection_t* conn) {
    congestion_ctrl_t* cc = &conn->congestion;
    time_t now = time(NULL);
    
    // Reset window if time elapsed
    if (now - cc->window_start >= RATE_LIMIT_WINDOW_SEC) {
        cc->msgs_in_window = 0;
        cc->window_start = now;
    }
    
    // Check rate limit
    if (cc->msgs_in_window >= MAX_MSGS_PER_SEC) {
        printf("[CONGESTION] Rate limit hit for fd=%d (%u msgs/sec)\n", 
               conn->fd, cc->msgs_in_window);
        conn->state = CONN_THROTTLED;
        cc->consecutive_drops++;
        update_congestion_window(conn, 0); // Failed transmission
        return 0; // Drop message
    }
    
    // Check queue depth
    if (cc->queue_depth >= MAX_QUEUE_DEPTH) {
        printf("[CONGESTION] Queue full for fd=%d (depth=%u)\n", 
               conn->fd, cc->queue_depth);
        conn->state = CONN_THROTTLED;
        cc->consecutive_drops++;
        update_congestion_window(conn, 0);
        return 0;
    }
    
    // Check congestion window
    if (cc->queue_depth >= cc->congestion_window) {
        printf("[CONGESTION] Window full for fd=%d (window=%u, queued=%u)\n", 
               conn->fd, cc->congestion_window, cc->queue_depth);
        return 0; // Wait for ACKs
    }
    
    // Accept message
    cc->msgs_in_window++;
    cc->queue_depth++;
    cc->consecutive_drops = 0;
    
    return 1; // Accept
}

// Update congestion window based on success/failure
void update_congestion_window(connection_t* conn, int success) {
    congestion_ctrl_t* cc = &conn->congestion;
    
    if (success) {
        // Successful transmission - increase window
        if (cc->in_slow_start) {
            // Slow start: exponential growth
            cc->congestion_window++;
            if (cc->congestion_window >= cc->slow_start_threshold) {
                cc->in_slow_start = 0;
                printf("[CONGESTION] fd=%d exiting slow start (window=%u)\n", 
                       conn->fd, cc->congestion_window);
            }
        } else {
            // Congestion avoidance: linear growth
            static uint32_t ack_count = 0;
            ack_count++;
            if (ack_count >= cc->congestion_window) {
                cc->congestion_window++;
                ack_count = 0;
            }
        }
        
        // Reset backoff
        cc->backoff_factor = 1.0;
        
        // Decrease queue depth
        if (cc->queue_depth > 0) {
            cc->queue_depth--;
        }
        
    } else {
        // Failed transmission - decrease window
        cc->slow_start_threshold = cc->congestion_window / 2;
        if (cc->slow_start_threshold < 2) cc->slow_start_threshold = 2;
        
        cc->congestion_window = cc->slow_start_threshold;
        cc->in_slow_start = 0;
        
        // Apply exponential backoff
        cc->backoff_factor *= (1.0 + CONGESTION_BACKOFF);
        if (cc->backoff_factor > 8.0) cc->backoff_factor = 8.0;
        
        printf("[CONGESTION] fd=%d congestion detected - window reduced to %u (backoff=%.2f)\n", 
               conn->fd, cc->congestion_window, cc->backoff_factor);
    }
}

// Make socket non-blocking
int make_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    
    return 0;
}

// Connection management
connection_t* allocate_connection(void) {
    if (g_server.free_list) {
        connection_t* conn = g_server.free_list;
        g_server.free_list = conn->next;
        memset(conn, 0, sizeof(connection_t));
        return conn;
    }
    
    // Find free slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd == 0) {
            return &g_server.connections[i];
        }
    }
    
    return NULL; // No free connections
}

void free_connection(connection_t* conn) {
    if (conn->fd > 0) {
        epoll_ctl(g_server.epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
        close(conn->fd);
    }
    
    printf("[CONNECTION] Freed fd=%d (total_msgs=%u, uptime=%lds)\n",
           conn->fd, conn->total_messages, 
           time(NULL) - conn->created_at);
    
    // Add to free list
    conn->next = g_server.free_list;
    g_server.free_list = conn;
    conn->fd = 0;
    
    g_server.active_connections--;
}

int add_to_epoll(int epoll_fd, int fd, uint32_t events, void* data) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl ADD");
        return -1;
    }
    
    return 0;
}

// Fast protocol detection (optimized for event loop)
protocol_type_t detect_protocol_fast(const uint8_t* data, size_t len) {
    if (len < 2) return PROTOCOL_UNKNOWN;
    
    // HTTP - check for methods
    if (len >= 14 && (memcmp(data, "GET ", 4) == 0 || 
                      memcmp(data, "POST ", 5) == 0 ||
                      memcmp(data, "PUT ", 4) == 0 ||
                      memcmp(data, "DELETE ", 7) == 0)) {
        return PROTOCOL_HTTP;
    }
    
    // DNS - check header
    if (len >= 12) {
        uint16_t flags = (data[2] << 8) | data[3];
        uint8_t opcode = (flags >> 11) & 0x0F;
        if (opcode <= 2) {
            return PROTOCOL_DNS;
        }
    }
    
    // MQTT - check message type
    uint8_t msg_type = (data[0] >> 4) & 0x0F;
    if (msg_type >= 1 && msg_type <= 14) {
        return PROTOCOL_MQTT;
    }
    
    // CoAP - check version and type
    uint8_t version = (data[0] >> 6) & 0x03;
    uint8_t msg_type_coap = (data[0] >> 4) & 0x03;
    if (version == 1 && msg_type_coap <= 3) {
        return PROTOCOL_COAP;
    }
    
    return PROTOCOL_UNKNOWN;
}

// Handle new TCP connection
int handle_tcp_accept(void) {
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
    
    if (make_socket_nonblocking(client_fd) == -1) {
        close(client_fd);
        return -1;
    }
    
    connection_t* conn = allocate_connection();
    if (!conn) {
        printf("[ACCEPT] No free connections available\n");
        close(client_fd);
        return -1;
    }
    
    conn->fd = client_fd;
    conn->state = CONN_CONNECTED;
    conn->addr = client_addr;
    conn->created_at = time(NULL);
    conn->last_activity = time(NULL);
    init_congestion_control(&conn->congestion);
    
    if (add_to_epoll(g_server.epoll_fd, client_fd, 
                     EPOLLIN | EPOLLET, conn) == -1) {
        free_connection(conn);
        return -1;
    }
    
    g_server.active_connections++;
    printf("[ACCEPT] New connection fd=%d from %s:%d (total: %u)\n",
           client_fd, inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port), g_server.active_connections);
    
    return 0;
}

// Handle UDP message (stateless)
int handle_udp_message(void) {
    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    ssize_t received = recvfrom(g_server.udp_socket, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&client_addr, &addr_len);
    
    if (received <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        return -1;
    }
    
    protocol_type_t protocol = detect_protocol_fast(buffer, received);
    printf("[UDP] Received %zd bytes from %s:%d - Protocol: %d\n",
           received, inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port), protocol);
    
    // Simple UDP response based on protocol
    const char* response = NULL;
    size_t resp_len = 0;
    
    switch (protocol) {
        case PROTOCOL_DNS: {
            // Simple DNS response
            static uint8_t dns_response[] = {
                0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                0x07, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x03, 0x63, 0x6F, 0x6D, 0x00,
                0x00, 0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x2C,
                0x00, 0x04, 0x7F, 0x00, 0x00, 0x01
            };
            dns_response[0] = buffer[0]; // Copy transaction ID
            dns_response[1] = buffer[1];
            response = (const char*)dns_response;
            resp_len = sizeof(dns_response);
            break;
        }
        case PROTOCOL_COAP: {
            static const char coap_response[] = "\x60\x45\x00\x01\xFF{\"status\":\"ok\"}";
            response = coap_response;
            resp_len = strlen(coap_response);
            break;
        }
        default:
            response = "ERROR: Unsupported UDP protocol";
            resp_len = strlen(response);
    }
    
    if (response) {
        sendto(g_server.udp_socket, response, resp_len, 0,
               (struct sockaddr*)&client_addr, addr_len);
        printf("[UDP] Response sent (%zu bytes)\n", resp_len);
    }
    
    g_server.total_bytes_processed += received + resp_len;
    return 0;
}

// Handle connection read
int handle_connection_read(connection_t* conn) {
    ssize_t bytes_read = read(conn->fd, 
                             conn->read_buffer + conn->read_pos,
                             sizeof(conn->read_buffer) - conn->read_pos);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("[READ] Connection fd=%d closed by peer\n", conn->fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("[READ] Error on fd=%d: %s\n", conn->fd, strerror(errno));
        }
        return -1; // Connection should be closed
    }
    
    conn->read_pos += bytes_read;
    conn->last_activity = time(NULL);
    g_server.total_bytes_processed += bytes_read;
    
    printf("[READ] fd=%d read %zd bytes (total buffered: %zu)\n",
           conn->fd, bytes_read, conn->read_pos);
    
    // Apply congestion control
    if (!apply_congestion_control(conn)) {
        printf("[READ] Message dropped due to congestion control\n");
        update_congestion_window(conn, 0);
        return 0; // Don't close, just drop message
    }
    
    // Detect protocol if not known
    if (conn->protocol == PROTOCOL_UNKNOWN && conn->read_pos >= 4) {
        conn->protocol = detect_protocol_fast(conn->read_buffer, conn->read_pos);
        printf("[READ] Protocol detected: %d for fd=%d\n", conn->protocol, conn->fd);
    }
    
    // Process complete messages
    if (conn->read_pos > 0) {
        process_protocol_message(conn, conn->read_buffer, conn->read_pos);
        update_congestion_window(conn, 1); // Successful processing
        
        // Reset read buffer
        conn->read_pos = 0;
        conn->total_messages++;
    }
    
    return 0;
}

// Process protocol-specific message
void process_protocol_message(connection_t* conn, const uint8_t* data, size_t len) {
    const char* response = NULL;
    size_t resp_len = 0;
    
    switch (conn->protocol) {
        case PROTOCOL_HTTP: {
            const char* http_resp = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 27\r\n"
                "Connection: keep-alive\r\n\r\n"
                "{\"message\":\"Hello HTTP\"}";
            response = http_resp;
            resp_len = strlen(http_resp);
            break;
        }
        case PROTOCOL_MQTT: {
            uint8_t msg_type = (data[0] >> 4) & 0x0F;
            if (msg_type == 1) { // CONNECT
                static const char connack[] = "\x20\x02\x00\x00";
                response = connack;
                resp_len = 4;
            } else if (msg_type == 12) { // PINGREQ
                static const char pingresp[] = "\xD0\x00";
                response = pingresp;
                resp_len = 2;
            }
            break;
        }
        default:
            response = "UNSUPPORTED";
            resp_len = 11;
    }
    
    if (response && resp_len <= sizeof(conn->write_buffer) - conn->bytes_to_write) {
        memcpy(conn->write_buffer + conn->bytes_to_write, response, resp_len);
        conn->bytes_to_write += resp_len;
        
        // Add EPOLLOUT to write response
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = conn;
        epoll_ctl(g_server.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
        
        printf("[PROCESS] Queued %zu byte response for fd=%d\n", resp_len, conn->fd);
    }
}

// Handle connection write
int handle_connection_write(connection_t* conn) {
    if (conn->bytes_to_write == 0) {
        return 0;
    }
    
    ssize_t bytes_written = write(conn->fd, 
                                 conn->write_buffer + conn->write_pos,
                                 conn->bytes_to_write - conn->write_pos);
    
    if (bytes_written <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("[WRITE] Error on fd=%d: %s\n", conn->fd, strerror(errno));
            return -1;
        }
        return 0; // Would block, try later
    }
    
    conn->write_pos += bytes_written;
    printf("[WRITE] fd=%d wrote %zd bytes (%zu/%zu)\n",
           conn->fd, bytes_written, conn->write_pos, conn->bytes_to_write);
    
    if (conn->write_pos >= conn->bytes_to_write) {
        // All data written
        conn->write_pos = 0;
        conn->bytes_to_write = 0;
        
        // Remove EPOLLOUT
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = conn;
        epoll_ctl(g_server.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
        
        printf("[WRITE] fd=%d write complete\n", conn->fd);
    }
    
    return 0;
}

// Cleanup stale connections
void cleanup_stale_connections(void) {
    time_t now = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connection_t* conn = &g_server.connections[i];
        
        if (conn->fd > 0) {
            // Check for stale connections (60 seconds)
            if (now - conn->last_activity > 60) {
                printf("[CLEANUP] Closing stale connection fd=%d (idle: %lds)\n",
                       conn->fd, now - conn->last_activity);
                free_connection(conn);
                cleaned++;
            }
            // Reset throttled connections
            else if (conn->state == CONN_THROTTLED && 
                     now - conn->last_activity > 5) {
                conn->state = CONN_CONNECTED;
                conn->congestion.msgs_in_window = 0;
                printf("[CLEANUP] Reset throttled connection fd=%d\n", conn->fd);
            }
        }
    }
    
    if (cleaned > 0) {
        printf("[CLEANUP] Cleaned %d stale connections\n", cleaned);
    }
}

// Print server statistics
void print_server_stats(void) {
    printf("\n=== SERVER STATISTICS ===\n");
    printf("Active connections: %u/%d\n", g_server.active_connections, MAX_CLIENTS);
    printf("Total bytes processed: %llu MB\n", g_server.total_bytes_processed / (1024*1024));
    printf("System load: %u%%\n", g_server.system_load);
    
    // Per-connection stats
    int throttled = 0, slow_start = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd > 0) {
            if (g_server.connections[i].state == CONN_THROTTLED) throttled++;
            if (g_server.connections[i].congestion.in_slow_start) slow_start++;
        }
    }
    printf("Throttled connections: %d\n", throttled);
    printf("Slow start connections: %d\n", slow_start);
    printf("=========================\n\n");
}

// Main server loop
int main() {
    printf("Event-Driven Protocol Middleware with Congestion Control\n");
    printf("========================================================\n");
    
    // Create epoll
    g_server.epoll_fd = epoll_create1(0);
    if (g_server.epoll_fd == -1) {
        perror("epoll_create1");
        return 1;
    }
    
    // Create and setup sockets
    g_server.tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    g_server.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (g_server.tcp_socket == -1 || g_server.udp_socket == -1) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(g_server.tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(g_server.udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    make_socket_nonblocking(g_server.tcp_socket);
    make_socket_nonblocking(g_server.udp_socket);
    
    // Bind sockets
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(g_server.tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1 ||
        bind(g_server.udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return 1;
    }
    
    if (listen(g_server.tcp_socket, 128) == -1) {
        perror("listen");
        return 1;
    }
    
    // Add sockets to epoll
    if (add_to_epoll(g_server.epoll_fd, g_server.tcp_socket, EPOLLIN, &g_server.tcp_socket) == -1 ||
        add_to_epoll(g_server.epoll_fd, g_server.udp_socket, EPOLLIN, &g_server.udp_socket) == -1) {
        return 1;
    }
    
    printf("Server listening on port %d\n", SERVER_PORT);
    printf("Event-driven architecture with congestion control active\n\n");
    
    // Main event loop
    struct epoll_event events[MAX_EVENTS];
    time_t last_cleanup = time(NULL);
    time_t last_stats = time(NULL);
    
    while (g_running) {
        int event_count = epoll_wait(g_server.epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        
        if (event_count == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        
        // Process events
        for (int i = 0; i < event_count; i++) {
            struct epoll_event* ev = &events[i];
            void* data = ev->data.ptr;
            
            if (data == &g_server.tcp_socket) {
                // New TCP connection
                handle_tcp_accept();
            }
            else if (data == &g_server.udp_socket) {
                // UDP message
                handle_udp_message();
            }
            else {
                // Existing connection
                connection_t* conn = (connection_t*)data;
                
                if (ev->events & (EPOLLERR | EPOLLHUP)) {
                    printf("[EVENT] Connection fd=%d error/hangup\n", conn->fd);
                    free_connection(conn);
                    continue;
                }
                
                if (ev->events & EPOLLIN) {
                    if (handle_connection_read(conn) == -1) {
                        free_connection(conn);
                        continue;
                    }
                }
                
                if (ev->events & EPOLLOUT) {
                    if (handle_connection_write(conn) == -1) {
                        free_connection(conn);
                        continue;
                    }
                }
            }
        }
        
        time_t now = time(NULL);
        
        // Periodic cleanup (every 30 seconds)
        if (now - last_cleanup > 30) {
            cleanup_stale_connections();
            last_cleanup = now;
        }
        
        // Print stats (every 60 seconds)
        if (now - last_stats > 60) {
            print_server_stats();
            last_stats = now;
        }
    }
    
    // Cleanup
    close(g_server.tcp_socket);
    close(g_server.udp_socket);
    close(g_server.epoll_fd);
    
    printf("Server shutdown complete\n");
    return 0;
}