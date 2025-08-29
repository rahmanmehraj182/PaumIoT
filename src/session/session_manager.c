#include "../../include/session/session_manager.h"
#include "../../include/protocol/protocol_detector.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

extern server_state_t g_server;

int create_session(int socket_fd, struct sockaddr_in* client_addr, protocol_type_t protocol) {
    pthread_mutex_lock(&g_server.table_mutex);
    
    if (g_server.session_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&g_server.table_mutex);
        printf("[SESSION] Maximum clients reached (%d)\n", MAX_CLIENTS);
        return -1;
    }
    
    // Find free slot
    int session_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd == 0) {
            session_index = i;
            break;
        }
    }
    
    if (session_index == -1) {
        pthread_mutex_unlock(&g_server.table_mutex);
        printf("[SESSION] No free session slots available\n");
        return -1;
    }
    
    connection_t* session = &g_server.connections[session_index];
    
    // Initialize session
    snprintf(session->session_id, MAX_SESSION_ID, "%s_%d_%ld", 
             protocol_to_string(protocol), socket_fd, time(NULL));
    session->fd = socket_fd;
    session->addr = *client_addr;
    session->protocol = protocol;
    session->session_state = (protocol == PROTOCOL_UNKNOWN) ? SESSION_CONNECTED : SESSION_ACTIVE;
    session->state = CONN_CONNECTED;
    session->last_activity = time(NULL);
    session->created_at = time(NULL);
    session->message_count = 0;
    session->error_count = 0;
    session->total_messages = 0;
    session->read_pos = 0;
    session->write_pos = 0;
    session->bytes_to_write = 0;
    session->detection_confidence = 0;
    session->detection_attempts = 0;
    session->last_detection = 0;
    
    session->session_flags.flags = SESSION_FLAG_ACTIVE;
    
    // Initialize protocol-specific data
    memset(&session->protocol_data, 0, sizeof(protocol_data_t));
    switch (protocol) {
        case PROTOCOL_MQTT:
            session->protocol_data.mqtt.keep_alive_interval = 60;
            session->protocol_data.mqtt.qos_level = 0;
            session->protocol_data.mqtt.protocol_version = 4;
            break;
        case PROTOCOL_COAP:
            session->protocol_data.coap.message_id_counter = 1;
            session->protocol_data.coap.token_length = 0;
            session->protocol_data.coap.observe_sequence = 0;
            break;
        case PROTOCOL_HTTP:
            strcpy(session->protocol_data.http.version, "HTTP/1.1");
            session->protocol_data.http.connection_close = 0;
            session->protocol_data.http.content_length = 0;
            break;
        case PROTOCOL_DNS:
            session->protocol_data.dns.transaction_id = 0;
            session->protocol_data.dns.query_type = 1; // A record
            session->protocol_data.dns.flags = 0;
            session->protocol_data.dns.questions = 0;
            session->protocol_data.dns.answers = 0;
            break;
        default:
            break;
    }
    
    pthread_mutex_init(&session->session_mutex, NULL);
    g_server.session_count++;
    
    pthread_mutex_unlock(&g_server.table_mutex);
    
    printf("[SESSION] Created session %s for %s:%d (Protocol: %s)\n",
           session->session_id,
           inet_ntoa(client_addr->sin_addr),
           ntohs(client_addr->sin_port),
           protocol_to_string(protocol));
    
    return session_index;
}

connection_t* get_session_by_socket(int socket_fd) {
    pthread_mutex_lock(&g_server.table_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd == socket_fd && 
            (g_server.connections[i].session_flags.flags & SESSION_FLAG_ACTIVE)) {
            pthread_mutex_unlock(&g_server.table_mutex);
            return &g_server.connections[i];
        }
    }
    
    pthread_mutex_unlock(&g_server.table_mutex);
    return NULL;
}

void remove_session(int socket_fd) {
    pthread_mutex_lock(&g_server.table_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd == socket_fd) {
            connection_t* session = &g_server.connections[i];
            
            printf("[SESSION] Removing session %s for fd=%d (msgs=%u, uptime=%lds)\n",
                   session->session_id, socket_fd, session->total_messages,
                   time(NULL) - session->created_at);
            
            // Cleanup session
            session->session_state = SESSION_CLOSED;
            session->session_flags.flags &= ~SESSION_FLAG_ACTIVE;
            pthread_mutex_destroy(&session->session_mutex);
            
            // Clear session data
            memset(session, 0, sizeof(connection_t));
            
            g_server.session_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_server.table_mutex);
}

void update_session_activity(connection_t* session) {
    if (!session) return;
    
    pthread_mutex_lock(&session->session_mutex);
    session->last_activity = time(NULL);
    session->message_count++;
    session->total_messages++;
    
    // Update session state based on activity
    if (session->protocol != PROTOCOL_UNKNOWN && 
        session->session_state == SESSION_CONNECTED) {
        session->session_state = SESSION_ACTIVE;
    }
    
    pthread_mutex_unlock(&session->session_mutex);
}

void update_session_protocol(int socket_fd, protocol_type_t protocol) {
    pthread_mutex_lock(&g_server.table_mutex);
    
    // Find session by socket fd
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.connections[i].fd == socket_fd) {
            connection_t* session = &g_server.connections[i];
            
            printf("[DEBUG] Found session at index %d, current protocol: %s, new protocol: %s\n", 
                   i, protocol_to_string(session->protocol), protocol_to_string(protocol));
            printf("[DEBUG] Current session ID: %s\n", session->session_id);
            
            // Update protocol
            protocol_type_t old_protocol = session->protocol;
            session->protocol = protocol;
            
            // Update session ID to reflect new protocol
            snprintf(session->session_id, MAX_SESSION_ID, "%s_%d_%ld", 
                     protocol_to_string(protocol), socket_fd, session->created_at);
            
            printf("[DEBUG] New session ID: %s\n", session->session_id);
            
            // Update session state to reflect protocol detection
            if (protocol != PROTOCOL_UNKNOWN && old_protocol == PROTOCOL_UNKNOWN) {
                session->session_state = SESSION_ACTIVE;
                printf("[SESSION] Updated session %s protocol to %s (state: Active)\n",
                       session->session_id, protocol_to_string(protocol));
            } else {
                printf("[SESSION] Updated session %s protocol to %s\n",
                       session->session_id, protocol_to_string(protocol));
            }
            
            pthread_mutex_unlock(&g_server.table_mutex);
            return;
        }
    }
    
    pthread_mutex_unlock(&g_server.table_mutex);
    printf("[SESSION] Warning: Could not find session for fd=%d to update protocol\n", socket_fd);
}

void print_session_table(void) {
    pthread_mutex_lock(&g_server.table_mutex);
    
    printf("\n=== SESSION STATE TABLE ===\n");
    printf("Active sessions: %d/%d\n", g_server.session_count, MAX_CLIENTS);
    printf("%-20s %-8s %-10s %-15s %-15s %-8s %-8s %-8s %-8s\n", 
           "Session ID", "Socket", "Protocol", "State", "Client IP", "Messages", "Uptime", "Conf", "Detect");
    printf("----------------------------------------------------------------------------------------------------\n");
    
    time_t now = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connection_t* session = &g_server.connections[i];
        if (session->fd > 0 && (session->session_flags.flags & SESSION_FLAG_ACTIVE)) {
            printf("%-20s %-8d %-10s %-15s %-15s %-8u %-8lds %-8u%% %-8u\n",
                   session->session_id,
                   session->fd,
                   protocol_to_string(session->protocol),
                   state_to_string(session->session_state),
                   inet_ntoa(session->addr.sin_addr),
                   session->total_messages,
                   now - session->created_at,
                   session->detection_confidence,
                   session->detection_attempts);
        }
    }
    printf("====================================================================================================\n\n");
    
    pthread_mutex_unlock(&g_server.table_mutex);
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

void cleanup_stale_connections(void) {
    time_t now = time(NULL);
    int cleaned = 0;
    
    pthread_mutex_lock(&g_server.table_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connection_t* conn = &g_server.connections[i];
        
        if (conn->fd > 0 && (conn->session_flags.flags & SESSION_FLAG_ACTIVE)) {
            // Check for stale connections (60 seconds)
            if (now - conn->last_activity > 60) {
                printf("[CLEANUP] Closing stale connection fd=%d (idle: %lds)\n",
                       conn->fd, now - conn->last_activity);
                
                // Mark for cleanup - actual cleanup will be done by the caller
                conn->session_state = SESSION_CLOSED;
                conn->state = CONN_CLOSING;
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
    
    pthread_mutex_unlock(&g_server.table_mutex);
    
    if (cleaned > 0) {
        printf("[CLEANUP] Marked %d stale connections for cleanup\n", cleaned);
    }
}
