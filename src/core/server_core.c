#include "../../include/core/server_core.h"
#include "../../include/network/network_manager.h"
#include "../../include/session/session_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Global server state
server_state_t g_server = {0};
volatile int g_running = 1;

int server_init(void) {
    printf("Event-Driven Protocol Middleware with Congestion Control\n");
    printf("========================================================\n");
    
    // Initialize server state
    memset(&g_server, 0, sizeof(server_state_t));
    pthread_mutex_init(&g_server.table_mutex, NULL);
    
    // Create epoll
    g_server.epoll_fd = epoll_create1(0);
    if (g_server.epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }
    
    // Create and setup sockets
    g_server.tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    g_server.udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (g_server.tcp_socket == -1 || g_server.udp_socket == -1) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(g_server.tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(g_server.udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (make_socket_nonblocking(g_server.tcp_socket) == -1 ||
        make_socket_nonblocking(g_server.udp_socket) == -1) {
        return -1;
    }
    
    // Bind sockets
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(g_server.tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1 ||
        bind(g_server.udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return -1;
    }
    
    if (listen(g_server.tcp_socket, 128) == -1) {
        perror("listen");
        return -1;
    }
    
    // Add sockets to epoll
    if (add_to_epoll(g_server.epoll_fd, g_server.tcp_socket, EPOLLIN, &g_server.tcp_socket) == -1 ||
        add_to_epoll(g_server.epoll_fd, g_server.udp_socket, EPOLLIN, &g_server.udp_socket) == -1) {
        return -1;
    }
    
    printf("Server listening on port %d\n", SERVER_PORT);
    printf("Event-driven architecture with congestion control active\n");
    printf("Supported protocols: HTTP, MQTT, CoAP, DNS\n\n");
    
    return 0;
}

void server_run(void) {
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
            
            // Clean up connections marked for closure
            for (int i = 0; i < MAX_CLIENTS; i++) {
                connection_t* conn = &g_server.connections[i];
                if (conn->fd > 0 && conn->state == CONN_CLOSING) {
                    free_connection(conn);
                }
            }
            
            last_cleanup = now;
        }
        
        // Print stats (every 60 seconds)
        if (now - last_stats > 60) {
            print_server_stats();
            print_session_table();
            last_stats = now;
        }
    }
}

void server_cleanup(void) {
    // Close all connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd > 0) {
            free_connection(&g_server.connections[i]);
        }
    }
    
    // Close server sockets
    if (g_server.tcp_socket > 0) close(g_server.tcp_socket);
    if (g_server.udp_socket > 0) close(g_server.udp_socket);
    if (g_server.epoll_fd > 0) close(g_server.epoll_fd);
    
    // Cleanup mutex
    pthread_mutex_destroy(&g_server.table_mutex);
    
    printf("Server shutdown complete\n");
}

// Print server statistics
void print_server_stats(void) {
    printf("\n=== SERVER STATISTICS ===\n");
    printf("Active connections: %u/%d\n", g_server.active_connections, MAX_CLIENTS);
    printf("Total bytes processed: %llu MB\n", g_server.total_bytes_processed / (1024*1024));
    printf("System load: %u%%\n", g_server.system_load);
    
    // Per-connection stats
    int throttled = 0, slow_start = 0;
    int protocol_count[5] = {0}; // Count per protocol
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd > 0) {
            if (g_server.connections[i].state == CONN_THROTTLED) throttled++;
            if (g_server.connections[i].congestion.in_slow_start) slow_start++;
            
            protocol_type_t prot = g_server.connections[i].protocol;
            if (prot >= 0 && prot < 5) protocol_count[prot]++;
        }
    }
    
    printf("Throttled connections: %d\n", throttled);
    printf("Slow start connections: %d\n", slow_start);
    printf("Protocol distribution:\n");
    printf("  Unknown: %d, MQTT: %d, CoAP: %d, HTTP: %d, DNS: %d\n",
           protocol_count[0], protocol_count[1], protocol_count[2], 
           protocol_count[3], protocol_count[4]);
    printf("=========================\n\n");
}
