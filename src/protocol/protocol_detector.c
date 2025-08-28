#include "../../include/protocol/protocol_detector.h"
#include "../../include/core/server_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pcap.h>

extern server_state_t g_server;

// Global packet capture handle
static pcap_t *capture_handle = NULL;
static int capture_initialized = 0;

// Enhanced Protocol Detection Engine
protocol_type_t detect_protocol(const uint8_t* data, size_t len) {
    if (len < 2) {
        printf("[DETECTION] Packet too short for detection (%zu bytes)\n", len);
        return PROTOCOL_UNKNOWN;
    }

    printf("[DETECTION] Analyzing %zu bytes: ", len);
    for (size_t i = 0; i < (len > 16 ? 16 : len); i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    int scores[7] = {0}; // unknown, mqtt, coap, http, dns, tls, quic scores
    
    // Try TLS detection first (can encapsulate other protocols)
    if (detect_tls_enhanced(data, len)) {
        scores[5] = 20; // High priority for TLS
        printf("[DETECTION] TLS validation: PASS (score: %d)\n", scores[5]);
    }
    
    // Try QUIC detection
    if (detect_quic_enhanced(data, len)) {
        scores[6] = 18;
        printf("[DETECTION] QUIC validation: PASS (score: %d)\n", scores[6]);
    }
    
    // Try HTTP detection
    if (detect_http_enhanced(data, len)) {
        scores[3] = 15;
        printf("[DETECTION] HTTP validation: PASS (score: %d)\n", scores[3]);
    }
    
    // Try DNS detection
    if (detect_dns_enhanced(data, len)) {
        scores[4] = 12;
        printf("[DETECTION] DNS validation: PASS (score: %d)\n", scores[4]);
    }
    
    // Try MQTT detection
    if (detect_mqtt_enhanced(data, len)) {
        scores[1] = 10;
        printf("[DETECTION] MQTT validation: PASS (score: %d)\n", scores[1]);
    }
    
    // Try CoAP detection
    if (detect_coap_enhanced(data, len)) {
        scores[2] = 8;
        printf("[DETECTION] CoAP validation: PASS (score: %d)\n", scores[2]);
    }
    
    // Determine best match
    int max_score = 0;
    protocol_type_t detected = PROTOCOL_UNKNOWN;
    
    for (int i = 0; i < 7; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            detected = (protocol_type_t)i;
        }
    }
    
    if (detected != PROTOCOL_UNKNOWN) {
        printf("[DETECTION] ✓ %s detected (confidence: %d)\n", 
               protocol_to_string(detected), max_score);
        return detected;
    }
    
    printf("[DETECTION] ✗ MALFORMED PDU - Unknown/invalid protocol\n");
    return PROTOCOL_UNKNOWN;
}

// Fast protocol detection (optimized for event loop)
protocol_type_t detect_protocol_fast(const uint8_t* data, size_t len) {
    if (len < 2) return PROTOCOL_UNKNOWN;
    
    // TLS - check content type
    if (len >= 1) {
        uint8_t content_type = data[0];
        if (content_type >= 20 && content_type <= 23) {
            return PROTOCOL_TLS;
        }
    }
    
    // QUIC - check long header bit
    if (len >= 1 && (data[0] & 0x80) == 0x80) {
        return PROTOCOL_QUIC;
    }
    
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

// Enhanced protocol detection with dynamic confidence scoring
protocol_type_t detect_protocol_enhanced(const uint8_t* data, size_t len, 
                                        uint8_t *confidence, int is_tcp) {
    if (len < 2) {
        *confidence = 0;
        return PROTOCOL_UNKNOWN;
    }
    
    *confidence = 0;
    protocol_type_t detected = PROTOCOL_UNKNOWN;
    confidence_features_t features;
    
    // Check TLS first (highest priority)
    if (detect_tls_enhanced(data, len)) {
        detected = PROTOCOL_TLS;
        *confidence = calculate_dynamic_confidence(PROTOCOL_TLS, data, len, is_tcp, &features);
        return detected;
    }
    
    // Check QUIC
    if (detect_quic_enhanced(data, len)) {
        detected = PROTOCOL_QUIC;
        *confidence = calculate_dynamic_confidence(PROTOCOL_QUIC, data, len, is_tcp, &features);
        return detected;
    }
    
    // Check DNS
    if (detect_dns_enhanced(data, len)) {
        detected = PROTOCOL_DNS;
        *confidence = calculate_dynamic_confidence(PROTOCOL_DNS, data, len, is_tcp, &features);
        return detected;
    }
    
    // Check HTTP
    if (detect_http_enhanced(data, len)) {
        detected = PROTOCOL_HTTP;
        *confidence = calculate_dynamic_confidence(PROTOCOL_HTTP, data, len, is_tcp, &features);
        return detected;
    }
    
    // Check MQTT (TCP only)
    if (is_tcp && detect_mqtt_enhanced(data, len)) {
        detected = PROTOCOL_MQTT;
        *confidence = calculate_dynamic_confidence(PROTOCOL_MQTT, data, len, is_tcp, &features);
        return detected;
    }
    
    // Check CoAP (UDP only)
    if (!is_tcp && detect_coap_enhanced(data, len)) {
        detected = PROTOCOL_COAP;
        *confidence = calculate_dynamic_confidence(PROTOCOL_COAP, data, len, is_tcp, &features);
        return detected;
    }
    
    return PROTOCOL_UNKNOWN;
}

// Enhanced HTTP Detection
int detect_http_enhanced(const uint8_t* data, size_t len) {
    if (len < 4) return 0;
    
    // Check for HTTP request methods (extended list)
    const char* methods[] = {
        "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "OPTIONS ", "PATCH ", 
        "CONNECT ", "TRACE ", "PRI ", "COPY ", "LOCK ", "MKCOL ", "MOVE ", 
        "PROPFIND ", "PROPPATCH ", "SEARCH ", "UNLOCK ", "BIND ", "REBIND ",
        "UNBIND ", "ACL ", "REPORT ", "MKACTIVITY ", "CHECKOUT ", "MERGE ",
        "M-SEARCH ", "NOTIFY ", "SUBSCRIBE ", "UNSUBSCRIBE ", NULL
    };
    
    for (int i = 0; methods[i] != NULL; i++) {
        size_t method_len = strlen(methods[i]);
        if (len >= method_len && strncmp((char*)data, methods[i], method_len) == 0) {
            return 1;
        }
    }
    
    // Check for HTTP response
    if (len >= 5 && strncmp((char*)data, "HTTP/", 5) == 0) {
        return 1;
    }
    
    // Check for HTTP headers in middle of stream
    if (len > 20) {
        const char* headers[] = {
            "Host:", "User-Agent:", "Content-Type:", "Content-Length:", 
            "Connection:", "Accept:", "Referer:", "Cookie:", "Authorization:",
            "Location:", "Server:", "Date:", "Set-Cookie:", NULL
        };
        
        for (int i = 0; headers[i] != NULL; i++) {
            if (strstr((char*)data, headers[i]) != NULL) {
                return 1;
            }
        }
    }
    
    return 0;
}

// Enhanced DNS Detection
int detect_dns_enhanced(const uint8_t* data, size_t len) {
    if (len < 12) return 0;
    
    // Check for DNS-over-TCP length prefix (2 bytes)
    uint32_t offset = 0;
    if (len >= 2 && ntohs(*(uint16_t*)data) == len - 2) {
        offset = 2;
        if (len - offset < 12) return 0;
    }
    
    // Extract flags from bytes 2 and 3
    uint16_t flags = ntohs(*(uint16_t*)(data + offset + 2));
    
    uint8_t qr = (flags >> 15) & 0x1;
    uint8_t opcode = (flags >> 11) & 0xF;
    uint8_t rcode = flags & 0xF;
    
    // Check for valid combinations
    if (opcode > 5) return 0;
    if (rcode > 5) return 0;
    if (qr == 0 && rcode != 0) return 0;
    
    // Check counts
    uint16_t qdcount = ntohs(*(uint16_t*)(data + offset + 4));
    uint16_t ancount = ntohs(*(uint16_t*)(data + offset + 6));
    uint16_t nscount = ntohs(*(uint16_t*)(data + offset + 8));
    uint16_t arcount = ntohs(*(uint16_t*)(data + offset + 10));
    
    if (qr == 0 && qdcount == 0) return 0;
    if (qdcount > 100 || ancount > 1000 || nscount > 1000 || arcount > 1000) return 0;
    
    return 1;
}

// Enhanced MQTT Detection
int detect_mqtt_enhanced(const uint8_t* data, size_t len) {
    if (len < 2) return 0;
    
    uint8_t first_byte = data[0];
    uint8_t packet_type = (first_byte >> 4) & 0x0F;
    uint8_t flags = first_byte & 0x0F;
    
    if (packet_type < 1 || packet_type > 15) return 0;
    
    // Validate flags for specific packet types
    switch (packet_type) {
        case 1:  // CONNECT
            if (flags != 0) return 0;
            break;
        case 2:  // CONNACK
        case 13: // PINGRESP
        case 14: // DISCONNECT
            if (flags != 0) return 0;
            break;
        case 8:  // SUBSCRIBE
        case 10: // UNSUBSCRIBE
            if (flags != 2) return 0;
            break;
        case 9:  // SUBACK
        case 11: // UNSUBACK
            if (flags != 0) return 0;
            break;
        case 3:  // PUBLISH - flags can vary based on QoS, DUP, RETAIN
            if ((flags & 0x06) == 0x06) return 0; // Invalid QoS 3
            break;
        case 4: case 5: case 6: case 7: // PUBACK, PUBREC, PUBREL, PUBCOMP
            if (flags != 0) return 0;
            break;
        case 12: // PINGREQ
            if (flags != 0) return 0;
            break;
    }
    
    // Parse variable length remaining length
    uint32_t index = 1;
    uint32_t multiplier = 1;
    uint32_t length = 0;
    
    do {
        if (index >= len) return 0;
        uint8_t encoded_byte = data[index++];
        length += (encoded_byte & 0x7F) * multiplier;
        multiplier *= 128;
        if (multiplier > 128 * 128 * 128) return 0;
    } while (index < len && (data[index-1] & 0x80));
    
    if (index + length > len) return 0;
    
    // Additional validation for specific packet types
    if (packet_type == 1 && index + 10 <= len) { // CONNECT
        if (strncmp((char*)(data + index), "MQTT", 4) != 0 &&
            strncmp((char*)(data + index), "MQIsdp", 6) != 0) {
            return 0;
        }
    }
    
    return 1;
}

// Enhanced CoAP Detection
int detect_coap_enhanced(const uint8_t* data, size_t len) {
    if (len < 4) return 0;
    
    uint8_t first_byte = data[0];
    uint8_t version = (first_byte >> 6) & 0x03;
    uint8_t token_length = first_byte & 0x0F;
    
    if (version != 1) return 0;
    if (token_length > 8) return 0;
    
    uint8_t code = data[1];
    uint8_t code_class = code >> 5;
    uint8_t code_detail = code & 0x1F;
    
    // Enhanced code validation
    if (code_class == 0) { // Request
        if (code_detail < 1 || code_detail > 31) return 0;
    } else if (code_class == 1) { // Client error response
        if (code_detail > 31) return 0;
    } else if (code_class == 2 || code_class == 3) { // Success/Server error
        if (code_detail > 31) return 0;
    } else if (code_class == 4 || code_class == 5) { // Additional responses
        if (code_detail > 31) return 0;
    } else {
        return 0;
    }
    
    if (len < 4 + token_length) return 0;
    
    return 1;
}

// Enhanced TLS Detection
int detect_tls_enhanced(const uint8_t* data, size_t len) {
    if (len < 5) return 0;
    
    // Check TLS record header
    uint8_t content_type = data[0];
    uint16_t version = ntohs(*(uint16_t*)(data + 1));
    uint16_t length = ntohs(*(uint16_t*)(data + 3));
    
    // Validate content type
    if (content_type != TLS_CHANGE_CIPHER_SPEC &&
        content_type != TLS_ALERT &&
        content_type != TLS_HANDSHAKE &&
        content_type != TLS_APPLICATION_DATA) {
        return 0;
    }
    
    // Validate TLS version (SSL 3.0, TLS 1.0-1.3)
    if (version < 0x0300 || version > 0x0304) {
        return 0;
    }
    
    // Validate length
    if (length + 5 > len) {
        return 0;
    }
    
    // For handshake messages, check handshake type
    if (content_type == TLS_HANDSHAKE && len >= 6) {
        uint8_t handshake_type = data[5];
        if (handshake_type > TLS_FINISHED) {
            return 0;
        }
    }
    
    return 1;
}

// Enhanced QUIC Detection
int detect_quic_enhanced(const uint8_t* data, size_t len) {
    if (len < 6) return 0;
    
    // QUIC often starts with specific patterns
    // Check for QUIC version negotiation or initial packets
    if ((data[0] & 0x80) == 0x80) { // Long header packet
        uint32_t version = ntohl(*(uint32_t*)(data + 1));
        // Check for known QUIC versions or 0x00000000 (version negotiation)
        if (version == 0x00000000 || 
            version == 0x51303030 || // Q000
            version == 0x51303130 || // Q010
            version == 0x51303131 || // Q011
            version == 0x51303139 || // Q019
            version == 0x51303230 || // Q020
            version == 0x51303231 || // Q021
            version == 0x51303235 || // Q025
            version == 0x51303236 || // Q026
            version == 0x51303237 || // Q027
            version == 0x51303238 || // Q028
            version == 0x51303239 || // Q029
            version == 0x51303330 || // Q030
            version == 0x51303331 || // Q031
            version == 0x51303332 || // Q032
            version == 0x51303333 || // Q033
            version == 0x51303430 || // Q040
            version == 0x51303431 || // Q041
            version == 0x51303432 || // Q042
            version == 0x51303433 || // Q043
            version == 0x51303434 || // Q044
            version == 0x51303435 || // Q045
            version == 0x51303436 || // Q046
            version == 0x51303437 || // Q047
            version == 0x51303438 || // Q048
            version == 0x51303439 || // Q049
            version == 0x51303530 || // Q050
            version == 0x51303531) { // Q051
            return 1;
        }
    }
    
    return 0;
}

// Hash function for connection state table
uint32_t connection_hash(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
    return (src_ip ^ dst_ip ^ src_port ^ dst_port) % STATE_TABLE_SIZE;
}

// Update connection state
void update_connection_state(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, 
                           uint16_t dst_port, protocol_type_t protocol, uint32_t seq_number, uint8_t confidence) {
    uint32_t hash = connection_hash(src_ip, dst_ip, src_port, dst_port);
    
    pthread_mutex_lock(&g_server.detection_mutex);
    
    tcp_connection_state_t *conn = g_server.state_table[hash];
    while (conn != NULL) {
        if (conn->src_ip == src_ip && conn->dst_ip == dst_ip && 
            conn->src_port == src_port && conn->dst_port == dst_port) {
            conn->protocol = protocol;
            conn->last_seen = time(NULL);
            conn->seq_number = seq_number;
            conn->confidence = confidence;
            pthread_mutex_unlock(&g_server.detection_mutex);
            return;
        }
        conn = conn->next;
    }
    
    // Create new connection
    tcp_connection_state_t *new_conn = (tcp_connection_state_t *)malloc(sizeof(tcp_connection_state_t));
    if (new_conn) {
        new_conn->src_ip = src_ip;
        new_conn->dst_ip = dst_ip;
        new_conn->src_port = src_port;
        new_conn->dst_port = dst_port;
        new_conn->protocol = protocol;
        new_conn->last_seen = time(NULL);
        new_conn->seq_number = seq_number;
        new_conn->confidence = confidence;
        new_conn->next = g_server.state_table[hash];
        g_server.state_table[hash] = new_conn;
        g_server.enhanced_stats.tcp_connections_tracked++;
    }
    
    pthread_mutex_unlock(&g_server.detection_mutex);
}

// Find connection state
protocol_type_t find_connection_state(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
    uint32_t hash = connection_hash(src_ip, dst_ip, src_port, dst_port);
    
    pthread_mutex_lock(&g_server.detection_mutex);
    tcp_connection_state_t *conn = g_server.state_table[hash];
    while (conn != NULL) {
        if (conn->src_ip == src_ip && conn->dst_ip == dst_ip && 
            conn->src_port == src_port && conn->dst_port == dst_port) {
            protocol_type_t protocol = conn->protocol;
            pthread_mutex_unlock(&g_server.detection_mutex);
            return protocol;
        }
        conn = conn->next;
    }
    pthread_mutex_unlock(&g_server.detection_mutex);
    
    return PROTOCOL_UNKNOWN;
}

// Cleanup old connections
void cleanup_old_connections(void) {
    time_t now = time(NULL);
    pthread_mutex_lock(&g_server.detection_mutex);
    
    for (int i = 0; i < STATE_TABLE_SIZE; i++) {
        tcp_connection_state_t **ptr = &g_server.state_table[i];
        while (*ptr != NULL) {
            if (now - (*ptr)->last_seen > CONNECTION_TIMEOUT) {
                tcp_connection_state_t *to_free = *ptr;
                *ptr = to_free->next;
                free(to_free);
                g_server.enhanced_stats.tcp_connections_tracked--;
            } else {
                ptr = &(*ptr)->next;
            }
        }
    }
    
    pthread_mutex_unlock(&g_server.detection_mutex);
}

// Protocol identification with state tracking
protocol_type_t identify_protocol_with_state(int is_tcp, uint32_t src_ip, uint32_t dst_ip, 
                                            uint16_t src_port, uint16_t dst_port, uint32_t seq_number,
                                            const uint8_t *payload, uint32_t payload_len, uint8_t *confidence) {
    
    // Check for existing TCP connection state
    if (is_tcp) {
        protocol_type_t state = find_connection_state(src_ip, dst_ip, src_port, dst_port);
        if (state != PROTOCOL_UNKNOWN) {
            *confidence = 100; // High confidence for known state
            return state;
        }
    }
    
    // Perform enhanced detection
    protocol_type_t detected = detect_protocol_enhanced(payload, payload_len, confidence, is_tcp);
    
    // Update state for TCP connections
    if (is_tcp && detected != PROTOCOL_UNKNOWN) {
        update_connection_state(src_ip, dst_ip, src_port, dst_port, detected, seq_number, *confidence);
    }
    
    return detected;
}

// Legacy protocol detectors (kept for compatibility)
int detect_http(const uint8_t* data, size_t len) {
    return detect_http_enhanced(data, len);
}

int detect_dns(const uint8_t* data, size_t len) {
    return detect_dns_enhanced(data, len);
}

int detect_mqtt(const uint8_t* data, size_t len) {
    return detect_mqtt_enhanced(data, len);
}

int detect_coap(const uint8_t* data, size_t len) {
    return detect_coap_enhanced(data, len);
}

// Utility functions
const char* protocol_to_string(protocol_type_t protocol) {
    switch (protocol) {
        case PROTOCOL_MQTT: return "MQTT";
        case PROTOCOL_COAP: return "CoAP";
        case PROTOCOL_HTTP: return "HTTP";
        case PROTOCOL_DNS: return "DNS";
        case PROTOCOL_TLS: return "TLS";
        case PROTOCOL_QUIC: return "QUIC";
        case PROTOCOL_UNKNOWN: default: return "Unknown";
    }
}

// Print enhanced statistics
void print_enhanced_stats(void) {
    printf("\n=== ENHANCED PROTOCOL STATISTICS ===\n");
    printf("Total packets: %lu\n", g_server.enhanced_stats.total_packets);
    printf("Identified packets: %lu (%.2f%%)\n", g_server.enhanced_stats.identified_packets,
           g_server.enhanced_stats.total_packets > 0 ? 
           (double)g_server.enhanced_stats.identified_packets / g_server.enhanced_stats.total_packets * 100 : 0);
    printf("TCP connections tracked: %lu\n", g_server.enhanced_stats.tcp_connections_tracked);
    printf("UDP packets processed: %lu\n", g_server.enhanced_stats.udp_packets_processed);
    printf("Detection confidence - High: %lu, Medium: %lu, Low: %lu\n",
           g_server.enhanced_stats.detection_confidence_high,
           g_server.enhanced_stats.detection_confidence_medium,
           g_server.enhanced_stats.detection_confidence_low);
    printf("Protocol distribution:\n");
    printf("  Unknown: %lu, MQTT: %lu, CoAP: %lu, HTTP: %lu, DNS: %lu, TLS: %lu, QUIC: %lu\n",
           g_server.enhanced_stats.protocol_count[0], g_server.enhanced_stats.protocol_count[1],
           g_server.enhanced_stats.protocol_count[2], g_server.enhanced_stats.protocol_count[3],
           g_server.enhanced_stats.protocol_count[4], g_server.enhanced_stats.protocol_count[5],
           g_server.enhanced_stats.protocol_count[6]);
    
    // Dynamic confidence system statistics
    printf("\n=== DYNAMIC CONFIDENCE SYSTEM ===\n");
    printf("Calibration factor: %.3f\n", g_server.enhanced_stats.confidence_calibration_factor);
    printf("Average confidence error: %.3f\n", g_server.enhanced_stats.average_confidence_error);
    printf("Confidence history entries: %u\n", g_server.enhanced_stats.confidence_history_index);
    
    // Protocol accuracy statistics
    printf("\n=== PROTOCOL ACCURACY STATISTICS ===\n");
    const char* protocol_names[] = {"Unknown", "MQTT", "CoAP", "HTTP", "DNS", "TLS", "QUIC"};
    for (int i = 0; i < 7; i++) {
        protocol_accuracy_t *acc = &g_server.enhanced_stats.protocol_accuracy[i];
        if (acc->total_detections > 0) {
            printf("%s: Total=%u, Correct=%u, Accuracy=%.2f%%, Precision=%.2f%%, Recall=%.2f%%, F1=%.2f%%, Adj=%.3f\n",
                   protocol_names[i], acc->total_detections, acc->correct_detections,
                   acc->accuracy_rate * 100, acc->precision_rate * 100, acc->recall_rate * 100,
                   acc->f1_score * 100, acc->confidence_adjustment);
        }
    }
    
    printf("=====================================\n\n");
}

// Initialize packet capture
int init_packet_capture(void) {
    if (capture_initialized) return 0;
    
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // Open live capture on "any" interface
    capture_handle = pcap_open_live("any", BUFSIZ, 1, 1000, errbuf);
    if (capture_handle == NULL) {
        fprintf(stderr, "Couldn't open device: %s\n", errbuf);
        return -1;
    }
    
    // Set filter for TCP and UDP only
    struct bpf_program fp;
    char filter_exp[] = "tcp or udp";
    if (pcap_compile(capture_handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(capture_handle));
        pcap_close(capture_handle);
        return -1;
    }
    
    if (pcap_setfilter(capture_handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(capture_handle));
        pcap_close(capture_handle);
        return -1;
    }
    
    // Initialize state table
    for (int i = 0; i < STATE_TABLE_SIZE; i++) {
        g_server.state_table[i] = NULL;
    }
    
    capture_initialized = 1;
    printf("[CAPTURE] Packet capture initialized successfully\n");
    return 0;
}

// Cleanup packet capture
void cleanup_packet_capture(void) {
    if (capture_handle) {
        pcap_close(capture_handle);
        capture_handle = NULL;
    }
    
    // Cleanup state table
    cleanup_old_connections();
    
    capture_initialized = 0;
    printf("[CAPTURE] Packet capture cleaned up\n");
}

// Packet handler for libpcap
void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    g_server.enhanced_stats.total_packets++;
    
    // Parse Ethernet header
    struct ethhdr *eth_header = (struct ethhdr *)packet;
    if (ntohs(eth_header->h_proto) != ETH_P_IP) return;
    
    // Parse IP header
    struct iphdr *ip_header = (struct iphdr *)(packet + sizeof(struct ethhdr));
    uint32_t src_ip = ntohl(ip_header->saddr);
    uint32_t dst_ip = ntohl(ip_header->daddr);
    
    int is_tcp = 0;
    uint16_t src_port = 0, dst_port = 0;
    uint32_t seq_number = 0;
    const u_char *payload = NULL;
    uint32_t payload_len = 0;
    uint32_t ip_header_len = ip_header->ihl * 4;
    
    if (ip_header->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp_header = (struct tcphdr *)(packet + sizeof(struct ethhdr) + ip_header_len);
        src_port = ntohs(tcp_header->source);
        dst_port = ntohs(tcp_header->dest);
        seq_number = ntohl(tcp_header->seq);
        
        uint32_t tcp_header_len = tcp_header->doff * 4;
        payload = packet + sizeof(struct ethhdr) + ip_header_len + tcp_header_len;
        payload_len = ntohs(ip_header->tot_len) - ip_header_len - tcp_header_len;
        is_tcp = 1;
    } else if (ip_header->protocol == IPPROTO_UDP) {
        struct udphdr *udp_header = (struct udphdr *)(packet + sizeof(struct ethhdr) + ip_header_len);
        src_port = ntohs(udp_header->source);
        dst_port = ntohs(udp_header->dest);
        
        payload = packet + sizeof(struct ethhdr) + ip_header_len + sizeof(struct udphdr);
        payload_len = ntohs(udp_header->len) - sizeof(struct udphdr);
        g_server.enhanced_stats.udp_packets_processed++;
    } else {
        return;
    }
    
    if (payload_len == 0) return;
    
    // Clean up old connections periodically
    if (g_server.enhanced_stats.total_packets % 1000 == 0) {
        cleanup_old_connections();
    }
    
    // Identify protocol with state tracking
    uint8_t confidence = 0;
    protocol_type_t protocol = identify_protocol_with_state(is_tcp, src_ip, dst_ip, src_port, dst_port, 
                                                           seq_number, payload, payload_len, &confidence);
    
    if (protocol != PROTOCOL_UNKNOWN) {
        g_server.enhanced_stats.identified_packets++;
        g_server.enhanced_stats.protocol_count[protocol]++;
        
        // Update confidence statistics
        if (confidence >= DETECTION_CONFIDENCE_HIGH) {
            g_server.enhanced_stats.detection_confidence_high++;
        } else if (confidence >= DETECTION_CONFIDENCE_MEDIUM) {
            g_server.enhanced_stats.detection_confidence_medium++;
        } else if (confidence >= DETECTION_CONFIDENCE_LOW) {
            g_server.enhanced_stats.detection_confidence_low++;
        }
        
        printf("Detected %s: %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d (size: %u, confidence: %d%%)\n",
               protocol_to_string(protocol),
               (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF, 
               (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port,
               (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
               (dst_ip >> 8) & 0xFF, dst_ip & 0xFF, dst_port,
               payload_len, confidence);
    } else {
        g_server.enhanced_stats.protocol_count[PROTOCOL_UNKNOWN]++;
    }
    
    // Print statistics every 100 packets
    if (g_server.enhanced_stats.total_packets % 100 == 0) {
        print_enhanced_stats();
    }
}

// ============================================================================
// DYNAMIC CONFIDENCE SYSTEM IMPLEMENTATION
// ============================================================================

// Calculate dynamic confidence based on multiple features
uint8_t calculate_dynamic_confidence(protocol_type_t protocol, const uint8_t *data, size_t len, 
                                   int is_tcp, confidence_features_t *features) {
    // Extract all confidence features
    extract_confidence_features(data, len, is_tcp, protocol, features);
    
    // Calculate weighted confidence score
    float confidence_score = 0.0f;
    
    // Feature weights (can be adjusted based on importance)
    const float weights[10] = {
        0.15f,  // entropy_score
        0.20f,  // pattern_strength
        0.15f,  // validation_depth
        0.10f,  // header_consistency
        0.10f,  // payload_structure
        0.05f,  // transport_compatibility
        0.05f,  // context_relevance
        0.10f,  // historical_accuracy
        0.05f,  // false_positive_risk
        0.05f   // protocol_specificity
    };
    
    // Apply weights to features
    confidence_score += features->entropy_score * weights[0];
    confidence_score += features->pattern_strength * weights[1];
    confidence_score += features->validation_depth * weights[2];
    confidence_score += features->header_consistency * weights[3];
    confidence_score += features->payload_structure * weights[4];
    confidence_score += features->transport_compatibility * weights[5];
    confidence_score += features->context_relevance * weights[6];
    confidence_score += features->historical_accuracy * weights[7];
    confidence_score += (1.0f - features->false_positive_risk) * weights[8]; // Invert risk
    confidence_score += features->protocol_specificity * weights[9];
    

    
    // Apply calibration factor
    confidence_score *= g_server.enhanced_stats.confidence_calibration_factor;
    
    // Convert to percentage (0-100 range)
    confidence_score *= 100.0f;
    
    // Clamp to valid range
    if (confidence_score < MIN_CONFIDENCE_THRESHOLD) {
        confidence_score = MIN_CONFIDENCE_THRESHOLD;
    } else if (confidence_score > MAX_CONFIDENCE_THRESHOLD) {
        confidence_score = MAX_CONFIDENCE_THRESHOLD;
    }
    
    return (uint8_t)confidence_score;
}

// Extract all confidence features from packet data
void extract_confidence_features(const uint8_t *data, size_t len, int is_tcp, 
                               protocol_type_t protocol, confidence_features_t *features) {
    features->entropy_score = calculate_entropy_score(data, len);
    features->pattern_strength = calculate_pattern_strength(data, len, protocol);
    features->validation_depth = calculate_validation_depth(data, len, protocol);
    features->header_consistency = calculate_header_consistency(data, len, protocol);
    features->payload_structure = calculate_payload_structure(data, len, protocol);
    features->transport_compatibility = calculate_transport_compatibility(is_tcp, protocol);
    features->context_relevance = calculate_context_relevance(data, len, protocol);
    features->historical_accuracy = get_historical_accuracy(protocol);
    features->false_positive_risk = calculate_false_positive_risk(data, len, protocol);
    features->protocol_specificity = calculate_protocol_specificity(data, len, protocol);
}

// Calculate packet entropy (measure of randomness/encryption)
float calculate_entropy_score(const uint8_t *data, size_t len) {
    if (len < 16) return 0.5f; // Short packets get medium entropy
    
    int byte_counts[256] = {0};
    float entropy = 0.0f;
    
    // Count byte frequencies
    for (size_t i = 0; i < len; i++) {
        byte_counts[data[i]]++;
    }
    
    // Calculate Shannon entropy
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) {
            float p = (float)byte_counts[i] / len;
            entropy -= p * log2f(p);
        }
    }
    
    // Normalize to 0-1 range (max entropy is 8 bits)
    float normalized_entropy = entropy / 8.0f;
    
    // High entropy suggests encryption (TLS/QUIC), low suggests text (HTTP)
    return normalized_entropy;
}

// Calculate pattern strength (how distinctive the protocol patterns are)
float calculate_pattern_strength(const uint8_t *data, size_t len, protocol_type_t protocol) {
    float strength = 0.0f;
    
    switch (protocol) {
        case PROTOCOL_TLS:
            // TLS has very distinctive content types and version patterns
            if (len >= 5) {
                uint8_t content_type = data[0];
                uint16_t version = ntohs(*(uint16_t*)(data + 1));
                
                if (content_type >= 20 && content_type <= 23) strength += 0.4f;
                if (version >= 0x0300 && version <= 0x0304) strength += 0.4f;
                if (len >= 6 && content_type == 22) { // Handshake
                    uint8_t handshake_type = data[5];
                    if (handshake_type <= 20) strength += 0.2f;
                }
            }
            break;
            
        case PROTOCOL_HTTP:
            // HTTP has distinctive method patterns
            if (len >= 4) {
                if (strncmp((char*)data, "GET ", 4) == 0) strength += 0.8f;
                else if (strncmp((char*)data, "POST ", 5) == 0) strength += 0.8f;
                else if (strncmp((char*)data, "HTTP/", 5) == 0) strength += 0.8f;
                else if (strstr((char*)data, "Host:") != NULL) strength += 0.6f;
                else strength += 0.3f;
            }
            break;
            
        case PROTOCOL_MQTT:
            // MQTT has distinctive packet type and flag patterns
            if (len >= 2) {
                uint8_t packet_type = (data[0] >> 4) & 0x0F;
                if (packet_type >= 1 && packet_type <= 14) strength += 0.6f;
                
                // Check for MQTT protocol name in CONNECT
                if (packet_type == 1 && len >= 10) {
                    if (strncmp((char*)(data + 6), "MQTT", 4) == 0) strength += 0.4f;
                }
            }
            break;
            
        case PROTOCOL_COAP:
            // CoAP has distinctive version and code patterns
            if (len >= 4) {
                uint8_t version = (data[0] >> 6) & 0x03;
                uint8_t code = data[1];
                
                if (version == 1) strength += 0.5f;
                if (code >= 1 && code <= 31) strength += 0.5f;
            }
            break;
            
        case PROTOCOL_DNS:
            // DNS has distinctive flag patterns
            if (len >= 12) {
                uint16_t flags = ntohs(*(uint16_t*)(data + 2));
                uint8_t opcode = (flags >> 11) & 0x0F;
                
                if (opcode <= 5) strength += 0.7f;
                if ((flags & 0x8000) == 0) strength += 0.3f; // Query
            }
            break;
            
        case PROTOCOL_QUIC:
            // QUIC has distinctive long header patterns
            if (len >= 6 && (data[0] & 0x80) == 0x80) {
                strength += 0.8f;
                uint32_t version = ntohl(*(uint32_t*)(data + 1));
                if (version == 0x00000000 || version >= 0x51303030) strength += 0.2f;
            }
            break;
            
        default:
            strength = 0.3f; // Unknown protocols get low strength
            break;
    }
    
    return (strength > 1.0f) ? 1.0f : strength;
}

// Calculate validation depth (how many checks passed)
float calculate_validation_depth(const uint8_t *data, size_t len, protocol_type_t protocol) {
    int checks_passed = 0;
    int total_checks = 0;
    
    switch (protocol) {
        case PROTOCOL_HTTP:
            total_checks = 3;
            if (len >= 4) checks_passed++; // Length check
            if (detect_http_enhanced(data, len)) checks_passed++; // Method/response check
            if (len > 20 && strstr((char*)data, "Host:") != NULL) checks_passed++; // Header check
            break;
            
        case PROTOCOL_MQTT:
            total_checks = 4;
            if (len >= 2) checks_passed++; // Length check
            if (detect_mqtt_enhanced(data, len)) checks_passed++; // Basic validation
            if (len >= 2) {
                uint8_t packet_type = (data[0] >> 4) & 0x0F;
                if (packet_type >= 1 && packet_type <= 14) checks_passed++; // Type validation
            }
            if (len >= 6 && strncmp((char*)(data + 6), "MQTT", 4) == 0) checks_passed++; // Protocol name
            break;
            
        case PROTOCOL_COAP:
            total_checks = 3;
            if (len >= 4) checks_passed++; // Length check
            if (detect_coap_enhanced(data, len)) checks_passed++; // Basic validation
            if (len >= 4) {
                uint8_t version = (data[0] >> 6) & 0x03;
                if (version == 1) checks_passed++; // Version check
            }
            break;
            
        case PROTOCOL_DNS:
            total_checks = 4;
            if (len >= 12) checks_passed++; // Length check
            if (detect_dns_enhanced(data, len)) checks_passed++; // Basic validation
            if (len >= 12) {
                uint16_t flags = ntohs(*(uint16_t*)(data + 2));
                uint8_t opcode = (flags >> 11) & 0x0F;
                if (opcode <= 5) checks_passed++; // Opcode validation
            }
            if (len >= 12) {
                uint16_t flags = ntohs(*(uint16_t*)(data + 2));
                if ((flags & 0x8000) == 0) checks_passed++; // Query/response check
            }
            break;
            
        case PROTOCOL_TLS:
            total_checks = 4;
            if (len >= 5) checks_passed++; // Length check
            if (detect_tls_enhanced(data, len)) checks_passed++; // Basic validation
            if (len >= 5) {
                uint8_t content_type = data[0];
                if (content_type >= 20 && content_type <= 23) checks_passed++; // Content type
            }
            if (len >= 5) {
                uint16_t version = ntohs(*(uint16_t*)(data + 1));
                if (version >= 0x0300 && version <= 0x0304) checks_passed++; // Version
            }
            break;
            
        case PROTOCOL_QUIC:
            total_checks = 3;
            if (len >= 6) checks_passed++; // Length check
            if (detect_quic_enhanced(data, len)) checks_passed++; // Basic validation
            if (len >= 6 && (data[0] & 0x80) == 0x80) checks_passed++; // Long header
            break;
            
        default:
            return 0.5f; // Unknown protocols get medium validation depth
    }
    
    return (float)checks_passed / total_checks;
}

// Calculate header consistency
float calculate_header_consistency(const uint8_t *data, size_t len, protocol_type_t protocol) {
    float consistency = 0.5f; // Base consistency
    
    switch (protocol) {
        case PROTOCOL_HTTP:
            // Check for consistent HTTP header structure
            if (len > 20) {
                const char *headers[] = {"Host:", "User-Agent:", "Content-Type:", "Content-Length:"};
                int found_headers = 0;
                for (int i = 0; i < 4; i++) {
                    if (strstr((char*)data, headers[i]) != NULL) found_headers++;
                }
                consistency += (float)found_headers * 0.1f;
            }
            break;
            
        case PROTOCOL_MQTT:
            // Check for consistent MQTT header structure
            if (len >= 2) {
                uint8_t packet_type = (data[0] >> 4) & 0x0F;
                uint8_t flags = data[0] & 0x0F;
                
                // Validate flags for packet type
                if ((packet_type == 1 && flags == 0) || // CONNECT
                    (packet_type == 3 && (flags & 0x06) != 0x06) || // PUBLISH
                    (packet_type == 8 && flags == 2)) { // SUBSCRIBE
                    consistency += 0.3f;
                }
            }
            break;
            
        case PROTOCOL_DNS:
            // Check for consistent DNS header structure
            if (len >= 12) {
                uint16_t flags = ntohs(*(uint16_t*)(data + 2));
                uint8_t qr = (flags >> 15) & 0x1;
                uint8_t opcode = (flags >> 11) & 0xF;
                
                if (opcode <= 5 && (qr == 0 || qr == 1)) {
                    consistency += 0.4f;
                }
            }
            break;
            
        default:
            consistency = 0.6f; // Default consistency for other protocols
            break;
    }
    
    return (consistency > 1.0f) ? 1.0f : consistency;
}

// Calculate payload structure validity
float calculate_payload_structure(const uint8_t *data, size_t len, protocol_type_t protocol) {
    float structure_score = 0.5f; // Base score
    
    switch (protocol) {
        case PROTOCOL_HTTP:
            // Check for proper HTTP structure
            if (len >= 4) {
                if (strncmp((char*)data, "GET ", 4) == 0 || 
                    strncmp((char*)data, "POST ", 5) == 0 ||
                    strncmp((char*)data, "HTTP/", 5) == 0) {
                    structure_score += 0.4f;
                }
            }
            break;
            
        case PROTOCOL_MQTT:
            // Check for proper MQTT structure
            if (len >= 2) {
                uint8_t packet_type = (data[0] >> 4) & 0x0F;
                if (packet_type >= 1 && packet_type <= 14) {
                    structure_score += 0.3f;
                    
                    // Check variable length field
                    if (len >= 3) {
                        uint32_t index = 1;
                        uint32_t multiplier = 1;
                        uint32_t length = 0;
                        
                        do {
                            if (index >= len) break;
                            uint8_t encoded_byte = data[index++];
                            length += (encoded_byte & 0x7F) * multiplier;
                            multiplier *= 128;
                            if (multiplier > 128 * 128 * 128) break;
                        } while (index < len && (data[index-1] & 0x80));
                        
                        if (index + length <= len) {
                            structure_score += 0.2f;
                        }
                    }
                }
            }
            break;
            
        case PROTOCOL_DNS:
            // Check for proper DNS structure
            if (len >= 12) {
                uint16_t qdcount = ntohs(*(uint16_t*)(data + 4));
                uint16_t ancount = ntohs(*(uint16_t*)(data + 6));
                
                if (qdcount <= 100 && ancount <= 1000) {
                    structure_score += 0.4f;
                }
            }
            break;
            
        default:
            structure_score = 0.6f; // Default for other protocols
            break;
    }
    
    return (structure_score > 1.0f) ? 1.0f : structure_score;
}

// Calculate transport compatibility
float calculate_transport_compatibility(int is_tcp, protocol_type_t protocol) {
    switch (protocol) {
        case PROTOCOL_HTTP:
            return 1.0f; // HTTP works on both TCP and UDP
        case PROTOCOL_MQTT:
            return is_tcp ? 1.0f : 0.0f; // MQTT is TCP-only
        case PROTOCOL_COAP:
            return is_tcp ? 0.0f : 1.0f; // CoAP is UDP-only
        case PROTOCOL_DNS:
            return 1.0f; // DNS works on both TCP and UDP
        case PROTOCOL_TLS:
            return is_tcp ? 1.0f : 0.0f; // TLS is TCP-only
        case PROTOCOL_QUIC:
            return is_tcp ? 0.0f : 1.0f; // QUIC is UDP-only
        default:
            return 0.5f; // Unknown protocols get medium compatibility
    }
}

// Calculate context relevance
float calculate_context_relevance(const uint8_t *data, size_t len, protocol_type_t protocol) {
    // This could be enhanced with connection state analysis
    // For now, return a base score based on protocol characteristics
    switch (protocol) {
        case PROTOCOL_TLS:
            return 0.9f; // TLS is very context-relevant
        case PROTOCOL_HTTP:
            return 0.8f; // HTTP is context-relevant
        case PROTOCOL_MQTT:
            return 0.7f; // MQTT is somewhat context-relevant
        case PROTOCOL_DNS:
            return 0.6f; // DNS is less context-relevant
        case PROTOCOL_COAP:
            return 0.5f; // CoAP is stateless
        case PROTOCOL_QUIC:
            return 0.8f; // QUIC is context-relevant
        default:
            return 0.5f; // Unknown protocols get medium relevance
    }
}

// Get historical accuracy for a protocol
float get_historical_accuracy(protocol_type_t protocol) {
    if (protocol < 0 || protocol >= 7) return 0.5f;
    
    protocol_accuracy_t *acc = &g_server.enhanced_stats.protocol_accuracy[protocol];
    
    if (acc->total_detections == 0) {
        return 0.7f; // Default accuracy for new protocols
    }
    
    return acc->accuracy_rate;
}

// Calculate false positive risk
float calculate_false_positive_risk(const uint8_t *data, size_t len, protocol_type_t protocol) {
    float risk = 0.3f; // Base risk
    
    switch (protocol) {
        case PROTOCOL_TLS:
            risk = 0.1f; // Very low risk - TLS is very distinctive
            break;
        case PROTOCOL_QUIC:
            risk = 0.15f; // Low risk - QUIC has unique patterns
            break;
        case PROTOCOL_DNS:
            risk = 0.2f; // Low risk - DNS has well-defined structure
            break;
        case PROTOCOL_HTTP:
            risk = 0.25f; // Medium-low risk - HTTP has clear patterns
            break;
        case PROTOCOL_MQTT:
            risk = 0.3f; // Medium risk - MQTT can be confused with binary protocols
            break;
        case PROTOCOL_COAP:
            risk = 0.35f; // Medium-high risk - CoAP is more flexible
            break;
        default:
            risk = 0.5f; // High risk for unknown protocols
            break;
    }
    
    // Adjust risk based on packet length
    if (len < 10) risk += 0.1f; // Short packets are riskier
    if (len > 1000) risk += 0.05f; // Very long packets might be fragmented
    
    return (risk > 1.0f) ? 1.0f : risk;
}

// Calculate protocol-specific features
float calculate_protocol_specificity(const uint8_t *data, size_t len, protocol_type_t protocol) {
    float specificity = 0.5f; // Base specificity
    
    switch (protocol) {
        case PROTOCOL_TLS:
            // TLS has very specific content types and versions
            if (len >= 5) {
                uint8_t content_type = data[0];
                uint16_t version = ntohs(*(uint16_t*)(data + 1));
                
                if (content_type >= 20 && content_type <= 23) specificity += 0.3f;
                if (version >= 0x0300 && version <= 0x0304) specificity += 0.2f;
            }
            break;
            
        case PROTOCOL_HTTP:
            // HTTP has specific method patterns
            if (len >= 4) {
                if (strncmp((char*)data, "GET ", 4) == 0) specificity += 0.4f;
                else if (strncmp((char*)data, "POST ", 5) == 0) specificity += 0.4f;
                else if (strncmp((char*)data, "HTTP/", 5) == 0) specificity += 0.4f;
            }
            break;
            
        case PROTOCOL_MQTT:
            // MQTT has specific packet types and protocol name
            if (len >= 2) {
                uint8_t packet_type = (data[0] >> 4) & 0x0F;
                if (packet_type >= 1 && packet_type <= 14) specificity += 0.3f;
                
                if (packet_type == 1 && len >= 10) {
                    if (strncmp((char*)(data + 6), "MQTT", 4) == 0) specificity += 0.2f;
                }
            }
            break;
            
        default:
            specificity = 0.6f; // Default for other protocols
            break;
    }
    
    return (specificity > 1.0f) ? 1.0f : specificity;
}

// Update protocol accuracy statistics
void update_protocol_accuracy(protocol_type_t protocol, uint8_t predicted_confidence, 
                            uint8_t actual_confidence, uint8_t was_correct) {
    if (protocol < 0 || protocol >= 7) return;
    
    protocol_accuracy_t *acc = &g_server.enhanced_stats.protocol_accuracy[protocol];
    
    acc->total_detections++;
    
    if (was_correct) {
        acc->correct_detections++;
    } else {
        if (predicted_confidence > actual_confidence) {
            acc->false_positives++;
        } else {
            acc->false_negatives++;
        }
    }
    
    // Calculate accuracy metrics
    acc->accuracy_rate = (float)acc->correct_detections / acc->total_detections;
    acc->precision_rate = (acc->correct_detections + acc->false_positives) > 0 ? 
                         (float)acc->correct_detections / (acc->correct_detections + acc->false_positives) : 0.0f;
    acc->recall_rate = (acc->correct_detections + acc->false_negatives) > 0 ? 
                      (float)acc->correct_detections / (acc->correct_detections + acc->false_negatives) : 0.0f;
    
    // Calculate F1 score
    if (acc->precision_rate + acc->recall_rate > 0) {
        acc->f1_score = 2.0f * (acc->precision_rate * acc->recall_rate) / (acc->precision_rate + acc->recall_rate);
    } else {
        acc->f1_score = 0.0f;
    }
    
    // Update confidence adjustment factor
    float confidence_error = fabsf((float)predicted_confidence - actual_confidence) / 100.0f;
    acc->confidence_adjustment = 1.0f - (confidence_error * ADAPTIVE_LEARNING_RATE);
    
    acc->last_update = time(NULL);
}

// Add confidence history entry
void add_confidence_history(protocol_type_t protocol, uint8_t predicted_confidence, 
                          uint8_t actual_confidence, uint8_t was_correct, 
                          const confidence_features_t *features) {
    uint32_t index = g_server.enhanced_stats.confidence_history_index;
    
    g_server.enhanced_stats.confidence_history[index].protocol = protocol;
    g_server.enhanced_stats.confidence_history[index].predicted_confidence = predicted_confidence;
    g_server.enhanced_stats.confidence_history[index].actual_confidence = actual_confidence;
    g_server.enhanced_stats.confidence_history[index].was_correct = was_correct;
    g_server.enhanced_stats.confidence_history[index].timestamp = time(NULL);
    
    if (features) {
        g_server.enhanced_stats.confidence_history[index].features = *features;
    }
    
    // Circular buffer
    g_server.enhanced_stats.confidence_history_index = 
        (g_server.enhanced_stats.confidence_history_index + 1) % CONFIDENCE_HISTORY_SIZE;
}

// Calibrate confidence system
void calibrate_confidence_system(void) {
    float total_error = 0.0f;
    int error_count = 0;
    
    // Calculate average confidence error from history
    for (int i = 0; i < CONFIDENCE_HISTORY_SIZE; i++) {
        confidence_history_entry_t *entry = &g_server.enhanced_stats.confidence_history[i];
        if (entry->timestamp > 0) {
            float error = calculate_confidence_error(entry->predicted_confidence, entry->actual_confidence);
            total_error += error;
            error_count++;
        }
    }
    
    if (error_count > 0) {
        g_server.enhanced_stats.average_confidence_error = total_error / error_count;
        
        // Adjust calibration factor based on error
        if (g_server.enhanced_stats.average_confidence_error > 0.2f) {
            g_server.enhanced_stats.confidence_calibration_factor *= 0.95f; // Reduce confidence
        } else if (g_server.enhanced_stats.average_confidence_error < 0.1f) {
            g_server.enhanced_stats.confidence_calibration_factor *= 1.05f; // Increase confidence
        }
        
        // Clamp calibration factor
        if (g_server.enhanced_stats.confidence_calibration_factor < 0.5f) {
            g_server.enhanced_stats.confidence_calibration_factor = 0.5f;
        } else if (g_server.enhanced_stats.confidence_calibration_factor > 1.5f) {
            g_server.enhanced_stats.confidence_calibration_factor = 1.5f;
        }
    }
}

// Calculate confidence error
float calculate_confidence_error(uint8_t predicted, uint8_t actual) {
    return fabsf((float)predicted - actual) / 100.0f;
}

// Update confidence calibration
void update_confidence_calibration(float error) {
    g_server.enhanced_stats.average_confidence_error = 
        (g_server.enhanced_stats.average_confidence_error * 0.9f) + (error * 0.1f);
}
