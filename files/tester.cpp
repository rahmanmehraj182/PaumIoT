#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

// MQTT Message Types
typedef enum {
    MQTT_RESERVED_0    = 0,   // Reserved
    MQTT_CONNECT       = 1,   // Client request to connect to Server
    MQTT_CONNACK       = 2,   // Connect acknowledgment
    MQTT_PUBLISH       = 3,   // Publish message
    MQTT_PUBACK        = 4,   // Publish acknowledgment
    MQTT_PUBREC        = 5,   // Publish received (assured delivery part 1)
    MQTT_PUBREL        = 6,   // Publish release (assured delivery part 2)
    MQTT_PUBCOMP       = 7,   // Publish complete (assured delivery part 3)
    MQTT_SUBSCRIBE     = 8,   // Client subscribe request
    MQTT_SUBACK        = 9,   // Subscribe acknowledgment
    MQTT_UNSUBSCRIBE   = 10,  // Client unsubscribe request
    MQTT_UNSUBACK      = 11,  // Unsubscribe acknowledgment
    MQTT_PINGREQ       = 12,  // PING request
    MQTT_PINGRESP      = 13,  // PING response
    MQTT_DISCONNECT    = 14,  // Client is disconnecting
    MQTT_RESERVED_15   = 15   // Reserved
} mqtt_message_type_t;

// CoAP Message Types
typedef enum {
    COAP_TYPE_CON = 0,  // Confirmable
    COAP_TYPE_NON = 1,  // Non-confirmable
    COAP_TYPE_ACK = 2,  // Acknowledgement
    COAP_TYPE_RST = 3   // Reset
} coap_message_type_t;

// CoAP Method/Response Codes
typedef enum {
    // Methods (0.XX)
    COAP_METHOD_EMPTY = 0,
    COAP_METHOD_GET   = 1,
    COAP_METHOD_POST  = 2,
    COAP_METHOD_PUT   = 3,
    COAP_METHOD_DELETE = 4,
    
    // Success Response (2.XX)
    COAP_RESPONSE_CREATED = 65,    // 2.01
    COAP_RESPONSE_DELETED = 66,    // 2.02
    COAP_RESPONSE_VALID   = 67,    // 2.03
    COAP_RESPONSE_CHANGED = 68,    // 2.04
    COAP_RESPONSE_CONTENT = 69,    // 2.05
    
    // Client Error (4.XX)
    COAP_ERROR_BAD_REQUEST = 128,  // 4.00
    COAP_ERROR_UNAUTHORIZED = 129, // 4.01
    COAP_ERROR_BAD_OPTION = 130,   // 4.02
    COAP_ERROR_FORBIDDEN = 131,    // 4.03
    COAP_ERROR_NOT_FOUND = 132,    // 4.04
    
    // Server Error (5.XX)
    COAP_ERROR_INTERNAL_SERVER_ERROR = 160, // 5.00
    COAP_ERROR_NOT_IMPLEMENTED = 161,       // 5.01
    COAP_ERROR_BAD_GATEWAY = 162,           // 5.02
    COAP_ERROR_SERVICE_UNAVAILABLE = 163,   // 5.03
    COAP_ERROR_GATEWAY_TIMEOUT = 164        // 5.04
} coap_code_t;

// MQTT Fixed Header Structure
typedef struct __attribute__((packed)) {
    uint8_t byte1;          // Message Type + Flags
    uint8_t remaining_length; // Remaining Length (simplified - can be 1-4 bytes)
} mqtt_fixed_header_t;

// MQTT Fixed Header Bit Layout
#define MQTT_MESSAGE_TYPE_MASK   0xF0  // Bits 7-4
#define MQTT_MESSAGE_TYPE_SHIFT  4
#define MQTT_DUP_FLAG           0x08   // Bit 3
#define MQTT_QOS_MASK           0x06   // Bits 2-1
#define MQTT_QOS_SHIFT          1
#define MQTT_RETAIN_FLAG        0x01   // Bit 0

// CoAP Header Structure (4 bytes fixed)
typedef struct __attribute__((packed)) {
    uint8_t ver_type_tkl;   // Version (2) + Type (2) + Token Length (4)
    uint8_t code;           // Method/Response Code
    uint16_t message_id;    // Message ID (network byte order)
} coap_header_t;

// CoAP Header Bit Layout
#define COAP_VERSION_MASK       0xC0  // Bits 7-6
#define COAP_VERSION_SHIFT      6
#define COAP_TYPE_MASK          0x30  // Bits 5-4
#define COAP_TYPE_SHIFT         4
#define COAP_TOKEN_LENGTH_MASK  0x0F  // Bits 3-0

#define COAP_VERSION_1          1

// Protocol Detection Functions
typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_MQTT = 1,
    PROTOCOL_COAP = 2,
    PROTOCOL_INVALID = -1
} protocol_type_t;

// Enhanced MQTT Detection
int detect_mqtt_detailed(const uint8_t* data, size_t len) {
    if (len < 2) return 0; // Need at least 2 bytes for MQTT
    
    mqtt_fixed_header_t* header = (mqtt_fixed_header_t*)data;
    
    // Extract message type
    uint8_t msg_type = (header->byte1 & MQTT_MESSAGE_TYPE_MASK) >> MQTT_MESSAGE_TYPE_SHIFT;
    
    printf("[MQTT DETECT] Message Type: %u (%s)\n", msg_type, 
           (msg_type == MQTT_CONNECT) ? "CONNECT" :
           (msg_type == MQTT_CONNACK) ? "CONNACK" :
           (msg_type == MQTT_PUBLISH) ? "PUBLISH" :
           (msg_type == MQTT_PUBACK) ? "PUBACK" :
           (msg_type == MQTT_SUBSCRIBE) ? "SUBSCRIBE" :
           (msg_type == MQTT_PINGREQ) ? "PINGREQ" :
           (msg_type == MQTT_PINGRESP) ? "PINGRESP" :
           (msg_type == MQTT_DISCONNECT) ? "DISCONNECT" : "OTHER");
    
    // Validate message type (1-14 are valid)
    if (msg_type < 1 || msg_type > 14) {
        printf("[MQTT DETECT] Invalid message type: %u\n", msg_type);
        return 0;
    }
    
    // Check QoS level (0-2 are valid)
    uint8_t qos = (header->byte1 & MQTT_QOS_MASK) >> MQTT_QOS_SHIFT;
    if (qos > 2) {
        printf("[MQTT DETECT] Invalid QoS: %u\n", qos);
        return 0;
    }
    
    // Validate message type specific flags
    switch (msg_type) {
        case MQTT_CONNECT:
        case MQTT_CONNACK:
        case MQTT_SUBSCRIBE:
        case MQTT_SUBACK:
        case MQTT_UNSUBSCRIBE:
        case MQTT_UNSUBACK:
        case MQTT_PINGREQ:
        case MQTT_PINGRESP:
        case MQTT_DISCONNECT:
            // These message types should have specific flag patterns
            if (header->byte1 & 0x0F) { // Reserved bits should be 0
                printf("[MQTT DETECT] Invalid flags for message type %u: 0x%02X\n", 
                       msg_type, header->byte1 & 0x0F);
                return 0;
            }
            break;
        case MQTT_PUBLISH:
            // PUBLISH can have DUP, QoS, and RETAIN flags
            break;
        default:
            // Other message types have specific flag requirements
            break;
    }
    
    // Parse remaining length (MQTT variable length encoding)
    size_t remaining_length = 0;
    size_t multiplier = 1;
    size_t pos = 1;
    
    do {
        if (pos >= len) break;
        uint8_t byte = data[pos];
        remaining_length += (byte & 0x7F) * multiplier;
        multiplier *= 128;
        pos++;
    } while ((data[pos-1] & 0x80) && pos < len && pos < 5); // Max 4 bytes for length
    
    printf("[MQTT DETECT] Remaining length: %zu bytes\n", remaining_length);
    
    // For CONNECT messages, check protocol name
    if (msg_type == MQTT_CONNECT && len > pos + 6) {
        uint16_t proto_name_len = ntohs(*(uint16_t*)(data + pos));
        if (proto_name_len == 4 && pos + 2 + proto_name_len < len) {
            if (memcmp(data + pos + 2, "MQTT", 4) == 0) {
                uint8_t protocol_level = data[pos + 6];
                printf("[MQTT DETECT] CONNECT: Protocol=MQTT, Level=%u\n", protocol_level);
                return 1;
            }
        }
    }
    
    return 1; // Likely MQTT
}

// Enhanced CoAP Detection
int detect_coap_detailed(const uint8_t* data, size_t len) {
    if (len < 4) return 0; // CoAP header is always 4 bytes minimum
    
    coap_header_t* header = (coap_header_t*)data;
    
    // Extract header fields
    uint8_t version = (header->ver_type_tkl & COAP_VERSION_MASK) >> COAP_VERSION_SHIFT;
    uint8_t type = (header->ver_type_tkl & COAP_TYPE_MASK) >> COAP_TYPE_SHIFT;
    uint8_t token_len = header->ver_type_tkl & COAP_TOKEN_LENGTH_MASK;
    uint8_t code = header->code;
    uint16_t message_id = ntohs(header->message_id);
    
    printf("[COAP DETECT] Version: %u, Type: %u (%s), Token Len: %u, Code: %u, MsgID: %u\n",
           version, type,
           (type == COAP_TYPE_CON) ? "CON" :
           (type == COAP_TYPE_NON) ? "NON" :
           (type == COAP_TYPE_ACK) ? "ACK" :
           (type == COAP_TYPE_RST) ? "RST" : "UNKNOWN",
           token_len, code, message_id);
    
    // Validate CoAP version (must be 1)
    if (version != COAP_VERSION_1) {
        printf("[COAP DETECT] Invalid version: %u (expected 1)\n", version);
        return 0;
    }
    
    // Validate message type (0-3)
    if (type > 3) {
        printf("[COAP DETECT] Invalid type: %u\n", type);
        return 0;
    }
    
    // Validate token length (0-8)
    if (token_len > 8) {
        printf("[COAP DETECT] Invalid token length: %u\n", token_len);
        return 0;
    }
    
    // Check if we have enough data for token
    if (len < 4 + token_len) {
        printf("[COAP DETECT] Insufficient data for token (need %u, have %zu)\n", 
               4 + token_len, len);
        return 0;
    }
    
    // Validate code based on message type
    if (type == COAP_TYPE_RST && code != 0) {
        printf("[COAP DETECT] RST message should have code 0, got %u\n", code);
        return 0;
    }
    
    // Decode and validate code
    uint8_t code_class = code >> 5;
    uint8_t code_detail = code & 0x1F;
    
    printf("[COAP DETECT] Code Class: %u, Detail: %u\n", code_class, code_detail);
    
    // Validate code class (0-7)
    if (code_class > 7) {
        printf("[COAP DETECT] Invalid code class: %u\n", code_class);
        return 0;
    }
    
    // Check for valid method/response codes
    switch (code_class) {
        case 0: // Methods or Empty
            if (code_detail > 31) return 0;
            break;
        case 2: // Success Response
            if (code_detail > 31) return 0;
            break;
        case 4: // Client Error
            if (code_detail > 31) return 0;
            break;
        case 5: // Server Error  
            if (code_detail > 31) return 0;
            break;
        case 1: case 3: case 6: case 7: // Reserved
            printf("[COAP DETECT] Reserved code class: %u\n", code_class);
            return 0;
    }
    
    return 1; // Likely CoAP
}

// Combined Protocol Detection with Detailed Analysis
protocol_type_t detect_protocol_enhanced(const uint8_t* data, size_t len) {
    if (len < 2) return PROTOCOL_UNKNOWN;
    
    printf("\n[PROTOCOL DETECT] Analyzing %zu bytes: ", len);
    for (size_t i = 0; i < (len > 8 ? 8 : len); i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Try CoAP first (more restrictive header)
    if (detect_coap_detailed(data, len)) {
        printf("[PROTOCOL DETECT] Detected: CoAP\n");
        return PROTOCOL_COAP;
    }
    
    // Try MQTT
    if (detect_mqtt_detailed(data, len)) {
        printf("[PROTOCOL DETECT] Detected: MQTT\n");
        return PROTOCOL_MQTT;
    }
    
    printf("[PROTOCOL DETECT] No protocol detected\n");
    return PROTOCOL_UNKNOWN;
}

// Generate sample protocol messages for testing
void generate_mqtt_samples(void) {
    printf("\n=== MQTT Sample Messages ===\n");
    
    // MQTT CONNECT message
    uint8_t mqtt_connect[] = {
        0x10, 0x12,                    // Fixed header: CONNECT, remaining length = 18
        0x00, 0x04, 'M', 'Q', 'T', 'T', // Protocol name length + "MQTT"
        0x04,                          // Protocol level 4 (MQTT 3.1.1)
        0x02,                          // Connect flags (Clean Session)
        0x00, 0x3C,                    // Keep Alive = 60 seconds
        0x00, 0x04, 't', 'e', 's', 't' // Client ID length + "test"
    };
    printf("MQTT CONNECT: ");
    detect_protocol_enhanced(mqtt_connect, sizeof(mqtt_connect));
    
    // MQTT PUBLISH message
    uint8_t mqtt_publish[] = {
        0x32, 0x0F,                    // Fixed header: PUBLISH, DUP=0, QoS=1, RETAIN=0
        0x00, 0x05, 't', 'o', 'p', 'i', 'c', // Topic name length + "topic"
        0x12, 0x34,                    // Packet identifier
        'h', 'e', 'l', 'l', 'o'        // Payload "hello"
    };
    printf("MQTT PUBLISH: ");
    detect_protocol_enhanced(mqtt_publish, sizeof(mqtt_publish));
    
    // MQTT PINGREQ message
    uint8_t mqtt_pingreq[] = {0xC0, 0x00}; // Fixed header only
    printf("MQTT PINGREQ: ");
    detect_protocol_enhanced(mqtt_pingreq, sizeof(mqtt_pingreq));
}

void generate_coap_samples(void) {
    printf("\n=== CoAP Sample Messages ===\n");
    
    // CoAP GET request
    uint8_t coap_get[] = {
        0x44,           // Version=1, Type=CON, Token Length=4
        0x01,           // Code=0.01 (GET)
        0x12, 0x34,     // Message ID
        0xAB, 0xCD, 0xEF, 0x01, // Token (4 bytes)
        0xB3, 'f', 'o', 'o',    // Uri-Path option "foo"
        0xFF,           // Payload marker
        'H', 'e', 'l', 'l', 'o' // Payload
    };
    printf("CoAP GET: ");
    detect_protocol_enhanced(coap_get, sizeof(coap_get));
    
    // CoAP ACK response
    uint8_t coap_ack[] = {
        0x62,           // Version=1, Type=ACK, Token Length=2
        0x45,           // Code=2.05 (Content)
        0x12, 0x34,     // Message ID (same as request)
        0xAB, 0xCD,     // Token (2 bytes)
        0xFF,           // Payload marker
        'W', 'o', 'r', 'l', 'd'  // Payload "World"
    };
    printf("CoAP ACK: ");
    detect_protocol_enhanced(coap_ack, sizeof(coap_ack));
    
    // CoAP POST request
    uint8_t coap_post[] = {
        0x50,           // Version=1, Type=NON, Token Length=0
        0x02,           // Code=0.02 (POST)
        0x56, 0x78,     // Message ID
        0xC1, 0x00,     // Content-Format: text/plain (option)
        0xFF,           // Payload marker
        'D', 'a', 't', 'a' // Payload
    };
    printf("CoAP POST: ");
    detect_protocol_enhanced(coap_post, sizeof(coap_post));
}

// Test with ambiguous data
void test_edge_cases(void) {
    printf("\n=== Edge Cases ===\n");
    
    // Invalid MQTT (bad message type)
    uint8_t invalid_mqtt[] = {0x00, 0x02, 0x12, 0x34}; // Message type 0 (reserved)
    printf("Invalid MQTT: ");
    detect_protocol_enhanced(invalid_mqtt, sizeof(invalid_mqtt));
    
    // Invalid CoAP (bad version)
    uint8_t invalid_coap[] = {0x80, 0x01, 0x12, 0x34}; // Version 2 (invalid)
    printf("Invalid CoAP: ");
    detect_protocol_enhanced(invalid_coap, sizeof(invalid_coap));
    
    // Too short data
    uint8_t short_data[] = {0x10};
    printf("Short data: ");
    detect_protocol_enhanced(short_data, sizeof(short_data));
    
    // Random data
    uint8_t random_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    printf("Random data: ");
    detect_protocol_enhanced(random_data, sizeof(random_data));
}

int main() {
    printf("MQTT vs CoAP Protocol Header Analysis\n");
    printf("=====================================\n");
    
    printf("Key Differences:\n");
    printf("1. MQTT: Variable header length, message type in upper 4 bits of first byte\n");
    printf("2. CoAP: Fixed 4-byte header, version in upper 2 bits of first byte\n");
    printf("3. MQTT: Uses remaining length encoding (1-4 bytes)\n");
    printf("4. CoAP: Uses fixed header + options + payload\n");
    printf("5. MQTT: TCP-based, connection-oriented\n");
    printf("6. CoAP: UDP-based, request/response model\n\n");
    
    generate_mqtt_samples();
    generate_coap_samples();
    test_edge_cases();
    
    return 0;
}