#include "../../include/protocol/protocol_detector.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
    
    int scores[5] = {0}; // mqtt, coap, http, dns scores
    
    // Try HTTP detection first (most distinctive)
    if (detect_http(data, len)) {
        scores[3] = 15;
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
        scores[2] = 8;
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

int detect_http(const uint8_t* data, size_t len) {
    if (len < 14) {
        printf("[HTTP_DETECT] Packet too short (%zu bytes)\n", len);
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
            method_found = 1;
            break;
        }
    }
    
    if (!method_found) {
        printf("[HTTP_DETECT] No valid HTTP method found\n");
        return 0;
    }
    
    // Look for HTTP version string with bounded search
    const char* http_pos = NULL;
    for (size_t i = 0; i + 5 < len; i++) {
        if (memcmp(data + i, "HTTP/", 5) == 0) {
            http_pos = (const char*)(data + i);
            break;
        }
    }
    if (!http_pos) {
        printf("[HTTP_DETECT] HTTP version string not found\n");
        return 0;
    }
    
    // Check if it's within reasonable bounds
    if (http_pos - (char*)data > (long)(len - 8)) {
        printf("[HTTP_DETECT] HTTP version string beyond packet boundary\n");
        return 0;
    }
    
    // Verify HTTP version format (HTTP/x.x)
    if (http_pos[5] != '.' || !isdigit(http_pos[4]) || !isdigit(http_pos[6])) {
        printf("[HTTP_DETECT] Invalid HTTP version format\n");
        return 0;
    }
    
    // Look for CRLF (\r\n) which should be present in HTTP
    const char* crlf_pos = NULL;
    for (size_t i = 0; i + 1 < len; i++) {
        if (data[i] == '\r' && data[i+1] == '\n') {
            crlf_pos = (const char*)(data + i);
            break;
        }
    }
    if (!crlf_pos) {
        printf("[HTTP_DETECT] CRLF not found\n");
        return 0;
    }
    
    printf("[HTTP_DETECT] Valid HTTP request detected\n");
    return 1;
}

int detect_dns(const uint8_t* data, size_t len) {
    if (len < 12) {
        printf("[DNS_DETECT] Packet too short (%zu bytes)\n", len);
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
    if (qr == 1) {
        uint8_t rcode = flags & 0x0F;
        if (rcode > 5) {
            printf("[DNS_DETECT] Invalid RCODE: %d\n", rcode);
            return 0;
        }
    }
    
    // Reasonable limits check
    if (questions > 100 || answers > 100 || authority > 100 || additional > 100) {
        printf("[DNS_DETECT] Suspicious record counts - Q:%d A:%d Auth:%d Add:%d\n",
               questions, answers, authority, additional);
        return 0;
    }
    
    // For queries, there should be at least one question
    if (qr == 0 && questions == 0) {
        printf("[DNS_DETECT] Query with no questions\n");
        return 0;
    }
    
    // If there are questions, validate the first one
    if (questions > 0 && len > 12) {
        size_t pos = 12;
        int label_count = 0;
        
        // Parse QNAME (simplified validation)
        while (pos < len && label_count < 63) {
            uint8_t label_len = data[pos];
            
            if (label_len == 0) {
                pos++; // End of QNAME
                break;
            } else if ((label_len & 0xC0) == 0xC0) {
                pos += 2; // Compression pointer
                break;
            } else if (label_len > 63) {
                printf("[DNS_DETECT] Invalid label length: %d\n", label_len);
                return 0;
            } else {
                pos += label_len + 1;
                label_count++;
            }
        }
        
        // Check if we have space for QTYPE and QCLASS
        if (pos + 4 > len) {
            printf("[DNS_DETECT] Incomplete question section\n");
            return 0;
        }
        
        uint16_t qtype = (data[pos] << 8) | data[pos + 1];
        uint16_t qclass = (data[pos + 2] << 8) | data[pos + 3];
        
        // Validate QTYPE and QCLASS
        if (qtype == 0 || qtype > 255 || (qclass != 1 && qclass != 255)) {
            printf("[DNS_DETECT] Invalid QTYPE (%d) or QCLASS (%d)\n", qtype, qclass);
            return 0;
        }
    }
    
    printf("[DNS_DETECT] Valid DNS packet - QR:%d OPCODE:%d Questions:%d Answers:%d\n",
           qr, opcode, questions, answers);
    
    return 1;
}

int detect_mqtt(const uint8_t* data, size_t len) {
    if (len < 2) {
        printf("[MQTT_DETECT] Packet too short (%zu bytes)\n", len);
        return 0;
    }
    
    uint8_t first_byte = data[0];
    uint8_t msg_type = (first_byte >> 4) & 0x0F;
    
    if (msg_type < 1 || msg_type > 14) {
        printf("[MQTT_DETECT] Invalid message type: %d (0x%02X)\n", msg_type, first_byte);
        return 0;
    }
    
    uint8_t flags = first_byte & 0x0F;
    switch (msg_type) {
        case 1: case 2: case 4: case 5: case 6: case 7: case 8: 
        case 9: case 10: case 11: case 12: case 13: case 14:
            if (flags != 0 && msg_type != 3) {
                printf("[MQTT_DETECT] Invalid flags for message type %d: 0x%X\n", msg_type, flags);
                return 0;
            }
            break;
        case 3: // PUBLISH
            if (((flags >> 1) & 0x03) == 3) {
                printf("[MQTT_DETECT] Invalid QoS in PUBLISH: %d\n", (flags >> 1) & 0x03);
                return 0;
            }
            break;
    }
    
    // Basic remaining length validation
    size_t byte_index = 1;
    int multiplier = 1;
    int remaining_length = 0;
    
    while (byte_index < len && byte_index < 5) {
        uint8_t encoded_byte = data[byte_index];
        remaining_length += (encoded_byte & 0x7F) * multiplier;
        
        if ((encoded_byte & 0x80) == 0) {
            break; // Last byte of remaining length
        }
        
        multiplier *= 128;
        byte_index++;
        
        if (byte_index >= 5) {
            printf("[MQTT_DETECT] Remaining length encoding too long\n");
            return 0;
        }
    }
    
    // Validate remaining length against available data
    size_t expected_total = byte_index + 1 + remaining_length;
    if (expected_total > len && len > 10) {
        printf("[MQTT_DETECT] Length mismatch - expected %zu, got %zu\n", expected_total, len);
        return 0;
    }
    
    printf("[MQTT_DETECT] Basic structure validation passed for message type %d\n", msg_type);
    return 1;
}

int detect_coap(const uint8_t* data, size_t len) {
    if (len < 4) {
        printf("[COAP_DETECT] Packet too short (%zu bytes)\n", len);
        return 0;
    }
    
    uint8_t first_byte = data[0];
    
    uint8_t version = (first_byte >> 6) & 0x03;
    if (version != 1) {
        printf("[COAP_DETECT] Invalid version: %d\n", version);
        return 0;
    }
    
    uint8_t msg_type = (first_byte >> 4) & 0x03;
    if (msg_type > 3) {
        printf("[COAP_DETECT] Invalid message type: %d\n", msg_type);
        return 0;
    }
    
    uint8_t token_length = first_byte & 0x0F;
    if (token_length > 8) {
        printf("[COAP_DETECT] Invalid token length: %d\n", token_length);
        return 0;
    }
    
    if (len < 4 + token_length) {
        printf("[COAP_DETECT] Packet too short for declared token length\n");
        return 0;
    }
    
    uint8_t code = data[1];
    uint8_t code_class = (code >> 5) & 0x07;
    
    switch (code_class) {
        case 0: case 2: case 4: case 5: break;
        case 1: case 3: case 6: case 7:
            printf("[COAP_DETECT] Reserved code class: %d\n", code_class);
            return 0;
        default:
            printf("[COAP_DETECT] Invalid code class: %d\n", code_class);
            return 0;
    }
    
    printf("[COAP_DETECT] Valid CoAP packet detected\n");
    return 1;
}

const char* protocol_to_string(protocol_type_t protocol) {
    switch (protocol) {
        case PROTOCOL_MQTT: return "MQTT";
        case PROTOCOL_COAP: return "CoAP";
        case PROTOCOL_HTTP: return "HTTP";
        case PROTOCOL_DNS: return "DNS";
        case PROTOCOL_UNKNOWN: default: return "Unknown";
    }
}
