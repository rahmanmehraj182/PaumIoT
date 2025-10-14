/**
 * @file test_pal_adapters.c
 * @brief Unit tests for Protocol Adaptation Layer adapters
 */

#include "../test_framework.h"
#include "../../include/paumiot.h"

/* Test data */
static const char *test_mqtt_topic = "test/topic";
static const char *test_payload = "Hello, World!";
static const uint8_t test_mqtt_publish[] = {
    0x30, 0x19,           /* PUBLISH header, remaining length = 25 */
    0x00, 0x0A,           /* Topic length = 10 */
    't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',  /* Topic: "test/topic" */
    'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'  /* Payload */
};

void test_message_creation(void) {
    TEST_CASE("Message Creation and Cleanup");
    
    message_t *msg = message_create();
    ASSERT_NOT_NULL(msg);
    ASSERT_NOT_NULL(msg->message_id);    /* Auto-generated ID */
    ASSERT_NOT_NULL(msg->timestamp);     /* Auto-generated timestamp */
    ASSERT_NULL(msg->source);
    ASSERT_NULL(msg->destination);
    ASSERT_NULL(msg->payload);
    ASSERT_EQ(0, msg->payload_len);
    
    message_free(msg);
    
    TEST_CASE_END();
}

void test_message_payload_handling(void) {
    TEST_CASE("Message Payload Handling");
    
    message_t *msg = message_create();
    ASSERT_NOT_NULL(msg);
    
    const char *payload = "test payload";
    size_t payload_len = strlen(payload);
    
    paumiot_result_t result = message_set_payload(msg, 
                                                  (const uint8_t*)payload, 
                                                  payload_len);
    ASSERT_SUCCESS(result);
    ASSERT_EQ(payload_len, msg->payload_len);
    ASSERT_TRUE(memcmp(msg->payload, payload, payload_len) == 0);
    
    message_free(msg);
    
    TEST_CASE_END();
}

void test_message_copy(void) {
    TEST_CASE("Message Copy Functionality");
    
    message_t *original = message_create();
    ASSERT_NOT_NULL(original);
    
    /* Set up original message */
    original->source = strdup("test_source");
    original->destination = strdup("test_destination");
    message_set_payload(original, (const uint8_t*)"test_data", 9);
    original->metadata.qos = QOS_LEVEL_1;
    original->metadata.protocol = PROTOCOL_TYPE_MQTT;
    
    /* Copy message */
    message_t *copy = message_copy(original);
    ASSERT_NOT_NULL(copy);
    
    /* Verify copy */
    ASSERT_STR_EQ(original->source, copy->source);
    ASSERT_STR_EQ(original->destination, copy->destination);
    ASSERT_EQ(original->payload_len, copy->payload_len);
    ASSERT_TRUE(memcmp(original->payload, copy->payload, original->payload_len) == 0);
    ASSERT_EQ(original->metadata.qos, copy->metadata.qos);
    ASSERT_EQ(original->metadata.protocol, copy->metadata.protocol);
    
    /* Verify they are separate objects */
    ASSERT_NEQ((long)original->source, (long)copy->source);
    ASSERT_NEQ((long)original->payload, (long)copy->payload);
    
    message_free(original);
    message_free(copy);
    
    TEST_CASE_END();
}

void test_pal_initialization(void) {
    TEST_CASE("PAL Initialization");
    
    pal_config_t config = {0}; /* Initialize with defaults */
    
    pal_context_t *ctx = pal_init(&config);
    ASSERT_NOT_NULL(ctx);
    ASSERT_TRUE(ctx->initialized);
    ASSERT_EQ(0, ctx->num_adapters);
    
    pal_cleanup(ctx);
    
    TEST_CASE_END();
}

void test_adapter_registration(void) {
    TEST_CASE("Adapter Registration and Lookup");
    
    pal_config_t config = {0};
    pal_context_t *ctx = pal_init(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Register MQTT adapter */
    paumiot_result_t result = pal_register_adapter(ctx, &mqtt_adapter);
    ASSERT_SUCCESS(result);
    ASSERT_EQ(1, ctx->num_adapters);
    
    /* Find adapter */
    protocol_adapter_t *adapter = pal_find_adapter(ctx, PROTOCOL_TYPE_MQTT);
    ASSERT_NOT_NULL(adapter);
    ASSERT_EQ(PROTOCOL_TYPE_MQTT, adapter->protocol_type);
    
    /* Try to find non-existent adapter */
    adapter = pal_find_adapter(ctx, PROTOCOL_TYPE_COAP);
    ASSERT_NULL(adapter);
    
    /* Unregister adapter */
    result = pal_unregister_adapter(ctx, PROTOCOL_TYPE_MQTT);
    ASSERT_SUCCESS(result);
    ASSERT_EQ(0, ctx->num_adapters);
    
    pal_cleanup(ctx);
    
    TEST_CASE_END();
}

void test_mqtt_adapter_decode(void) {
    TEST_CASE("MQTT Adapter Decode");
    
    message_t *message = NULL;
    
    /* Test MQTT PUBLISH packet decode */
    paumiot_result_t result = mqtt_adapter.decode(&mqtt_adapter,
                                                  test_mqtt_publish,
                                                  sizeof(test_mqtt_publish),
                                                  &message);
    
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ(test_mqtt_topic, message->destination);
    ASSERT_EQ(strlen(test_payload), message->payload_len);
    ASSERT_TRUE(memcmp(test_payload, message->payload, message->payload_len) == 0);
    ASSERT_EQ(QOS_LEVEL_0, message->metadata.qos);
    ASSERT_EQ(PROTOCOL_TYPE_MQTT, message->metadata.protocol);
    
    message_free(message);
    
    TEST_CASE_END();
}

void test_mqtt_adapter_encode(void) {
    TEST_CASE("MQTT Adapter Encode");
    
    /* Create test message */
    message_t *message = message_create();
    ASSERT_NOT_NULL(message);
    
    message->destination = strdup(test_mqtt_topic);
    message_set_payload(message, (const uint8_t*)test_payload, strlen(test_payload));
    message->metadata.qos = QOS_LEVEL_0;
    message->metadata.protocol = PROTOCOL_TYPE_MQTT;
    
    /* Encode message */
    uint8_t buffer[256];
    size_t bytes_written = 0;
    
    paumiot_result_t result = mqtt_adapter.encode(&mqtt_adapter,
                                                  message,
                                                  buffer,
                                                  sizeof(buffer),
                                                  &bytes_written);
    
    ASSERT_SUCCESS(result);
    ASSERT_TRUE(bytes_written > 0);
    ASSERT_TRUE(bytes_written <= sizeof(buffer));
    
    /* Basic sanity check on encoded packet */
    ASSERT_EQ(0x30, buffer[0] & 0xF0);  /* PUBLISH packet type */
    
    message_free(message);
    
    TEST_CASE_END();
}

void test_pal_packet_processing(void) {
    TEST_CASE("PAL Packet Processing");
    
    pal_config_t config = {0};
    pal_context_t *ctx = pal_init(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Register MQTT adapter */
    paumiot_result_t result = pal_register_adapter(ctx, &mqtt_adapter);
    ASSERT_SUCCESS(result);
    
    /* Decode packet through PAL */
    message_t *message = NULL;
    result = pal_decode_packet(ctx,
                               PROTOCOL_TYPE_MQTT,
                               test_mqtt_publish,
                               sizeof(test_mqtt_publish),
                               &message);
    
    ASSERT_SUCCESS(result);
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ(test_mqtt_topic, message->destination);
    
    /* Encode message back through PAL */
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

void test_adapter_capabilities(void) {
    TEST_CASE("Adapter Capabilities");
    
    device_capabilities_t capabilities;
    
    paumiot_result_t result = mqtt_adapter.get_capabilities(&mqtt_adapter, &capabilities);
    ASSERT_SUCCESS(result);
    
    /* MQTT should support all QoS levels */
    ASSERT_TRUE(capabilities.supports_qos0);
    ASSERT_TRUE(capabilities.supports_qos1);
    ASSERT_TRUE(capabilities.supports_qos2);
    ASSERT_TRUE(capabilities.supports_retain);
    ASSERT_TRUE(capabilities.supports_wildcards);
    
    TEST_CASE_END();
}

void test_invalid_inputs(void) {
    TEST_CASE("Invalid Input Handling");
    
    message_t *message = NULL;
    uint8_t buffer[256];
    size_t bytes_written = 0;
    
    /* Test NULL adapter */
    paumiot_result_t result = mqtt_adapter.decode(NULL,
                                                  test_mqtt_publish,
                                                  sizeof(test_mqtt_publish),
                                                  &message);
    ASSERT_ERROR(result);
    
    /* Test NULL packet */
    result = mqtt_adapter.decode(&mqtt_adapter,
                                 NULL,
                                 sizeof(test_mqtt_publish),
                                 &message);
    ASSERT_ERROR(result);
    
    /* Test zero length packet */
    result = mqtt_adapter.decode(&mqtt_adapter,
                                 test_mqtt_publish,
                                 0,
                                 &message);
    ASSERT_ERROR(result);
    
    /* Test NULL message output */
    result = mqtt_adapter.decode(&mqtt_adapter,
                                 test_mqtt_publish,
                                 sizeof(test_mqtt_publish),
                                 NULL);
    ASSERT_ERROR(result);
    
    TEST_CASE_END();
}

int main(void) {
    TEST_INIT();
    
    TEST_SUITE_BEGIN("Protocol Adaptation Layer Tests");
    
    test_message_creation();
    test_message_payload_handling();
    test_message_copy();
    test_pal_initialization();
    test_adapter_registration();
    test_mqtt_adapter_decode();
    test_mqtt_adapter_encode();
    test_pal_packet_processing();
    test_adapter_capabilities();
    test_invalid_inputs();
    
    TEST_SUMMARY();
}