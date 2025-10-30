/**
 * @file test_types.c
 * @brief Unit tests for types.h
 */

#include "types.h"
#include <stdio.h>
#include <assert.h>

static void test_result_codes(void) {
    printf("Testing result codes...\n");
    
    /* Test success code */
    assert(PAUMIOT_SUCCESS == 0);
    
    /* Test error codes are negative */
    assert(PAUMIOT_ERROR_INVALID_PARAM < 0);
    assert(PAUMIOT_ERROR_NULL_POINTER < 0);
    assert(PAUMIOT_ERROR_OUT_OF_MEMORY < 0);
    assert(PAUMIOT_ERROR_BUFFER_TOO_SMALL < 0);
    assert(PAUMIOT_ERROR_QUEUE_FULL < 0);
    assert(PAUMIOT_ERROR_QUEUE_EMPTY < 0);
    assert(PAUMIOT_ERROR_NOT_FOUND < 0);
    assert(PAUMIOT_ERROR_ALREADY_EXISTS < 0);
    assert(PAUMIOT_ERROR_NOT_INITIALIZED < 0);
    assert(PAUMIOT_ERROR_INVALID_STATE < 0);
    assert(PAUMIOT_ERROR_CONNECTION_LOST < 0);
    assert(PAUMIOT_ERROR_CONNECTION_REFUSED < 0);
    assert(PAUMIOT_ERROR_TIMEOUT < 0);
    assert(PAUMIOT_ERROR_PROTOCOL_ERROR < 0);
    assert(PAUMIOT_ERROR_PROTOCOL_VERSION < 0);
    assert(PAUMIOT_ERROR_MALFORMED_PACKET < 0);
    assert(PAUMIOT_ERROR_NOT_SUPPORTED < 0);
    assert(PAUMIOT_ERROR_PERMISSION_DENIED < 0);
    assert(PAUMIOT_ERROR_OPERATION_FAILED < 0);
    assert(PAUMIOT_ERROR_INTERNAL < 0);
    assert(PAUMIOT_ERROR_UNKNOWN < 0);
    
    printf("  ✓ Result codes test passed\n");
}

static void test_protocol_types(void) {
    printf("Testing protocol types...\n");
    
    assert(PROTOCOL_TYPE_UNKNOWN == 0);
    assert(PROTOCOL_TYPE_MQTT == 1);
    assert(PROTOCOL_TYPE_COAP == 2);
    assert(PROTOCOL_TYPE_HTTP == 3);
    
    printf("  ✓ Protocol types test passed\n");
}

static void test_qos_levels(void) {
    printf("Testing QoS levels...\n");
    
    assert(QOS_LEVEL_0 == 0);
    assert(QOS_LEVEL_1 == 1);
    assert(QOS_LEVEL_2 == 2);
    
    printf("  ✓ QoS levels test passed\n");
}

static void test_operation_types(void) {
    printf("Testing operation types...\n");
    
    assert(OP_TYPE_CONNECT == 1);
    assert(OP_TYPE_DISCONNECT == 2);
    assert(OP_TYPE_PUBLISH == 3);
    assert(OP_TYPE_SUBSCRIBE == 4);
    assert(OP_TYPE_UNSUBSCRIBE == 5);
    assert(OP_TYPE_PING == 6);
    assert(OP_TYPE_RESPONSE == 7);
    
    printf("  ✓ Operation types test passed\n");
}

static void test_session_states(void) {
    printf("Testing session states...\n");
    
    assert(SESSION_STATE_DISCONNECTED == 0);
    assert(SESSION_STATE_CONNECTING == 1);
    assert(SESSION_STATE_CONNECTED == 2);
    assert(SESSION_STATE_ACTIVE == 3);
    assert(SESSION_STATE_DISCONNECTING == 4);
    assert(SESSION_STATE_ERROR == 5);
    
    printf("  ✓ Session states test passed\n");
}

static void test_sensor_states(void) {
    printf("Testing sensor states...\n");
    
    assert(SENSOR_STATE_UNKNOWN == 0);
    assert(SENSOR_STATE_ONLINE == 1);
    assert(SENSOR_STATE_OFFLINE == 2);
    assert(SENSOR_STATE_ERROR == 3);
    assert(SENSOR_STATE_DEGRADED == 4);
    
    printf("  ✓ Sensor states test passed\n");
}

int main(void) {
    printf("\n========================================\n");
    printf("Running types.h tests...\n");
    printf("========================================\n\n");
    
    test_result_codes();
    test_protocol_types();
    test_qos_levels();
    test_operation_types();
    test_session_states();
    test_sensor_states();
    
    printf("\n========================================\n");
    printf("✅ All tests passed successfully!\n");
    printf("========================================\n\n");
    
    return 0;
}
