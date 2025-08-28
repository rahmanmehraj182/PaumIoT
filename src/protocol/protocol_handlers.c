#include "../../include/protocol/protocol_handlers.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

void handle_mqtt_message(connection_t* conn, const uint8_t* data, size_t len) {
    uint8_t msg_type = (data[0] >> 4) & 0x0F;
    
    printf("[MQTT] Processing message type %d from %s\n", 
           msg_type, conn->session_id);
    
    switch (msg_type) {
        case 1: { // CONNECT
            printf("[MQTT] CONNECT received\n");
            conn->session_state = SESSION_AUTHENTICATED;
            
            // Set authentication flag atomically  
            conn->session_flags.flags |= SESSION_FLAG_AUTHENTICATED;
            
            // Extract and store client ID, keep-alive, etc.
            if (len > 12) {
                // Parse protocol name length
                if (len >= 14) {
                    uint16_t proto_name_len = (data[2] << 8) | data[3];
                    if (proto_name_len == 4 && len >= 14 + proto_name_len) {
                        // Check for "MQTT"
                        if (memcmp(&data[4], "MQTT", 4) == 0) {
                            conn->protocol_data.mqtt.protocol_version = data[8];
                            conn->protocol_data.mqtt.keep_alive_interval = (data[10] << 8) | data[11];
                            printf("[MQTT] Protocol version: %d, Keep-alive: %d\n",
                                   conn->protocol_data.mqtt.protocol_version,
                                   conn->protocol_data.mqtt.keep_alive_interval);
                        }
                    }
                }
            }
            
            // Send CONNACK
            uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
            if (conn->bytes_to_write + sizeof(connack) <= sizeof(conn->write_buffer)) {
                memcpy(conn->write_buffer + conn->bytes_to_write, connack, sizeof(connack));
                conn->bytes_to_write += sizeof(connack);
                printf("[MQTT] CONNACK queued (%zu bytes)\n", sizeof(connack));
            }
            break;
        }
        case 3: { // PUBLISH
            printf("[MQTT] PUBLISH received\n");
            // Parse topic and payload (simplified)
            if (len > 4) {
                uint16_t topic_len = (data[2] << 8) | data[3];
                if (len >= 4 + topic_len) {
                    printf("[MQTT] Topic length: %d\n", topic_len);
                    // Extract topic name and payload...
                }
            }
            
            // Send PUBACK for QoS 1
            uint8_t flags = data[0] & 0x0F;
            uint8_t qos = (flags >> 1) & 0x03;
            if (qos == 1) {
                uint8_t puback[] = {0x40, 0x02, data[2], data[3]}; // Copy packet ID
                if (conn->bytes_to_write + sizeof(puback) <= sizeof(conn->write_buffer)) {
                    memcpy(conn->write_buffer + conn->bytes_to_write, puback, sizeof(puback));
                    conn->bytes_to_write += sizeof(puback);
                    printf("[MQTT] PUBACK queued (%zu bytes)\n", sizeof(puback));
                }
            }
            break;
        }
        case 8: { // SUBSCRIBE
            printf("[MQTT] SUBSCRIBE received\n");
            // Send SUBACK
            uint8_t suback[] = {0x90, 0x03, data[2], data[3], 0x00}; // QoS 0
            if (conn->bytes_to_write + sizeof(suback) <= sizeof(conn->write_buffer)) {
                memcpy(conn->write_buffer + conn->bytes_to_write, suback, sizeof(suback));
                conn->bytes_to_write += sizeof(suback);
                printf("[MQTT] SUBACK queued (%zu bytes)\n", sizeof(suback));
            }
            break;
        }
        case 12: { // PINGREQ
            printf("[MQTT] PINGREQ received\n");
            // Send PINGRESP
            uint8_t pingresp[] = {0xD0, 0x00};
            if (conn->bytes_to_write + sizeof(pingresp) <= sizeof(conn->write_buffer)) {
                memcpy(conn->write_buffer + conn->bytes_to_write, pingresp, sizeof(pingresp));
                conn->bytes_to_write += sizeof(pingresp);
                printf("[MQTT] PINGRESP queued (%zu bytes)\n", sizeof(pingresp));
            }
            break;
        }
        case 14: { // DISCONNECT
            printf("[MQTT] DISCONNECT received\n");
            conn->session_state = SESSION_DISCONNECTING;
            break;
        }
        default:
            printf("[MQTT] Unhandled message type: %d\n", msg_type);
    }
}

void handle_coap_message(connection_t* conn, const uint8_t* data, size_t len) {
    uint8_t msg_type = (data[0] >> 4) & 0x03;
    uint8_t code = data[1];
    uint16_t message_id = (data[2] << 8) | data[3];
    uint8_t token_length = data[0] & 0x0F;
    
    printf("[COAP] Processing - Type: %d, Code: %d, ID: %d from %s\n",
           msg_type, code, message_id, conn->session_id);
    
    // Store CoAP message info
    conn->protocol_data.coap.message_id_counter = message_id;
    if (token_length <= 8) {
        conn->protocol_data.coap.token_length = token_length;
        if (len >= 4 + token_length) {
            memcpy(conn->protocol_data.coap.token, &data[4], token_length);
        }
    }
    
    // If it's a request, send ACK response
    if (code >= 1 && code <= 31) {
        // Generate CoAP response
        uint8_t response[64];
        response[0] = 0x60; // Version 1, Type ACK (2), Token Length 0
        response[1] = 0x45; // Code 2.05 Content
        response[2] = data[2]; // Copy message ID
        response[3] = data[3];
        
        // Add payload marker and simple JSON response
        const char* payload = "\xFF{\"status\":\"ok\",\"protocol\":\"CoAP\"}";
        size_t payload_len = strlen(payload);
        
        if (4 + payload_len <= sizeof(response) && 
            conn->bytes_to_write + 4 + payload_len <= sizeof(conn->write_buffer)) {
            memcpy(response + 4, payload, payload_len);
            memcpy(conn->write_buffer + conn->bytes_to_write, response, 4 + payload_len);
            conn->bytes_to_write += 4 + payload_len;
            printf("[COAP] Response queued (%zu bytes)\n", 4 + payload_len);
        }
    }
}

void handle_http_message(connection_t* conn, const uint8_t* data, size_t len) {
    char* request = (char*)data;
    printf("[HTTP] Processing request from %s\n", conn->session_id);
    
    // Parse HTTP request line
    char method[16], uri[128], version[16];
    if (sscanf(request, "%15s %127s %15s", method, uri, version) == 3) {
        strncpy(conn->protocol_data.http.method, method, sizeof(conn->protocol_data.http.method) - 1);
        strncpy(conn->protocol_data.http.uri, uri, sizeof(conn->protocol_data.http.uri) - 1);
        strncpy(conn->protocol_data.http.version, version, sizeof(conn->protocol_data.http.version) - 1);
        
        printf("[HTTP] Method: %s, URI: %s, Version: %s\n", method, uri, version);
    }
    
    // Parse headers
    char* header_start = strstr(request, "\r\n");
    if (header_start) {
        header_start += 2; // Skip first CRLF
        char* host_header = strstr(header_start, "Host:");
        if (host_header) {
            char* end_line = strstr(host_header, "\r\n");
            if (end_line) {
                size_t host_len = end_line - host_header - 5; // Skip "Host:"
                if (host_len < sizeof(conn->protocol_data.http.host)) {
                    memcpy(conn->protocol_data.http.host, host_header + 5, host_len);
                    conn->protocol_data.http.host[host_len] = '\0';
                    // Trim whitespace
                    while (conn->protocol_data.http.host[0] == ' ') {
                        memmove(conn->protocol_data.http.host, conn->protocol_data.http.host + 1, 
                               strlen(conn->protocol_data.http.host));
                    }
                }
            }
        }
    }
    
    // Generate HTTP response based on method
    char response[2048];
    const char* status_line = "HTTP/1.1 200 OK\r\n";
    const char* headers = 
        "Content-Type: application/json\r\n"
        "Server: PaumIoT-Enhanced/1.0\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n";
    
    const char* body_template = "{\n"
        "  \"status\": \"success\",\n"
        "  \"message\": \"Enhanced protocol detection working\",\n"
        "  \"protocol\": \"HTTP\",\n"
        "  \"method\": \"%s\",\n"
        "  \"uri\": \"%s\",\n"
        "  \"detection_confidence\": %d,\n"
        "  \"timestamp\": %ld,\n"
        "  \"server\": \"PaumIoT-Enhanced\"\n"
        "}";
    
    char json_body[1024];
    snprintf(json_body, sizeof(json_body), body_template, 
             conn->protocol_data.http.method,
             conn->protocol_data.http.uri,
             conn->detection_confidence,
             time(NULL));
    
    snprintf(response, sizeof(response),
        "%s%sContent-Length: %zu\r\n\r\n%s",
        status_line, headers, strlen(json_body), json_body);
    
    size_t response_len = strlen(response);
    if (conn->bytes_to_write + response_len <= sizeof(conn->write_buffer)) {
        memcpy(conn->write_buffer + conn->bytes_to_write, response, response_len);
        conn->bytes_to_write += response_len;
        printf("[HTTP] Response queued (%zu bytes) for %s %s\n", response_len, method, uri);
    } else {
        printf("[HTTP] ERROR: Response too large for buffer\n");
    }
    
    // Close connection after response
    conn->session_state = SESSION_DISCONNECTING;
}

void handle_dns_message(connection_t* conn, const uint8_t* data, size_t len, struct sockaddr_in* client_addr) {
    printf("[DNS] Processing query from %s\n", conn->session_id);
    
    if (len < 12) {
        printf("[DNS] Packet too short for DNS header\n");
        return;
    }
    
    // Parse DNS header
    uint16_t transaction_id = (data[0] << 8) | data[1];
    uint16_t flags = (data[2] << 8) | data[3];
    uint16_t questions = (data[4] << 8) | data[5];
    
    conn->protocol_data.dns.transaction_id = transaction_id;
    conn->protocol_data.dns.flags = flags;
    conn->protocol_data.dns.questions = questions;
    
    printf("[DNS] Transaction ID: 0x%04X, Questions: %d\n", transaction_id, questions);
    
    // Parse first question (simplified)
    if (questions > 0 && len > 12) {
        size_t pos = 12;
        size_t name_start = pos;
        
        // Skip QNAME
        while (pos < len) {
            uint8_t label_len = data[pos];
            if (label_len == 0) {
                pos++; // End of name
                break;
            } else if ((label_len & 0xC0) == 0xC0) {
                pos += 2; // Compression pointer
                break;
            } else {
                pos += label_len + 1;
            }
        }
        
        if (pos + 4 <= len) {
            uint16_t qtype = (data[pos] << 8) | data[pos + 1];
            conn->protocol_data.dns.query_type = qtype;
            
            // Extract query name (simplified)
            size_t name_len = pos - name_start;
            if (name_len < sizeof(conn->protocol_data.dns.query_name)) {
                memcpy(conn->protocol_data.dns.query_name, &data[name_start], name_len);
                conn->protocol_data.dns.query_name[name_len] = '\0';
            }
        }
    }
    
    // Generate DNS response
    uint8_t response[512];
    int resp_pos = 0;
    
    // Copy header with response flags
    memcpy(response, data, 12);
    response[2] |= 0x80; // Set QR bit (response)
    response[3] &= 0xF0; // Clear RCODE
    response[6] = 0x00;  // Answer count high byte
    response[7] = 0x01;  // Answer count low byte (1 answer)
    resp_pos = 12;
    
    // Copy question section
    if (questions > 0 && len > 12) {
        size_t question_len = 0;
        size_t pos = 12;
        
        // Calculate question section length
        while (pos < len) {
            uint8_t label_len = data[pos];
            if (label_len == 0) {
                pos++; // End of name
                break;
            } else {
                pos += label_len + 1;
            }
        }
        pos += 4; // QTYPE and QCLASS
        
        question_len = pos - 12;
        if (resp_pos + question_len < sizeof(response)) {
            memcpy(response + resp_pos, data + 12, question_len);
            resp_pos += question_len;
        }
        
        // Add answer record (simplified A record pointing to localhost)
        if (resp_pos + 16 < sizeof(response)) {
            // Name (compression pointer to question)
            response[resp_pos++] = 0xC0;
            response[resp_pos++] = 0x0C;
            
            // Type A (0x0001)
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x01;
            
            // Class IN (0x0001)
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x01;
            
            // TTL (300 seconds)
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x01;
            response[resp_pos++] = 0x2C;
            
            // RDLENGTH (4 bytes)
            response[resp_pos++] = 0x00;
            response[resp_pos++] = 0x04;
            
            // RDATA (127.0.0.1)
            response[resp_pos++] = 127;
            response[resp_pos++] = 0;
            response[resp_pos++] = 0;
            response[resp_pos++] = 1;
        }
    }
    
    if (resp_pos > 0 && conn->bytes_to_write + resp_pos <= sizeof(conn->write_buffer)) {
        memcpy(conn->write_buffer + conn->bytes_to_write, response, resp_pos);
        conn->bytes_to_write += resp_pos;
        printf("[DNS] Response queued (%d bytes)\n", resp_pos);
    }
}

void handle_tls_message(connection_t* conn, const uint8_t* data, size_t len) {
    printf("[TLS] Processing TLS record from %s\n", conn->session_id);
    
    if (len < 5) {
        printf("[TLS] Packet too short for TLS header\n");
        return;
    }
    
    // Parse TLS record header
    uint8_t content_type = data[0];
    uint16_t version = (data[1] << 8) | data[2];
    uint16_t record_length = (data[3] << 8) | data[4];
    
    conn->protocol_data.tls.content_type = content_type;
    conn->protocol_data.tls.version = version;
    conn->protocol_data.tls.record_length = record_length;
    
    printf("[TLS] Content Type: %d, Version: 0x%04X, Length: %d\n", 
           content_type, version, record_length);
    
    // Handle different content types
    switch (content_type) {
        case TLS_HANDSHAKE:
            if (len >= 6) {
                uint8_t handshake_type = data[5];
                conn->protocol_data.tls.handshake_type = handshake_type;
                printf("[TLS] Handshake Type: %d\n", handshake_type);
                
                if (handshake_type == TLS_CLIENT_HELLO) {
                    printf("[TLS] Client Hello detected - starting handshake\n");
                    conn->session_state = SESSION_AUTHENTICATED;
                    conn->session_flags.flags |= SESSION_FLAG_TLS_ESTABLISHED;
                }
            }
            break;
            
        case TLS_APPLICATION_DATA:
            printf("[TLS] Application data - encrypted payload\n");
            break;
            
        case TLS_ALERT:
            printf("[TLS] Alert message received\n");
            break;
            
        case TLS_CHANGE_CIPHER_SPEC:
            printf("[TLS] Change Cipher Spec - encryption established\n");
            conn->session_flags.flags |= SESSION_FLAG_TLS_ESTABLISHED;
            break;
    }
    
    // For TLS, we typically don't send responses in the same way
    // The TLS handshake is handled by the underlying transport
    printf("[TLS] TLS record processed successfully\n");
}

void handle_quic_message(connection_t* conn, const uint8_t* data, size_t len) {
    printf("[QUIC] Processing QUIC packet from %s\n", conn->session_id);
    
    if (len < 6) {
        printf("[QUIC] Packet too short for QUIC header\n");
        return;
    }
    
    // Parse QUIC header
    uint8_t first_byte = data[0];
    uint8_t packet_type = (first_byte >> 4) & 0x0F;
    uint8_t long_header = (first_byte >> 7) & 0x01;
    
    conn->protocol_data.quic.packet_type = packet_type;
    
    printf("[QUIC] Packet Type: %d, Long Header: %s\n", 
           packet_type, long_header ? "Yes" : "No");
    
    if (long_header) {
        // Long header packet
        if (len >= 5) {
            uint32_t version = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
            conn->protocol_data.quic.version = version;
            printf("[QUIC] Version: 0x%08X\n", version);
            
            // Parse connection ID length
            if (len >= 6) {
                uint8_t dcil = (data[5] >> 4) & 0x0F;
                uint8_t scil = data[5] & 0x0F;
                printf("[QUIC] DCIL: %d, SCIL: %d\n", dcil, scil);
                
                // Extract connection ID (simplified)
                if (dcil > 0 && len >= 6 + dcil) {
                    uint64_t conn_id = 0;
                    for (int i = 0; i < dcil && i < 8; i++) {
                        conn_id = (conn_id << 8) | data[6 + i];
                    }
                    conn->protocol_data.quic.connection_id = conn_id;
                    conn->protocol_data.quic.connection_id_length = dcil;
                    printf("[QUIC] Connection ID: 0x%016lX\n", conn_id);
                }
            }
        }
    } else {
        // Short header packet
        printf("[QUIC] Short header packet - connection established\n");
        conn->session_state = SESSION_AUTHENTICATED;
    }
    
    // QUIC is typically handled at the transport level
    // We acknowledge receipt but don't generate application-level responses
    printf("[QUIC] QUIC packet processed successfully\n");
}

// Generic protocol message processor
void process_protocol_message(connection_t* conn, const uint8_t* data, size_t len) {
    switch (conn->protocol) {
        case PROTOCOL_HTTP:
            handle_http_message(conn, data, len);
            break;
        case PROTOCOL_MQTT:
            handle_mqtt_message(conn, data, len);
            break;
        case PROTOCOL_COAP:
            handle_coap_message(conn, data, len);
            break;
        case PROTOCOL_DNS:
            handle_dns_message(conn, data, len, &conn->addr);
            break;
        case PROTOCOL_TLS:
            handle_tls_message(conn, data, len);
            break;
        case PROTOCOL_QUIC:
            handle_quic_message(conn, data, len);
            break;
        default:
            printf("[PROTOCOL] Unsupported protocol: %d\n", conn->protocol);
            break;
    }
    
    // Update activity
    conn->last_activity = time(NULL);
    conn->message_count++;
}
