/**
 * @file mqtt_adapter.c
 * @brief MQTT protocol adapter implementation
 * @details Implements MQTT v3.1.1 and v5.0 packet encoding/decoding
 */

#include "pal/pal.h"
#include "common/errors.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* MQTT Protocol Constants */
#define MQTT_PROTOCOL_NAME_V311 "MQTT"
#define MQTT_PROTOCOL_NAME_V5   "MQTT"
#define MQTT_PROTOCOL_LEVEL_311 4
#define MQTT_PROTOCOL_LEVEL_5   5

/* MQTT Packet Types */
typedef enum {
    MQTT_PACKET_RESERVED    = 0,
    MQTT_PACKET_CONNECT     = 1,
    MQTT_PACKET_CONNACK     = 2,
    MQTT_PACKET_PUBLISH     = 3,
    MQTT_PACKET_PUBACK      = 4,
    MQTT_PACKET_PUBREC      = 5,
    MQTT_PACKET_PUBREL      = 6,
    MQTT_PACKET_PUBCOMP     = 7,
    MQTT_PACKET_SUBSCRIBE   = 8,
    MQTT_PACKET_SUBACK      = 9,
    MQTT_PACKET_UNSUBSCRIBE = 10,
    MQTT_PACKET_UNSUBACK    = 11,
    MQTT_PACKET_PINGREQ     = 12,
    MQTT_PACKET_PINGRESP    = 13,
    MQTT_PACKET_DISCONNECT  = 14,
    MQTT_PACKET_AUTH        = 15
} mqtt_packet_type_t;

/* MQTT Adapter State */
typedef struct {
    uint16_t next_packet_id;
    uint8_t protocol_version;
    bool session_present;
    uint64_t packets_decoded;
    uint64_t packets_encoded;
    uint64_t errors;
} mqtt_adapter_state_t;

/* Global MQTT adapter state */
static mqtt_adapter_state_t g_mqtt_state = {
    .next_packet_id = 1,
    .protocol_version = MQTT_PROTOCOL_LEVEL_5,
    .session_present = false,
    .packets_decoded = 0,
    .packets_encoded = 0,
    .errors = 0
};

/* Helper Functions */

/**
 * @brief Decode variable length integer from MQTT packet
 */
static paumiot_result_t mqtt_decode_var_int(const uint8_t *buffer, size_t buf_len,
                                            size_t *pos, uint32_t *value) {
    if (!buffer || !pos || !value || *pos >= buf_len) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    uint32_t val = 0;
    uint8_t byte;
    int multiplier = 1;
    
    do {
        if (*pos >= buf_len) {
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        
        byte = buffer[(*pos)++];
        val += (byte & 0x7F) * multiplier;
        
        if (multiplier > 128 * 128 * 128) {
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        
        multiplier *= 128;
    } while (byte & 0x80);
    
    *value = val;
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Encode variable length integer to MQTT packet
 */
static paumiot_result_t mqtt_encode_var_int(uint32_t value, uint8_t *buffer, 
                                            size_t buf_len, size_t *bytes_written) {
    if (!buffer || !bytes_written || buf_len < 1) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    *bytes_written = 0;
    
    do {
        if (*bytes_written >= buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        
        uint8_t byte = value % 128;
        value /= 128;
        
        if (value > 0) {
            byte |= 0x80;
        }
        
        buffer[(*bytes_written)++] = byte;
    } while (value > 0);
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Decode UTF-8 string from MQTT packet
 */
static paumiot_result_t mqtt_decode_string(const uint8_t *buffer, size_t buf_len,
                                           size_t *pos, char **str) {
    if (!buffer || !pos || !str || *pos + 2 > buf_len) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    uint16_t str_len = (buffer[*pos] << 8) | buffer[*pos + 1];
    *pos += 2;
    
    if (*pos + str_len > buf_len) {
        return PAUMIOT_ERROR_PACKET_MALFORMED;
    }
    
    *str = malloc(str_len + 1);
    if (!*str) {
        return PAUMIOT_ERROR_OUT_OF_MEMORY;
    }
    
    memcpy(*str, buffer + *pos, str_len);
    (*str)[str_len] = '\0';
    *pos += str_len;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Encode UTF-8 string to MQTT packet
 */
static paumiot_result_t mqtt_encode_string(const char *str, uint8_t *buffer,
                                           size_t buf_len, size_t *pos) {
    if (!str || !buffer || !pos) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    uint16_t str_len = strlen(str);
    
    if (*pos + 2 + str_len > buf_len) {
        return PAUMIOT_ERROR_BUFFER_OVERFLOW;
    }
    
    buffer[(*pos)++] = str_len >> 8;
    buffer[(*pos)++] = str_len & 0xFF;
    
    memcpy(buffer + *pos, str, str_len);
    *pos += str_len;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Decode MQTT PUBLISH packet
 */
static paumiot_result_t mqtt_decode_publish(const uint8_t *packet, size_t packet_len,
                                            uint8_t flags, message_t **message) {
    if (!packet || packet_len < 2 || !message) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    size_t pos = 0;
    paumiot_result_t result;
    
    /* Decode topic name */
    char *topic = NULL;
    result = mqtt_decode_string(packet, packet_len, &pos, &topic);
    if (result != PAUMIOT_SUCCESS) {
        return result;
    }
    
    /* Extract QoS from flags */
    uint8_t qos = (flags >> 1) & 0x03;
    bool retain = flags & 0x01;
    bool dup = (flags >> 3) & 0x01;
    
    /* Decode packet identifier (if QoS > 0) */
    uint16_t packet_id = 0;
    if (qos > 0) {
        if (pos + 2 > packet_len) {
            free(topic);
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        packet_id = (packet[pos] << 8) | packet[pos + 1];
        pos += 2;
    }
    
    /* Skip properties (MQTT v5.0) */
    if (g_mqtt_state.protocol_version == MQTT_PROTOCOL_LEVEL_5) {
        uint32_t prop_len = 0;
        result = mqtt_decode_var_int(packet, packet_len, &pos, &prop_len);
        if (result != PAUMIOT_SUCCESS) {
            free(topic);
            return result;
        }
        
        if (pos + prop_len > packet_len) {
            free(topic);
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        
        pos += prop_len;
    }
    
    /* Create message */
    message_t *msg = message_create();
    if (!msg) {
        free(topic);
        return PAUMIOT_ERROR_OUT_OF_MEMORY;
    }
    
    msg->destination = topic;
    msg->metadata.protocol = PROTOCOL_TYPE_MQTT;
    msg->metadata.qos = (qos_level_t)qos;
    msg->metadata.retain = retain;
    
    /* Copy payload */
    size_t payload_len = packet_len - pos;
    if (payload_len > 0) {
        result = message_set_payload(msg, packet + pos, payload_len);
        if (result != PAUMIOT_SUCCESS) {
            message_free(msg);
            return result;
        }
    }
    
    *message = msg;
    g_mqtt_state.packets_decoded++;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Encode MQTT PUBLISH packet
 */
static paumiot_result_t mqtt_encode_publish(const message_t *message, uint8_t *buffer,
                                            size_t buf_len, size_t *bytes_written) {
    if (!message || !buffer || !bytes_written) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    ADAPTER_VALIDATE_MESSAGE(message);
    
    size_t pos = 0;
    paumiot_result_t result;
    
    /* Fixed header */
    if (pos >= buf_len) {
        return PAUMIOT_ERROR_BUFFER_OVERFLOW;
    }
    
    uint8_t packet_type = MQTT_PACKET_PUBLISH << 4;
    uint8_t flags = 0;
    
    /* Set flags */
    if (message->metadata.retain) flags |= 0x01;
    flags |= (message->metadata.qos & 0x03) << 1;
    /* DUP flag would be set during retransmission */
    
    buffer[pos++] = packet_type | flags;
    
    /* Calculate remaining length */
    size_t topic_len = strlen(message->destination);
    size_t remaining_len = 2 + topic_len; /* Topic length field + topic */
    
    /* Packet ID for QoS > 0 */
    if (message->metadata.qos > QOS_LEVEL_0) {
        remaining_len += 2;
    }
    
    /* Properties length (MQTT v5.0) */
    if (g_mqtt_state.protocol_version == MQTT_PROTOCOL_LEVEL_5) {
        remaining_len += 1; /* Properties length = 0 for now */
    }
    
    /* Payload */
    remaining_len += message->payload_len;
    
    /* Encode remaining length */
    size_t var_len_bytes;
    result = mqtt_encode_var_int(remaining_len, buffer + pos, buf_len - pos, &var_len_bytes);
    if (result != PAUMIOT_SUCCESS) {
        return result;
    }
    pos += var_len_bytes;
    
    /* Variable header - Topic */
    result = mqtt_encode_string(message->destination, buffer, buf_len, &pos);
    if (result != PAUMIOT_SUCCESS) {
        return result;
    }
    
    /* Packet identifier */
    if (message->metadata.qos > QOS_LEVEL_0) {
        if (pos + 2 > buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        uint16_t packet_id = g_mqtt_state.next_packet_id++;
        buffer[pos++] = packet_id >> 8;
        buffer[pos++] = packet_id & 0xFF;
    }
    
    /* Properties (MQTT v5.0) */
    if (g_mqtt_state.protocol_version == MQTT_PROTOCOL_LEVEL_5) {
        if (pos >= buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        buffer[pos++] = 0; /* Properties length = 0 */
    }
    
    /* Payload */
    if (message->payload_len > 0) {
        if (pos + message->payload_len > buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        memcpy(buffer + pos, message->payload, message->payload_len);
        pos += message->payload_len;
    }
    
    *bytes_written = pos;
    g_mqtt_state.packets_encoded++;
    
    return PAUMIOT_SUCCESS;
}

/* MQTT Adapter Interface Implementation */

/**
 * @brief Decode MQTT packet to internal message format
 */
static paumiot_result_t mqtt_adapter_decode(const protocol_adapter_t *adapter,
                                            const uint8_t *packet, size_t packet_len,
                                            message_t **message) {
    if (!adapter) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    ADAPTER_VALIDATE_PACKET(packet, packet_len, 2);
    
    if (!message) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    /* Parse fixed header */
    uint8_t packet_type_flags = packet[0];
    uint8_t packet_type = (packet_type_flags >> 4) & 0x0F;
    uint8_t flags = packet_type_flags & 0x0F;
    
    size_t pos = 1;
    uint32_t remaining_len = 0;
    paumiot_result_t result = mqtt_decode_var_int(packet, packet_len, &pos, &remaining_len);
    if (result != PAUMIOT_SUCCESS) {
        g_mqtt_state.errors++;
        return result;
    }
    
    /* Validate remaining length */
    if (pos + remaining_len != packet_len) {
        g_mqtt_state.errors++;
        return PAUMIOT_ERROR_PACKET_MALFORMED;
    }
    
    /* Handle different packet types */
    switch (packet_type) {
        case MQTT_PACKET_PUBLISH:
            result = mqtt_decode_publish(packet + pos, remaining_len, flags, message);
            break;
            
        case MQTT_PACKET_CONNECT:
        case MQTT_PACKET_SUBSCRIBE:
        case MQTT_PACKET_UNSUBSCRIBE:
            /* These would be implemented for a full broker */
            result = PAUMIOT_ERROR_PROTOCOL_UNSUPPORTED;
            break;
            
        default:
            result = PAUMIOT_ERROR_INVALID_PACKET_TYPE;
            break;
    }
    
    if (result != PAUMIOT_SUCCESS) {
        g_mqtt_state.errors++;
    }
    
    return result;
}

/**
 * @brief Encode internal message to MQTT packet format
 */
static paumiot_result_t mqtt_adapter_encode(const protocol_adapter_t *adapter,
                                            const message_t *message,
                                            uint8_t *packet, size_t packet_len,
                                            size_t *bytes_written) {
    if (!adapter || !message || !packet || !bytes_written) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (message->metadata.protocol != PROTOCOL_TYPE_MQTT) {
        return PAUMIOT_ERROR_PROTOCOL;
    }
    
    paumiot_result_t result = mqtt_encode_publish(message, packet, packet_len, bytes_written);
    if (result != PAUMIOT_SUCCESS) {
        g_mqtt_state.errors++;
    }
    
    return result;
}

/**
 * @brief Get MQTT adapter capabilities
 */
static paumiot_result_t mqtt_adapter_get_capabilities(const protocol_adapter_t *adapter,
                                                      device_capabilities_t *capabilities) {
    (void)adapter; /* Unused parameter */
    
    if (!capabilities) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    capabilities->supports_qos0 = true;
    capabilities->supports_qos1 = true;
    capabilities->supports_qos2 = true;
    capabilities->supports_retain = true;
    capabilities->supports_wildcards = true;
    capabilities->max_packet_size = 268435455; /* MQTT v5.0 max */
    capabilities->max_topic_alias = 65535;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Handle control commands
 */
static paumiot_result_t mqtt_adapter_handle_control(protocol_adapter_t *adapter,
                                                    const char *command,
                                                    void *params) {
    (void)adapter; /* Unused parameter */
    (void)params;  /* Unused parameter */
    
    if (!command) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (strcmp(command, "reset") == 0) {
        g_mqtt_state.next_packet_id = 1;
        g_mqtt_state.session_present = false;
        return PAUMIOT_SUCCESS;
    } else if (strcmp(command, "set_version") == 0) {
        /* Could set protocol version from params */
        return PAUMIOT_SUCCESS;
    }
    
    return PAUMIOT_ERROR_NOT_SUPPORTED;
}

/**
 * @brief Initialize MQTT adapter
 */
static paumiot_result_t mqtt_adapter_init(protocol_adapter_t *adapter, const void *config) {
    (void)config; /* Unused parameter */
    
    if (!adapter) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    adapter->private_data = &g_mqtt_state;
    memset(&g_mqtt_state, 0, sizeof(g_mqtt_state));
    g_mqtt_state.next_packet_id = 1;
    g_mqtt_state.protocol_version = MQTT_PROTOCOL_LEVEL_311;  /* Default to v3.1.1 */
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Cleanup MQTT adapter
 */
static void mqtt_adapter_cleanup(protocol_adapter_t *adapter) {
    if (adapter) {
        adapter->private_data = NULL;
    }
}

/* MQTT Adapter Instance */
protocol_adapter_t mqtt_adapter = {
    .protocol_type = PROTOCOL_TYPE_MQTT,
    .name = "MQTT Adapter",
    .version = "1.0.0",
    .decode = mqtt_adapter_decode,
    .encode = mqtt_adapter_encode,
    .get_capabilities = mqtt_adapter_get_capabilities,
    .handle_control = mqtt_adapter_handle_control,
    .init = mqtt_adapter_init,
    .cleanup = mqtt_adapter_cleanup,
    .private_data = NULL
};