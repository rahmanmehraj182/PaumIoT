#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Protocol detection based purely on packet headers
typedef enum {
    PROTOCOL_UNKNOWN,
    PROTOCOL_MQTT,
    PROTOCOL_COAP,
    PROTOCOL_HTTP,
    PROTOCOL_RTP
} protocol_type_t;

// MQTT Fixed Header Structure
/*
 * Byte 1: Message Type (4 bits) + Flags (4 bits)
 * Byte 2+: Remaining Length (1-4 bytes, variable encoding)
 * 
 * MQTT Message Types:
 * 1 = CONNECT, 2 = CONNACK, 3 = PUBLISH, 4 = PUBACK
 * 5 = PUBREC, 6 = PUBREL, 7 = PUBCOMP, 8 = SUBSCRIBE
 * 9 = SUBACK, 10 = UNSUBSCRIBE, 11 = UNSUBACK
 * 12 = PINGREQ, 13 = PINGRESP, 14 = DISCONNECT
 */

// CoAP Header Structure (4 bytes minimum)
/*
 * Byte 1: Version(2) + Type(2) + Token Length(4)
 * Byte 2: Code (3 bits class + 5 bits detail)
 * Bytes 3-4: Message ID (16 bits)
 * Bytes 5+: Token (0-8 bytes) + Options + Payload
 */

// Helper function to validate MQTT remaining length encoding
int is_valid_mqtt_remaining_length(const uint8_t *data, size_t length, uint32_t *decoded_length) {
    if (length == 0) return 0;
    
    uint32_t multiplier = 1;
    uint32_t value = 0;
    int index = 0;
    
    do {
        if (index >= length || index >= 4) return 0;  // Max 4 bytes for remaining length
        
        uint8_t byte = data[index];
        value += (byte & 0x7F) * multiplier;
        
        if (multiplier > 128 * 128 * 128) return 0;  // Overflow check
        multiplier *= 128;
        index++;
        
    } while ((data[index - 1] & 0x80) != 0);
    
    if (decoded_length) *decoded_length = value;
    return 1;  // Valid encoding
}

// Detailed MQTT detection
int detect_mqtt_detailed(const uint8_t *data, size_t length) {
    if (length < 2) return 0;
    
    // Extract message type from first byte
    uint8_t message_type = (data[0] >> 4) & 0x0F;
    uint8_t flags = data[0] & 0x0F;
    
    printf("  MQTT Analysis:\n");
    printf("    Message Type: %d ", message_type);
    
    // Check if message type is valid (1-14)
    if (message_type < 1 || message_type > 14) {
        printf("(INVALID - must be 1-14)\n");
        return 0;
    }
    
    // Print message type name
    const char* msg_names[] = {"", "CONNECT", "CONNACK", "PUBLISH", "PUBACK", 
                               "PUBREC", "PUBREL", "PUBCOMP", "SUBSCRIBE", 
                               "SUBACK", "UNSUBSCRIBE", "UNSUBACK", "PINGREQ", 
                               "PINGRESP", "DISCONNECT"};
    printf("(%s)\n", msg_names[message_type]);
    
    // Validate flags based on message type
    printf("    Flags: 0x%02X ", flags);
    switch (message_type) {
        case 1:  // CONNECT
        case 2:  // CONNACK
        case 4:  // PUBACK
        case 5:  // PUBREC
        case 7:  // PUBCOMP
        case 8:  // SUBSCRIBE
        case 9:  // SUBACK
        case 10: // UNSUBSCRIBE
        case 11: // UNSUBACK
        case 12: // PINGREQ
        case 13: // PINGRESP
        case 14: // DISCONNECT
            if (flags != 0) {
                printf("(INVALID - should be 0x00 for this message type)\n");
                return 0;
            }
            printf("(Valid)\n");
            break;
        case 3:  // PUBLISH
            printf("(DUP=%d, QoS=%d, RETAIN=%d)\n", 
                   (flags >> 3) & 1, (flags >> 1) & 3, flags & 1);
            break;
        case 6:  // PUBREL
            if ((flags & 0x0F) != 0x02) {
                printf("(INVALID - should be 0x02 for PUBREL)\n");
                return 0;
            }
            printf("(Valid)\n");
            break;
    }
    
    // Validate remaining length
    uint32_t remaining_length;
    if (!is_valid_mqtt_remaining_length(data + 1, length - 1, &remaining_length)) {
        printf("    Remaining Length: INVALID encoding\n");
        return 0;
    }
    
    printf("    Remaining Length: %u bytes\n", remaining_length);
    
    // Additional validation: check if remaining length makes sense with packet length
    int rl_bytes = 1;
    for (int i = 1; i < 5 && i < length; i++) {
        if ((data[i] & 0x80) == 0) break;
        rl_bytes++;
    }
    
    size_t expected_total = 1 + rl_bytes + remaining_length;  // Fixed header + remaining length + payload
    if (expected_total > length + 10) {  // Allow some tolerance for partial packets
        printf("    Length Check: SUSPICIOUS (expected ~%zu, got %zu)\n", expected_total, length);
        return 0;
    }
    
    printf("    Length Check: OK\n");
    return 1;  // Looks like valid MQTT
}

// Detailed CoAP detection
int detect_coap_detailed(const uint8_t *data, size_t length) {
    if (length < 4) return 0;  // CoAP header is minimum 4 bytes
    
    // Parse CoAP header
    uint8_t version = (data[0] >> 6) & 0x03;
    uint8_t type = (data[0] >> 4) & 0x03;
    uint8_t token_length = data[0] & 0x0F;
    uint8_t code_class = (data[1] >> 5) & 0x07;
    uint8_t code_detail = data[1] & 0x1F;
    uint16_t message_id = (data[2] << 8) | data[3];
    
    printf("  CoAP Analysis:\n");
    printf("    Version: %d ", version);
    
    // CoAP version should be 1
    if (version != 1) {
        printf("(INVALID - should be 1)\n");
        return 0;
    }
    printf("(Valid)\n");
    
    printf("    Type: %d ", type);
    const char* type_names[] = {"CON", "NON", "ACK", "RST"};
    if (type <= 3) {
        printf("(%s)\n", type_names[type]);
    } else {
        printf("(INVALID)\n");
        return 0;
    }
    
    printf("    Token Length: %d ", token_length);
    if (token_length > 8) {
        printf("(INVALID - max 8 bytes)\n");
        return 0;
    }
    printf("bytes\n");
    
    printf("    Code: %d.%02d ", code_class, code_detail);
    
    // Validate code combinations
    if (code_class == 0) {
        // Request codes
        if (code_detail >= 1 && code_detail <= 4) {
            const char* methods[] = {"", "GET", "POST", "PUT", "DELETE"};
            printf("(%s Request)\n", methods[code_detail]);
        } else if (code_detail == 0) {
            printf("(Empty Message)\n");
        } else {
            printf("(INVALID Request Code)\n");
            return 0;
        }
    } else if (code_class == 2) {
        // Success response codes
        printf("(Success Response)\n");
    } else if (code_class == 4) {
        // Client error codes
        printf("(Client Error)\n");
    } else if (code_class == 5) {
        // Server error codes
        printf("(Server Error)\n");
    } else {
        printf("(INVALID Code Class)\n");
        return 0;
    }
    
    printf("    Message ID: %d\n", message_id);
    
    // Check if we have enough bytes for the token
    if (length < 4 + token_length) {
        printf("    Token: INSUFFICIENT DATA (need %d more bytes)\n", 4 + token_length - (int)length);
        return 0;
    }
    
    printf("    Token: ");
    if (token_length > 0) {
        for (int i = 0; i < token_length; i++) {
            printf("%02X", data[4 + i]);
        }
        printf("\n");
    } else {
        printf("(None)\n");
    }
    
    return 1;  // Looks like valid CoAP
}

// HTTP detection (simple)
int detect_http_detailed(const uint8_t *data, size_t length) {
    if (length < 4) return 0;
    
    printf("  HTTP Analysis:\n");
    
    // Check for HTTP methods
    const char* methods[] = {"GET ", "POST", "PUT ", "DELE", "HEAD", "OPTI", "TRAC", "CONN"};
    const char* method_names[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "TRACE", "CONNECT"};
    
    for (int i = 0; i < 8; i++) {
        if (length >= 4 && memcmp(data, methods[i], 4) == 0) {
            printf("    Method: %s\n", method_names[i]);
            
            // Look for " HTTP/" in the request line
            for (size_t j = 4; j < length - 5; j++) {
                if (memcmp(data + j, " HTTP/", 6) == 0) {
                    printf("    HTTP Version found at offset %zu\n", j);
                    return 1;
                }
            }
            printf("    No HTTP version string found\n");
            return 0;
        }
    }
    
    // Check for HTTP response
    if (length >= 8 && memcmp(data, "HTTP/", 5) == 0) {
        printf("    HTTP Response detected\n");
        return 1;
    }
    
    return 0;
}

// RTP detection
int detect_rtp_detailed(const uint8_t *data, size_t length) {
    if (length < 12) return 0;  // RTP header is minimum 12 bytes
    
    printf("  RTP Analysis:\n");
    
    uint8_t version = (data[0] >> 6) & 0x03;
    uint8_t padding = (data[0] >> 5) & 0x01;
    uint8_t extension = (data[0] >> 4) & 0x01;
    uint8_t cc = data[0] & 0x0F;  // CSRC count
    uint8_t marker = (data[1] >> 7) & 0x01;
    uint8_t payload_type = data[1] & 0x7F;
    
    printf("    Version: %d ", version);
    if (version != 2) {
        printf("(INVALID - should be 2)\n");
        return 0;
    }
    printf("(Valid)\n");
    
    printf("    Padding: %d, Extension: %d, CSRC Count: %d\n", padding, extension, cc);
    printf("    Marker: %d, Payload Type: %d\n", marker, payload_type);
    
    // Check if payload type is reasonable (0-127, common types: 0-34, 96-127)
    if (payload_type > 127) {
        printf("    Payload Type: INVALID\n");
        return 0;
    }
    
    // Check minimum length with CSRC
    size_t min_length = 12 + (cc * 4);
    if (length < min_length) {
        printf("    Length: INSUFFICIENT (need at least %zu bytes)\n", min_length);
        return 0;
    }
    
    return 1;
}

// Main protocol detection function - HEADER ONLY
protocol_type_t detect_protocol_header_only(const uint8_t *data, size_t length) {
    if (data == NULL || length == 0) {
        return PROTOCOL_UNKNOWN;
    }
    
    printf("\n=== Header-Based Protocol Detection ===\n");
    printf("Packet size: %zu bytes\n", length);
    printf("First 16 bytes: ");
    for (size_t i = 0; i < length && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Test each protocol in order of likelihood/specificity
    
    // 1. HTTP (easiest to detect - text-based)
    printf("\nTesting HTTP...\n");
    if (detect_http_detailed(data, length)) {
        return PROTOCOL_HTTP;
    }
    
    // 2. CoAP (has strict version field)
    printf("\nTesting CoAP...\n");
    if (detect_coap_detailed(data, length)) {
        return PROTOCOL_COAP;
    }
    
    // 3. RTP (has version field and specific structure)
    printf("\nTesting RTP...\n");
    if (detect_rtp_detailed(data, length)) {
        return PROTOCOL_RTP;
    }
    
    // 4. MQTT (more complex validation needed)
    printf("\nTesting MQTT...\n");
    if (detect_mqtt_detailed(data, length)) {
        return PROTOCOL_MQTT;
    }
    
    printf("\nNo protocol detected\n");
    return PROTOCOL_UNKNOWN;
}

// Test function with sample packets
void test_protocol_detection() {
    printf("=== Protocol Detection Testing ===\n\n");
    
    // Test MQTT CONNECT packet
    uint8_t mqtt_connect[] = {
        0x10,                    // Message Type: CONNECT, Flags: 0000
        0x2A,                    // Remaining Length: 42 bytes
        0x00, 0x04,             // Protocol Name Length: 4
        'M', 'Q', 'T', 'T',     // Protocol Name: MQTT
        0x04,                    // Protocol Level: 4
        0x02,                    // Connect Flags: Clean Session
        0x00, 0x3C,             // Keep Alive: 60 seconds
        0x00, 0x0C,             // Client ID Length: 12
        't', 'e', 's', 't', 'c', 'l', 'i', 'e', 'n', 't', '0', '1'
    };
    
    printf("1. Testing MQTT CONNECT packet:\n");
    protocol_type_t result = detect_protocol_header_only(mqtt_connect, sizeof(mqtt_connect));
    printf("Result: %s\n\n", result == PROTOCOL_MQTT ? "MQTT" : "Other/Unknown");
    
    // Test CoAP GET request
    uint8_t coap_get[] = {
        0x40,                    // Version: 1, Type: CON, Token Length: 0
        0x01,                    // Code: 0.01 (GET)
        0x12, 0x34,             // Message ID: 0x1234
        0xB3, 'f', 'o', 'o',    // Uri-Path option: "foo"
    };
    
    printf("2. Testing CoAP GET packet:\n");
    result = detect_protocol_header_only(coap_get, sizeof(coap_get));
    printf("Result: %s\n\n", result == PROTOCOL_COAP ? "CoAP" : "Other/Unknown");
    
    // Test HTTP GET request
    uint8_t http_get[] = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
    
    printf("3. Testing HTTP GET packet:\n");
    result = detect_protocol_header_only(http_get, strlen((char*)http_get));
    printf("Result: %s\n\n", result == PROTOCOL_HTTP ? "HTTP" : "Other/Unknown");
    
    // Test RTP packet
    uint8_t rtp_packet[] = {
        0x80,                    // Version: 2, Padding: 0, Extension: 0, CC: 0
        0x60,                    // Marker: 0, Payload Type: 96
        0x00, 0x01,             // Sequence Number: 1
        0x00, 0x00, 0x00, 0x64, // Timestamp: 100
        0x12, 0x34, 0x56, 0x78, // SSRC: 0x12345678
        // Payload would follow...
    };
    
    printf("4. Testing RTP packet:\n");
    result = detect_protocol_header_only(rtp_packet, sizeof(rtp_packet));
    printf("Result: %s\n\n", result == PROTOCOL_RTP ? "RTP" : "Other/Unknown");
    
    // Test unknown/garbage data
    uint8_t garbage[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA};
    
    printf("5. Testing garbage data:\n");
    result = detect_protocol_header_only(garbage, sizeof(garbage));
    printf("Result: %s\n\n", result == PROTOCOL_UNKNOWN ? "Unknown" : "False Positive");
}

int main() {
    test_protocol_detection();
    return 0;
}