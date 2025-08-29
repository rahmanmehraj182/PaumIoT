#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdint.h>

// MQTT Control Packet Types
#define MQTT_CONNECT      0x10
#define MQTT_CONNACK      0x20
#define MQTT_PUBLISH      0x30
#define MQTT_PUBACK       0x40
#define MQTT_PUBREC       0x50
#define MQTT_PUBREL       0x62
#define MQTT_PUBCOMP      0x70
#define MQTT_SUBSCRIBE    0x82
#define MQTT_SUBACK       0x90
#define MQTT_UNSUBSCRIBE  0xA2
#define MQTT_UNSUBACK     0xB0
#define MQTT_PINGREQ      0xC0
#define MQTT_PINGRESP     0xD0
#define MQTT_DISCONNECT   0xE0

// MQTT QoS levels
#define QOS_0  0x00
#define QOS_1  0x01
#define QOS_2  0x02

// Configuration
#define TARGET_IP "127.0.0.1"
#define TARGET_PORT 8080
#define MAX_PACKET_SIZE 1024

typedef struct {
    uint8_t packet_type;
    uint8_t flags;
    uint8_t qos;
    int retain;
    int dup;
    char *client_id;
    char *topic;
    char *payload;
    char *username;
    char *password;
} mqtt_config_t;

// Global packet counter
int packet_count = 0;

// Utility function to encode remaining length
int encode_remaining_length(uint8_t *buffer, int length) {
    int bytes = 0;
    do {
        uint8_t byte = length % 128;
        length = length / 128;
        if (length > 0) {
            byte = byte | 128;
        }
        buffer[bytes++] = byte;
    } while (length > 0);
    return bytes;
}
int create_connect_packet(uint8_t *buffer, mqtt_config_t *config) {
    // Reserve: [0]=fixed header, [1..4]=worst-case RL
    int pos = 1 + 4;
    int body_start = pos;

    // Variable Header: Protocol Name "MQTT"
    buffer[pos++] = 0x00; buffer[pos++] = 0x04;
    memcpy(&buffer[pos], "MQTT", 4); pos += 4;

    // Protocol Level (3.1.1)
    buffer[pos++] = 0x04;

    // Connect Flags
    uint8_t connect_flags = 0x02; // Clean Session
    if (config->username) connect_flags |= 0x80;
    if (config->password) connect_flags |= 0x40;
    buffer[pos++] = connect_flags;

    // Keep Alive = 60
    buffer[pos++] = 0x00; buffer[pos++] = 0x3C;

    // Payload: Client ID
    int client_id_len = (int)strlen(config->client_id);
    buffer[pos++] = (client_id_len >> 8) & 0xFF;
    buffer[pos++] = client_id_len & 0xFF;
    memcpy(&buffer[pos], config->client_id, client_id_len); pos += client_id_len;

    // Username (optional)
    if (config->username) {
        int username_len = (int)strlen(config->username);
        buffer[pos++] = (username_len >> 8) & 0xFF;
        buffer[pos++] = username_len & 0xFF;
        memcpy(&buffer[pos], config->username, username_len); pos += username_len;
    }

    // Password (optional)
    if (config->password) {
        int password_len = (int)strlen(config->password);
        buffer[pos++] = (password_len >> 8) & 0xFF;
        buffer[pos++] = password_len & 0xFF;
        memcpy(&buffer[pos], config->password, password_len); pos += password_len;
    }

    int body_len = pos - body_start;
    uint8_t fixed_header = (uint8_t)(MQTT_CONNECT | (config->flags & 0x0F));
    return finalize_mqtt_packet(buffer, fixed_header, body_start, body_len);
}
int create_publish_packet(uint8_t *buffer, mqtt_config_t *config) {
    // Build fixed header byte (type + flags)
    uint8_t fixed_header = 0x30; // MQTT_PUBLISH base
    if (config->dup) fixed_header |= 0x08;
    fixed_header |= ((config->qos & 0x03) << 1);
    if (config->retain) fixed_header |= 0x01;
    fixed_header |= (config->flags & 0x0F); // allow fuzzing lower nibble if desired

    // Reserve worst-case RL
    int pos = 1 + 4;
    int body_start = pos;

    // Topic Name
    int topic_len = (int)strlen(config->topic);
    buffer[pos++] = (topic_len >> 8) & 0xFF;
    buffer[pos++] = topic_len & 0xFF;
    memcpy(&buffer[pos], config->topic, topic_len); pos += topic_len;

    // Packet Identifier (QoS 1/2)
    if (config->qos > 0) {
        static uint16_t pid = 1; // vary for realism
        buffer[pos++] = (pid >> 8) & 0xFF;
        buffer[pos++] = pid & 0xFF;
        pid = (pid == 0xFFFF) ? 1 : (pid + 1);
    }

    // Payload
    if (config->payload) {
        int payload_len = (int)strlen(config->payload);
        memcpy(&buffer[pos], config->payload, payload_len); pos += payload_len;
    }

    int body_len = pos - body_start;
    return finalize_mqtt_packet(buffer, fixed_header, body_start, body_len);
}
int create_subscribe_packet(uint8_t *buffer, mqtt_config_t *config) {
    // Fixed header must have flags 0x2 for valid SUBSCRIBE;
    // we still allow fuzzing via config->flags if you want invalid cases.
    uint8_t fixed_header = (uint8_t)(MQTT_SUBSCRIBE | (config->flags & 0x0F));

    int pos = 1 + 4;
    int body_start = pos;

    // Packet Identifier
    static uint16_t pid = 1;
    buffer[pos++] = (pid >> 8) & 0xFF;
    buffer[pos++] = pid & 0xFF;
    pid = (pid == 0xFFFF) ? 1 : (pid + 1);

    // Topic Filter
    int topic_len = (int)strlen(config->topic);
    buffer[pos++] = (topic_len >> 8) & 0xFF;
    buffer[pos++] = topic_len & 0xFF;
    memcpy(&buffer[pos], config->topic, topic_len); pos += topic_len;

    // Requested QoS
    buffer[pos++] = (uint8_t)(config->qos & 0x03);

    int body_len = pos - body_start;
    return finalize_mqtt_packet(buffer, fixed_header, body_start, body_len);
}
int create_simple_packet(uint8_t *buffer, uint8_t packet_type_with_flags) {
    // No body → RL = 0
    buffer[0] = packet_type_with_flags;
    buffer[1] = 0x00;
    return 2;
}

int create_connack_packet(uint8_t *buffer, uint8_t return_code) {
    // Build body first at reserved area
    int pos = 1 + 4;
    int body_start = pos;

    buffer[pos++] = 0x00;        // Acknowledge Flags
    buffer[pos++] = return_code; // Return Code

    int body_len = pos - body_start;
    return finalize_mqtt_packet(buffer, 0x20 /* MQTT_CONNACK */, body_start, body_len);
}

// Send packet to target
int send_mqtt_packet(uint8_t *buffer, int packet_len, const char *packet_name) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket for packet %d (%s)\n", packet_count, packet_name);
        return -1;
    }
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TARGET_PORT);
    server_addr.sin_addr.s_addr = inet_addr(TARGET_IP);
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed for packet %d (%s)\n", packet_count, packet_name);
        close(sockfd);
        return -1;
    }
    
    // Send packet
    int bytes_sent = send(sockfd, buffer, packet_len, 0);
    if (bytes_sent > 0) {
        printf("Packet %d: Sent %s (%d bytes)\n", packet_count, packet_name, bytes_sent);
        
        // Print packet hex dump
        printf("  Hex: ");
        for (int i = 0; i < packet_len && i < 32; i++) {
            printf("%02X ", buffer[i]);
        }
        if (packet_len > 32) printf("...");
        printf("\n");
    } else {
        printf("Failed to send packet %d (%s)\n", packet_count, packet_name);
    }
    
    close(sockfd);
    usleep(100000); // 100ms delay between packets
    return bytes_sent;
}

// Generate all possible MQTT packet combinations
void generate_mqtt_combinations() {
    uint8_t buffer[MAX_PACKET_SIZE];
    mqtt_config_t config;
    char client_id[32], topic[64], payload[128], username[32], password[32];
    
    // Client ID variations
    const char* client_ids[] = {"client1", "test_client", "iot_device", "sensor01"};
    
    // Topic variations
    const char* topics[] = {"home/temperature", "device/status", "sensor/data", "test/topic"};
    
    // Payload variations
    const char* payloads[] = {"hello", "{\"temp\":25}", "test_data", "status:ok"};
    
    // Username/Password variations
    const char* usernames[] = {NULL, "testuser", "admin"};
    const char* passwords[] = {NULL, "testpass", "secret123"};
    
    printf("=== Starting MQTT Packet Generation ===\n");
    printf("Target: %s:%d\n\n", TARGET_IP, TARGET_PORT);
    
    // 1. CONNECT packets with different combinations
    printf("--- CONNECT Packets ---\n");
    for (int c = 0; c < 4 && packet_count < 25; c++) {
        for (int u = 0; u < 3 && packet_count < 25; u++) {
            for (int p = 0; p < 3 && packet_count < 25; p++) {
                if ((usernames[u] == NULL && passwords[p] != NULL) ||
                    (usernames[u] != NULL && passwords[p] == NULL)) continue;
                
                memset(&config, 0, sizeof(config));
                config.packet_type = MQTT_CONNECT;
                config.client_id = (char*)client_ids[c];
                config.username = (char*)usernames[u];
                config.password = (char*)passwords[p];
                
                int len = create_connect_packet(buffer, &config);
                sprintf(client_id, "CONNECT_%s_%s", client_ids[c], 
                       usernames[u] ? usernames[u] : "noauth");
                send_mqtt_packet(buffer, len, client_id);
                packet_count++;
            }
        }
    }
    
    // 2. PUBLISH packets with QoS, DUP, RETAIN combinations
    printf("\n--- PUBLISH Packets ---\n");
    for (int t = 0; t < 4 && packet_count < 50; t++) {
        for (int pl = 0; pl < 4 && packet_count < 50; pl++) {
            for (int qos = 0; qos <= 2 && packet_count < 50; qos++) {
                for (int dup = 0; dup <= 1 && packet_count < 50; dup++) {
                    for (int retain = 0; retain <= 1 && packet_count < 50; retain++) {
                        memset(&config, 0, sizeof(config));
                        config.packet_type = MQTT_PUBLISH;
                        config.topic = (char*)topics[t];
                        config.payload = (char*)payloads[pl];
                        config.qos = qos;
                        config.dup = dup;
                        config.retain = retain;
                        
                        int len = create_publish_packet(buffer, &config);
                        sprintf(topic, "PUBLISH_%s_QoS%d_DUP%d_RET%d", 
                               topics[t], qos, dup, retain);
                        send_mqtt_packet(buffer, len, topic);
                        packet_count++;
                    }
                }
            }
        }
    }
    
    // 3. SUBSCRIBE packets with different topics and QoS
    printf("\n--- SUBSCRIBE Packets ---\n");
    for (int t = 0; t < 4 && packet_count < 70; t++) {
        for (int qos = 0; qos <= 2 && packet_count < 70; qos++) {
            for (int flag = 0; flag <= 15 && packet_count < 70; flag++) {
                memset(&config, 0, sizeof(config));
                config.packet_type = MQTT_SUBSCRIBE;
                config.topic = (char*)topics[t];
                config.qos = qos;
                config.flags = flag & 0x0F;
                
                int len = create_subscribe_packet(buffer, &config);
                sprintf(topic, "SUBSCRIBE_%s_QoS%d_F%d", topics[t], qos, flag);
                send_mqtt_packet(buffer, len, topic);
                packet_count++;
            }
        }
    }
    
    // 4. Control packets (PINGREQ, PINGRESP, DISCONNECT, etc.)
    printf("\n--- Control Packets ---\n");
    uint8_t control_packets[] = {
        MQTT_PINGREQ, MQTT_PINGRESP, MQTT_DISCONNECT,
        MQTT_PUBACK, MQTT_PUBREC, MQTT_PUBREL, MQTT_PUBCOMP
    };
    const char* control_names[] = {
        "PINGREQ", "PINGRESP", "DISCONNECT",
        "PUBACK", "PUBREC", "PUBREL", "PUBCOMP"
    };
    
    for (int i = 0; i < 7 && packet_count < 85; i++) {
        for (int flag = 0; flag <= 15 && packet_count < 85; flag++) {
            uint8_t packet_type = control_packets[i];
            if (i == 5) packet_type |= 0x02; // PUBREL requires flags
            
            int len = create_simple_packet(buffer, packet_type | (flag & 0x0F));
            sprintf(topic, "%s_F%d", control_names[i], flag);
            send_mqtt_packet(buffer, len, topic);
            packet_count++;
        }
    }
    
    // 5. CONNACK packets with different return codes
    printf("\n--- CONNACK Packets ---\n");
    uint8_t return_codes[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    const char* return_code_names[] = {
        "ACCEPTED", "UNACCEPTABLE_PROTOCOL", "IDENTIFIER_REJECTED",
        "SERVER_UNAVAILABLE", "BAD_USERNAME_PASSWORD", "NOT_AUTHORIZED"
    };
    
    for (int i = 0; i < 6 && packet_count < 95; i++) {
        for (int flag = 0; flag <= 15 && packet_count < 95; flag++) {
            int len = create_connack_packet(buffer, return_codes[i]);
            buffer[0] |= (flag & 0x0F); // Add flag variations
            sprintf(topic, "CONNACK_%s_F%d", return_code_names[i], flag);
            send_mqtt_packet(buffer, len, topic);
            packet_count++;
        }
    }
    
    // 6. Malformed packets for edge case testing
    printf("\n--- Malformed/Edge Case Packets ---\n");
    while (packet_count < 100) {
        memset(buffer, 0, MAX_PACKET_SIZE);
        
        // Create various malformed packets
        switch (packet_count % 5) {
            case 0: // Invalid packet type
                buffer[0] = 0xF0 | (packet_count & 0x0F);
                buffer[1] = 0x02;
                buffer[2] = 0xAA;
                buffer[3] = 0xBB;
                sprintf(topic, "INVALID_TYPE_0x%02X", buffer[0]);
                send_mqtt_packet(buffer, 4, topic);
                break;
                
            case 1: // Zero length packet
                buffer[0] = MQTT_PUBLISH;
                sprintf(topic, "ZERO_LENGTH");
                send_mqtt_packet(buffer, 1, topic);
                break;
                
            case 2: // Oversized remaining length
                buffer[0] = MQTT_CONNECT;
                buffer[1] = 0xFF;
                buffer[2] = 0xFF;
                buffer[3] = 0xFF;
                buffer[4] = 0x7F;
                sprintf(topic, "OVERSIZED_REMAINING_LENGTH");
                send_mqtt_packet(buffer, 5, topic);
                break;
                
            case 3: // Truncated packet
                buffer[0] = MQTT_PUBLISH;
                buffer[1] = 0x10; // Claims 16 bytes but we'll send less
                buffer[2] = 0x00;
                buffer[3] = 0x04;
                memcpy(&buffer[4], "test", 4);
                sprintf(topic, "TRUNCATED_PACKET");
                send_mqtt_packet(buffer, 6, topic); // Send less than claimed
                break;
                
            case 4: // Random data
                for (int i = 0; i < 16; i++) {
                    buffer[i] = rand() & 0xFF;
                }
                sprintf(topic, "RANDOM_DATA_%d", packet_count);
                send_mqtt_packet(buffer, 16, topic);
                break;
        }
        packet_count++;
    }
}

// Generate protocol detection test packets
void generate_protocol_confusion_packets() {
    uint8_t buffer[MAX_PACKET_SIZE];
    
    printf("\n=== Protocol Confusion Test Packets ===\n");
    
    // HTTP-like packet that might confuse detection
    const char* http_like = "GET /mqtt/connect HTTP/1.1\r\nHost: localhost\r\n\r\n";
    strcpy((char*)buffer, http_like);
    send_mqtt_packet(buffer, strlen(http_like), "HTTP_LIKE_MQTT");
    
    // CoAP-like packet
    buffer[0] = 0x40; // CoAP version 1, Confirmable
    buffer[1] = 0x01; // GET
    buffer[2] = 0x00; // Message ID high
    buffer[3] = 0x01; // Message ID low
    send_mqtt_packet(buffer, 4, "COAP_LIKE_MQTT");
    
    // DNS-like packet
    buffer[0] = 0x12; // Transaction ID high
    buffer[1] = 0x34; // Transaction ID low
    buffer[2] = 0x01; // Flags
    buffer[3] = 0x00; // Flags
    send_mqtt_packet(buffer, 4, "DNS_LIKE_MQTT");
    
    printf("Protocol confusion test packets sent.\n");
}

// --- helpers ---
// encode_remaining_length(...) stays the same

// Finalize a packet after writing its body starting at body_start.
// Places fixed header, encodes Remaining Length, and shifts body correctly.
static int finalize_mqtt_packet(uint8_t *buffer, uint8_t fixed_header,
                                int body_start, int body_len) {
    // Encode RL into a temp (max 4 bytes)
    uint8_t rl_tmp[4];
    int rl_bytes = encode_remaining_length(rl_tmp, body_len);

    // Move body so it starts right after fixed header + RL
    // Current body is at buffer[body_start .. body_start+body_len)
    // Target is buffer[1 + rl_bytes .. 1 + rl_bytes + body_len)
    memmove(buffer + 1 + rl_bytes, buffer + body_start, body_len);

    // Write fixed header + RL
    buffer[0] = fixed_header;
    memcpy(buffer + 1, rl_tmp, rl_bytes);

    // Total length
    return 1 + rl_bytes + body_len;
}


// Main function
int main(int argc, char *argv[]) {
    printf("MQTT Packet Generator for PaumIoT Testing\n");
    printf("==========================================\n");
    printf("This tool generates 100+ MQTT packets with various combinations\n");
    printf("to test your protocol detection middleware.\n\n");
    
    if (argc > 1) {
        printf("Target IP: %s (from command line)\n", argv[1]);
        // Note: You can modify TARGET_IP if needed for command line args
    }
    
    srand(time(NULL));
    
    // Generate main MQTT packet combinations
    generate_mqtt_combinations();
    
    // Generate additional protocol confusion packets
    generate_protocol_confusion_packets();
    
    printf("\n=== Summary ===\n");
    printf("Total packets sent: %d\n", packet_count);
    printf("Target server: %s:%d\n", TARGET_IP, TARGET_PORT);
    printf("\nPacket types included:\n");
    printf("- CONNECT packets (with auth variations)\n");
    printf("- PUBLISH packets (QoS 0/1/2, DUP, RETAIN flags)\n");
    printf("- SUBSCRIBE packets (different topics and QoS)\n");
    printf("- Control packets (PINGREQ, PINGRESP, DISCONNECT, etc.)\n");
    printf("- CONNACK packets (various return codes)\n");
    printf("- Malformed packets (edge cases)\n");
    printf("- Protocol confusion packets\n");
    printf("\nThis should thoroughly test your PaumIoT middleware's\n");
    printf("MQTT protocol detection capabilities!\n");
    
    return 0;
}

// Compilation instructions in comments:
/*
To compile and run:

gcc -o mqtt_test_generator mqtt_packet_generator.c -Wall -Wextra

./mqtt_test_generator

Optional: Pass target IP as argument:
./mqtt_test_generator 192.168.1.100

Features of this generator:
1. Creates exactly 100+ MQTT packets with different combinations
2. Tests all major MQTT packet types (CONNECT, PUBLISH, SUBSCRIBE, etc.)
3. Varies QoS levels (0, 1, 2)
4. Tests DUP and RETAIN flags
5. Includes authentication scenarios (with/without username/password)
6. Creates malformed packets for edge case testing
7. Generates protocol confusion packets to test detection accuracy
8. Provides hex dumps for debugging
9. Includes proper MQTT protocol structure
10. Sequential packet sending with delays to avoid overwhelming

The packets are designed to thoroughly test your middleware's ability to:
- Detect MQTT protocol correctly
- Handle various MQTT packet types
- Deal with malformed/invalid packets
- Distinguish MQTT from other protocols (HTTP, CoAP, DNS)

Each packet follows proper MQTT v3.1.1 specification where applicable,
with intentional variations and edge cases for comprehensive testing.
*/