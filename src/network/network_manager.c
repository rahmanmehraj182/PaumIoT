#include "../../include/network/network_manager.h"
#include "../../include/protocol/protocol_detector.h"
#include "../../include/protocol/protocol_handlers.h"
#include "../../include/session/session_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

extern server_state_t g_server;

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
    
    // Remove from session table
    remove_session(conn->fd);
    
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
    
    // Create session first to get the connection object
    int session_index = create_session(client_fd, &client_addr, PROTOCOL_UNKNOWN);
    if (session_index == -1) {
        printf("[ACCEPT] Failed to create session for fd=%d\n", client_fd);
        close(client_fd);
        return -1;
    }
    
    // Use the connection object from the session (same memory)
    connection_t* conn = &g_server.connections[session_index];
    
    // Initialize connection state (session already has basic info)
    conn->state = CONN_CONNECTED;
    conn->detection_confidence = 0;
    conn->detection_attempts = 0;
    conn->last_detection = 0;
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
    
    uint8_t confidence = 0;
    protocol_type_t protocol = detect_protocol_enhanced(buffer, received, &confidence, 0);
    printf("[UDP] Received %zd bytes from %s:%d - Protocol: %s (confidence: %d%%)\n",
           received, inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port), protocol_to_string(protocol), confidence);
    
    // Update enhanced statistics
    g_server.enhanced_stats.total_packets++;
    if (protocol != PROTOCOL_UNKNOWN) {
        g_server.enhanced_stats.identified_packets++;
        g_server.enhanced_stats.protocol_count[protocol]++;
        g_server.enhanced_stats.udp_packets_processed++;
        
        if (confidence >= DETECTION_CONFIDENCE_HIGH) {
            g_server.enhanced_stats.detection_confidence_high++;
        } else if (confidence >= DETECTION_CONFIDENCE_MEDIUM) {
            g_server.enhanced_stats.detection_confidence_medium++;
        } else if (confidence >= DETECTION_CONFIDENCE_LOW) {
            g_server.enhanced_stats.detection_confidence_low++;
        }
    }
    
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
        default: {
            static const char error_response[] = "ERROR: Unsupported UDP protocol";
            response = error_response;
            resp_len = strlen(error_response);
            break;
        }
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
    if (conn->protocol == PROTOCOL_UNKNOWN && conn->read_pos >= 2) {
        uint8_t confidence = 0;
        protocol_type_t detected_protocol = detect_protocol_enhanced(conn->read_buffer, conn->read_pos, &confidence, 1);
        
        // Update connection protocol info
        conn->protocol = detected_protocol;
        conn->detection_confidence = confidence;
        conn->detection_attempts++;
        conn->last_detection = time(NULL);
        
        printf("[READ] Protocol detected: %s for fd=%d (confidence: %d%%)\n", 
               protocol_to_string(detected_protocol), conn->fd, confidence);
        
        // Update session protocol atomically
        if (detected_protocol != PROTOCOL_UNKNOWN) {
            update_session_protocol(conn->fd, detected_protocol);
            
            // Print immediate session state for debugging
            printf("[DEBUG] Session state after protocol detection:\n");
            print_session_table();
            
            // Update enhanced statistics
            g_server.enhanced_stats.identified_packets++;
            g_server.enhanced_stats.protocol_count[detected_protocol]++;
            
            if (confidence >= DETECTION_CONFIDENCE_HIGH) {
                g_server.enhanced_stats.detection_confidence_high++;
            } else if (confidence >= DETECTION_CONFIDENCE_MEDIUM) {
                g_server.enhanced_stats.detection_confidence_medium++;
            } else if (confidence >= DETECTION_CONFIDENCE_LOW) {
                g_server.enhanced_stats.detection_confidence_low++;
            }
        }
    }
    
    // Process complete messages
    if (conn->read_pos > 0) {
        printf("[READ] Processing %zu bytes for protocol %s\n", 
               conn->read_pos, protocol_to_string(conn->protocol));
        
        process_protocol_message(conn, conn->read_buffer, conn->read_pos);
        update_congestion_window(conn, 1); // Successful processing
        update_session_activity(conn);
        
        // Check if we have data to write
        if (conn->bytes_to_write > 0) {
            printf("[READ] fd=%d has %zu bytes to write, enabling EPOLLOUT\n", 
                   conn->fd, conn->bytes_to_write);
            
            // Enable write events
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.ptr = conn;
            if (epoll_ctl(g_server.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) == -1) {
                perror("epoll_ctl MOD EPOLLOUT");
                return -1;
            }
        }
        
        // Reset read buffer
        conn->read_pos = 0;
        conn->total_messages++;
    }
    
    return 0;
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
    } else {
        // More data to write, ensure EPOLLOUT is set
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = conn;
        epoll_ctl(g_server.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
    
    return 0;
}
