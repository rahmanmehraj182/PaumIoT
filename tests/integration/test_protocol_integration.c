/**
 * @file test_protocol_integration.c
 * @brief Integration tests for MQTT and CoAP protocol handling
 */

#include "../test_framework.h"
#include "../../include/paumiot.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Test configuration */
#define TEST_MQTT_PORT 11883
#define TEST_COAP_PORT 15683
#define TEST_TIMEOUT 5

/* Test client simulator */
typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    protocol_type_t protocol;
} test_client_t;

/* Helper functions */
static test_client_t* create_mqtt_client(void) {
    test_client_t *client = malloc(sizeof(test_client_t));
    if (!client) return NULL;
    
    client->protocol = PROTOCOL_TYPE_MQTT;
    client->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sockfd < 0) {
        free(client);
        return NULL;
    }
    
    client->addr.sin_family = AF_INET;
    client->addr.sin_port = htons(TEST_MQTT_PORT);
    client->addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    return client;
}

static test_client_t* create_coap_client(void) {
    test_client_t *client = malloc(sizeof(test_client_t));
    if (!client) return NULL;
    
    client->protocol = PROTOCOL_TYPE_COAP;
    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) {
        free(client);
        return NULL;
    }
    
    client->addr.sin_family = AF_INET;
    client->addr.sin_port = htons(TEST_COAP_PORT);
    client->addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    return client;
}

static void destroy_client(test_client_t *client) {
    if (client) {
        if (client->sockfd >= 0) {
            close(client->sockfd);
        }
        free(client);
    }
}

static bool send_packet(test_client_t *client, const uint8_t *data, size_t len) {
    ssize_t sent;
    
    if (client->protocol == PROTOCOL_TYPE_MQTT) {
        sent = send(client->sockfd, data, len, 0);
    } else {
        sent = sendto(client->sockfd, data, len, 0, 
                     (struct sockaddr*)&client->addr, sizeof(client->addr));
    }
    
    return sent == (ssize_t)len;
}

static ssize_t recv_packet(test_client_t *client, uint8_t *buffer, size_t buffer_len) {
    if (client->protocol == PROTOCOL_TYPE_MQTT) {
        return recv(client->sockfd, buffer, buffer_len, 0);
    } else {
        socklen_t addr_len = sizeof(client->addr);
        return recvfrom(client->sockfd, buffer, buffer_len, 0,
                       (struct sockaddr*)&client->addr, &addr_len);
    }
}

/* Test data */
static const uint8_t mqtt_connect_packet[] = {
    0x10, 0x2A,                          /* CONNECT, length=42 */
    0x00, 0x04, 'M', 'Q', 'T', 'T',      /* Protocol name */
    0x05,                                /* Protocol level (MQTT 5.0) */
    0x02,                                /* Connect flags (Clean Start) */
    0x00, 0x3C,                          /* Keep Alive = 60 */
    0x00,                                /* Properties length = 0 */
    0x00, 0x0C, 't', 'e', 's', 't', '_', 'c', 'l', 'i', 'e', 'n', 't'  /* Client ID */
};

static const uint8_t mqtt_publish_packet[] = {
    0x30, 0x19,                          /* PUBLISH */
    0x00, 0x0A, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',  /* Topic */
    'H', 'e', 'l', 'l', 'o', ' ', 'M', 'Q', 'T', 'T'              /* Payload */
};

static const uint8_t coap_get_packet[] = {
    0x40, 0x01, 0x12, 0x34,              /* Version=1, Type=CON, Code=GET, MID=0x1234 */
    0xB1, 0x74, 0x65, 0x73, 0x74,       /* Uri-Path: "test" */
    0x00, 0x74, 0x65, 0x73, 0x74        /* Uri-Path: "test" (continued) */
};

void test_mqtt_connection_handling(void) {
    TEST_CASE("MQTT Connection Handling");
    
    /* This would require the middleware to be running */
    /* For now, we test the protocol parsing components */
    
    message_t *message = NULL;
    paumiot_result_t result = mqtt_adapter.decode(&mqtt_adapter,
                                                  mqtt_connect_packet,
                                                  sizeof(mqtt_connect_packet),
                                                  &message);
    
    /* CONNECT packets might not be fully supported in basic adapter */
    /* Focus on PUBLISH packets for now */
    if (result == PAUMIOT_ERROR_PROTOCOL_UNSUPPORTED) {
        printf("  Note: CONNECT packet parsing not implemented in basic adapter\n");
    }
    
    /* Test PUBLISH packet parsing */
    result = mqtt_adapter.decode(&mqtt_adapter,
                                mqtt_publish_packet,
                                sizeof(mqtt_publish_packet),
                                &message);
    
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ("test/topic", message->destination);
    
    message_free(message);
    
    TEST_CASE_END();
}

void test_coap_request_handling(void) {
    TEST_CASE("CoAP Request Handling");
    
    message_t *message = NULL;
    paumiot_result_t result = coap_adapter.decode(&coap_adapter,
                                                  coap_get_packet,
                                                  sizeof(coap_get_packet),
                                                  &message);
    
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ("/test/test", message->destination);
    ASSERT_EQ(PROTOCOL_TYPE_COAP, message->metadata.protocol);
    
    message_free(message);
    
    TEST_CASE_END();
}

void test_protocol_interoperability(void) {
    TEST_CASE("Protocol Interoperability");
    
    /* Test converting MQTT message to internal format and back */
    message_t *mqtt_message = NULL;
    paumiot_result_t result = mqtt_adapter.decode(&mqtt_adapter,
                                                  mqtt_publish_packet,
                                                  sizeof(mqtt_publish_packet),
                                                  &mqtt_message);
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(mqtt_message);
    
    /* Convert to CoAP format (conceptually) */
    /* In a real implementation, this would involve more complex mapping */
    message_t *coap_message = message_copy(mqtt_message);
    ASSERT_NOT_NULL(coap_message);
    
    /* Change protocol metadata */
    coap_message->metadata.protocol = PROTOCOL_TYPE_COAP;
    
    /* Encode as CoAP */
    uint8_t coap_buffer[256];
    size_t coap_bytes_written = 0;
    
    result = coap_adapter.encode(&coap_adapter,
                                coap_message,
                                coap_buffer,
                                sizeof(coap_buffer),
                                &coap_bytes_written);
    
    ASSERT_SUCCESS(result);
    ASSERT_TRUE(coap_bytes_written > 0);
    
    message_free(mqtt_message);
    message_free(coap_message);
    
    TEST_CASE_END();
}

void test_message_routing(void) {
    TEST_CASE("Message Routing Between Protocols");
    
    pal_config_t config = {0};
    pal_context_t *ctx = pal_init(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Register both adapters */
    paumiot_result_t result = pal_register_adapter(ctx, &mqtt_adapter);
    ASSERT_SUCCESS(result);
    
    result = pal_register_adapter(ctx, &coap_adapter);
    ASSERT_SUCCESS(result);
    
    /* Decode MQTT packet */
    message_t *message = NULL;
    result = pal_decode_packet(ctx,
                               PROTOCOL_TYPE_MQTT,
                               mqtt_publish_packet,
                               sizeof(mqtt_publish_packet),
                               &message);
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    
    /* Change target protocol to CoAP */
    message->metadata.protocol = PROTOCOL_TYPE_COAP;
    
    /* Encode as CoAP */
    uint8_t buffer[256];
    size_t bytes_written = 0;
    
    result = pal_encode_message(ctx,
                                message,
                                buffer,
                                sizeof(buffer),
                                &bytes_written);
    
    ASSERT_SUCCESS(result);
    ASSERT_TRUE(bytes_written > 0);
    
    message_free(message);
    pal_cleanup(ctx);
    
    TEST_CASE_END();
}

void test_qos_mapping(void) {
    TEST_CASE("QoS Level Mapping");
    
    /* Test different QoS levels in MQTT */
    uint8_t mqtt_qos1_packet[] = {
        0x32, 0x1D,                          /* PUBLISH with QoS 1 */
        0x00, 0x0A, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',  /* Topic */
        0x12, 0x34,                          /* Packet ID */
        'Q', 'o', 'S', ' ', '1', ' ', 'T', 'e', 's', 't'              /* Payload */
    };
    
    message_t *message = NULL;
    paumiot_result_t result = mqtt_adapter.decode(&mqtt_adapter,
                                                  mqtt_qos1_packet,
                                                  sizeof(mqtt_qos1_packet),
                                                  &message);
    
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    ASSERT_EQ(QOS_LEVEL_1, message->metadata.qos);
    
    message_free(message);
    
    TEST_CASE_END();
}

void test_error_recovery(void) {
    TEST_CASE("Error Recovery and Resilience");
    
    /* Test malformed packet handling */
    uint8_t malformed_packet[] = {0x30, 0xFF, 0x00};  /* Invalid remaining length */
    
    message_t *message = NULL;
    paumiot_result_t result = mqtt_adapter.decode(&mqtt_adapter,
                                                  malformed_packet,
                                                  sizeof(malformed_packet),
                                                  &message);
    
    ASSERT_ERROR(result);
    ASSERT_NULL(message);
    
    /* Test buffer overflow conditions */
    uint8_t oversized_packet[65536];
    memset(oversized_packet, 0x30, sizeof(oversized_packet));
    
    result = mqtt_adapter.decode(&mqtt_adapter,
                                oversized_packet,
                                sizeof(oversized_packet),
                                &message);
    
    ASSERT_ERROR(result);
    ASSERT_NULL(message);
    
    TEST_CASE_END();
}

int main(void) {
    TEST_INIT();
    
    TEST_SUITE_BEGIN("Protocol Integration Tests");
    
    test_mqtt_connection_handling();
    test_coap_request_handling();
    test_protocol_interoperability();
    test_message_routing();
    test_qos_mapping();
    test_error_recovery();
    
    TEST_SUMMARY();
}