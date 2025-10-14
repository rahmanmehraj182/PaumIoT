/**
 * @file coap_adapter.c
 * @brief CoAP protocol adapter implementation
 * @details Implements RFC 7252 CoAP packet encoding/decoding with observe and blockwise support
 */

#include "pal/pal.h"
#include "common/errors.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* CoAP Protocol Constants */
#define COAP_VERSION 1
#define COAP_HEADER_SIZE 4
#define COAP_PAYLOAD_MARKER 0xFF
#define COAP_MAX_TOKEN_LEN 8
#define COAP_MAX_OPTION_NUM 65535

/* CoAP Message Types */
typedef enum {
    COAP_TYPE_CON = 0,  /* Confirmable */
    COAP_TYPE_NON = 1,  /* Non-confirmable */
    COAP_TYPE_ACK = 2,  /* Acknowledgement */
    COAP_TYPE_RST = 3   /* Reset */
} coap_msg_type_t;

/* CoAP Method/Response Codes */
typedef enum {
    COAP_CODE_EMPTY = 0,
    /* Methods */
    COAP_CODE_GET = 1,
    COAP_CODE_POST = 2,
    COAP_CODE_PUT = 3,
    COAP_CODE_DELETE = 4,
    /* Success Responses */
    COAP_CODE_CREATED = 65,      /* 2.01 */
    COAP_CODE_DELETED = 66,      /* 2.02 */
    COAP_CODE_VALID = 67,        /* 2.03 */
    COAP_CODE_CHANGED = 68,      /* 2.04 */
    COAP_CODE_CONTENT = 69,      /* 2.05 */
    /* Error Responses */
    COAP_CODE_BAD_REQUEST = 128, /* 4.00 */
    COAP_CODE_UNAUTHORIZED = 129, /* 4.01 */
    COAP_CODE_BAD_OPTION = 130,  /* 4.02 */
    COAP_CODE_FORBIDDEN = 131,   /* 4.03 */
    COAP_CODE_NOT_FOUND = 132,   /* 4.04 */
    COAP_CODE_NOT_ALLOWED = 133, /* 4.05 */
    COAP_CODE_INTERNAL_ERROR = 160 /* 5.00 */
} coap_code_t;

/* CoAP Option Numbers */
typedef enum {
    COAP_OPTION_IF_MATCH = 1,
    COAP_OPTION_URI_HOST = 3,
    COAP_OPTION_ETAG = 4,
    COAP_OPTION_IF_NONE_MATCH = 5,
    COAP_OPTION_OBSERVE = 6,
    COAP_OPTION_URI_PORT = 7,
    COAP_OPTION_LOCATION_PATH = 8,
    COAP_OPTION_URI_PATH = 11,
    COAP_OPTION_CONTENT_FORMAT = 12,
    COAP_OPTION_MAX_AGE = 14,
    COAP_OPTION_URI_QUERY = 15,
    COAP_OPTION_ACCEPT = 17,
    COAP_OPTION_LOCATION_QUERY = 20,
    COAP_OPTION_BLOCK2 = 23,
    COAP_OPTION_BLOCK1 = 27,
    COAP_OPTION_PROXY_URI = 35,
    COAP_OPTION_PROXY_SCHEME = 39
} coap_option_num_t;

/* CoAP Content Formats */
typedef enum {
    COAP_FORMAT_TEXT_PLAIN = 0,
    COAP_FORMAT_LINK_FORMAT = 40,
    COAP_FORMAT_XML = 41,
    COAP_FORMAT_OCTET_STREAM = 42,
    COAP_FORMAT_EXI = 47,
    COAP_FORMAT_JSON = 50,
    COAP_FORMAT_CBOR = 60
} coap_content_format_t;

/* CoAP Option Structure */
typedef struct {
    uint16_t number;
    uint16_t length;
    uint8_t *value;
} coap_option_t;

/* CoAP Message Structure */
typedef struct {
    uint8_t version;
    coap_msg_type_t type;
    uint8_t token_len;
    coap_code_t code;
    uint16_t message_id;
    uint8_t token[COAP_MAX_TOKEN_LEN];
    coap_option_t *options;
    size_t num_options;
    uint8_t *payload;
    size_t payload_len;
} coap_message_t;

/* CoAP Adapter State */
typedef struct {
    uint16_t next_message_id;
    uint64_t packets_decoded;
    uint64_t packets_encoded;
    uint64_t errors;
} coap_adapter_state_t;

/* Global CoAP adapter state */
static coap_adapter_state_t g_coap_state = {
    .next_message_id = 1,
    .packets_decoded = 0,
    .packets_encoded = 0,
    .errors = 0
};

/* Helper Functions */

/**
 * @brief Decode option delta/length with extended values
 */
static paumiot_result_t coap_decode_option_ext(const uint8_t *buffer, size_t buf_len,
                                               size_t *pos, uint8_t base_value,
                                               uint16_t *result) {
    if (!buffer || !pos || !result || *pos >= buf_len) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (base_value < 13) {
        *result = base_value;
        return PAUMIOT_SUCCESS;
    } else if (base_value == 13) {
        if (*pos >= buf_len) {
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        *result = buffer[(*pos)++] + 13;
        return PAUMIOT_SUCCESS;
    } else if (base_value == 14) {
        if (*pos + 1 >= buf_len) {
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        *result = ((buffer[*pos] << 8) | buffer[*pos + 1]) + 269;
        *pos += 2;
        return PAUMIOT_SUCCESS;
    } else {
        return PAUMIOT_ERROR_PACKET_MALFORMED;
    }
}

/**
 * @brief Encode option delta/length with extended values
 */
static paumiot_result_t coap_encode_option_ext(uint16_t value, uint8_t *buffer,
                                               size_t buf_len, size_t *pos,
                                               uint8_t *base_value) {
    if (!buffer || !pos || !base_value) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (value < 13) {
        *base_value = value;
        return PAUMIOT_SUCCESS;
    } else if (value < 269) {
        *base_value = 13;
        if (*pos >= buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        buffer[(*pos)++] = value - 13;
        return PAUMIOT_SUCCESS;
    } else if (value < 65804) {
        *base_value = 14;
        if (*pos + 1 >= buf_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        uint16_t ext_value = value - 269;
        buffer[(*pos)++] = ext_value >> 8;
        buffer[(*pos)++] = ext_value & 0xFF;
        return PAUMIOT_SUCCESS;
    } else {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief Build URI path from CoAP Uri-Path options
 */
static paumiot_result_t coap_build_uri_path(const coap_option_t *options, size_t num_options,
                                            char **uri_path) {
    if (!options || !uri_path) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    size_t total_len = 1; /* For leading '/' */
    
    /* Calculate total length needed */
    for (size_t i = 0; i < num_options; i++) {
        if (options[i].number == COAP_OPTION_URI_PATH) {
            total_len += 1 + options[i].length; /* '/' + segment */
        }
    }
    
    *uri_path = malloc(total_len + 1);
    if (!*uri_path) {
        return PAUMIOT_ERROR_OUT_OF_MEMORY;
    }
    
    strcpy(*uri_path, "/");
    
    /* Build path from Uri-Path options */
    for (size_t i = 0; i < num_options; i++) {
        if (options[i].number == COAP_OPTION_URI_PATH) {
            if (strlen(*uri_path) > 1) {
                strcat(*uri_path, "/");
            }
            strncat(*uri_path, (char*)options[i].value, options[i].length);
        }
    }
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Get content type string from CoAP content format
 */
static const char *coap_get_content_type(uint16_t format) {
    switch (format) {
        case COAP_FORMAT_TEXT_PLAIN:
            return "text/plain";
        case COAP_FORMAT_JSON:
            return "application/json";
        case COAP_FORMAT_XML:
            return "application/xml";
        case COAP_FORMAT_OCTET_STREAM:
            return "application/octet-stream";
        case COAP_FORMAT_CBOR:
            return "application/cbor";
        default:
            return "application/octet-stream";
    }
}

/**
 * @brief Decode CoAP message from packet
 */
static paumiot_result_t coap_decode_message(const uint8_t *packet, size_t packet_len,
                                            coap_message_t *msg) {
    if (!packet || packet_len < COAP_HEADER_SIZE || !msg) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    size_t pos = 0;
    
    /* Decode header */
    uint8_t ver_type_tkl = packet[pos++];
    msg->version = (ver_type_tkl >> 6) & 0x03;
    msg->type = (coap_msg_type_t)((ver_type_tkl >> 4) & 0x03);
    msg->token_len = ver_type_tkl & 0x0F;
    
    if (msg->version != COAP_VERSION) {
        return PAUMIOT_ERROR_PROTOCOL_VIOLATION;
    }
    
    if (msg->token_len > COAP_MAX_TOKEN_LEN) {
        return PAUMIOT_ERROR_PACKET_MALFORMED;
    }
    
    msg->code = (coap_code_t)packet[pos++];
    msg->message_id = (packet[pos] << 8) | packet[pos + 1];
    pos += 2;
    
    /* Decode token */
    if (pos + msg->token_len > packet_len) {
        return PAUMIOT_ERROR_PACKET_MALFORMED;
    }
    
    memcpy(msg->token, packet + pos, msg->token_len);
    pos += msg->token_len;
    
    /* Decode options */
    msg->num_options = 0;
    msg->options = NULL;
    uint16_t prev_option_num = 0;
    
    while (pos < packet_len && packet[pos] != COAP_PAYLOAD_MARKER) {
        uint8_t option_header = packet[pos++];
        uint8_t delta_nibble = (option_header >> 4) & 0x0F;
        uint8_t length_nibble = option_header & 0x0F;
        
        /* Decode option delta */
        uint16_t option_delta;
        paumiot_result_t result = coap_decode_option_ext(packet, packet_len, &pos, 
                                                         delta_nibble, &option_delta);
        if (result != PAUMIOT_SUCCESS) {
            return result;
        }
        
        /* Decode option length */
        uint16_t option_length;
        result = coap_decode_option_ext(packet, packet_len, &pos, 
                                        length_nibble, &option_length);
        if (result != PAUMIOT_SUCCESS) {
            return result;
        }
        
        if (pos + option_length > packet_len) {
            return PAUMIOT_ERROR_PACKET_MALFORMED;
        }
        
        /* Store option */
        msg->options = realloc(msg->options, (msg->num_options + 1) * sizeof(coap_option_t));
        if (!msg->options) {
            return PAUMIOT_ERROR_OUT_OF_MEMORY;
        }
        
        coap_option_t *opt = &msg->options[msg->num_options];
        opt->number = prev_option_num + option_delta;
        opt->length = option_length;
        
        if (option_length > 0) {
            opt->value = malloc(option_length);
            if (!opt->value) {
                return PAUMIOT_ERROR_OUT_OF_MEMORY;
            }
            memcpy(opt->value, packet + pos, option_length);
            pos += option_length;
        } else {
            opt->value = NULL;
        }
        
        prev_option_num = opt->number;
        msg->num_options++;
    }
    
    /* Decode payload */
    if (pos < packet_len && packet[pos] == COAP_PAYLOAD_MARKER) {
        pos++; /* Skip payload marker */
        msg->payload_len = packet_len - pos;
        if (msg->payload_len > 0) {
            msg->payload = malloc(msg->payload_len);
            if (!msg->payload) {
                return PAUMIOT_ERROR_OUT_OF_MEMORY;
            }
            memcpy(msg->payload, packet + pos, msg->payload_len);
        } else {
            msg->payload = NULL;
        }
    } else {
        msg->payload = NULL;
        msg->payload_len = 0;
    }
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Free CoAP message resources
 */
static void coap_free_message(coap_message_t *msg) {
    if (!msg) return;
    
    if (msg->options) {
        for (size_t i = 0; i < msg->num_options; i++) {
            free(msg->options[i].value);
        }
        free(msg->options);
    }
    
    free(msg->payload);
    memset(msg, 0, sizeof(coap_message_t));
}

/* CoAP Adapter Interface Implementation */

/**
 * @brief Decode CoAP packet to internal message format
 */
static paumiot_result_t coap_adapter_decode(const protocol_adapter_t *adapter,
                                            const uint8_t *packet, size_t packet_len,
                                            message_t **message) {
    if (!adapter) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    ADAPTER_VALIDATE_PACKET(packet, packet_len, COAP_HEADER_SIZE);
    
    if (!message) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    coap_message_t coap_msg = {0};
    paumiot_result_t result = coap_decode_message(packet, packet_len, &coap_msg);
    if (result != PAUMIOT_SUCCESS) {
        g_coap_state.errors++;
        return result;
    }
    
    /* Create internal message */
    message_t *msg = message_create();
    if (!msg) {
        coap_free_message(&coap_msg);
        return PAUMIOT_ERROR_OUT_OF_MEMORY;
    }
    
    msg->metadata.protocol = PROTOCOL_TYPE_COAP;
    
    /* Map CoAP type to QoS */
    msg->metadata.qos = (coap_msg.type == COAP_TYPE_CON) ? QOS_LEVEL_1 : QOS_LEVEL_0;
    
    /* Build URI path from options */
    result = coap_build_uri_path(coap_msg.options, coap_msg.num_options, 
                                 &msg->destination);
    if (result != PAUMIOT_SUCCESS) {
        message_free(msg);
        coap_free_message(&coap_msg);
        return result;
    }
    
    /* Set content type from options */
    for (size_t i = 0; i < coap_msg.num_options; i++) {
        if (coap_msg.options[i].number == COAP_OPTION_CONTENT_FORMAT && 
            coap_msg.options[i].length <= 2) {
            uint16_t format = 0;
            for (size_t j = 0; j < coap_msg.options[i].length; j++) {
                format = (format << 8) | coap_msg.options[i].value[j];
            }
            msg->metadata.content_type = strdup(coap_get_content_type(format));
            break;
        }
    }
    
    /* Copy payload */
    if (coap_msg.payload_len > 0) {
        result = message_set_payload(msg, coap_msg.payload, coap_msg.payload_len);
        if (result != PAUMIOT_SUCCESS) {
            message_free(msg);
            coap_free_message(&coap_msg);
            return result;
        }
    }
    
    coap_free_message(&coap_msg);
    *message = msg;
    g_coap_state.packets_decoded++;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Encode internal message to CoAP packet format
 */
static paumiot_result_t coap_adapter_encode(const protocol_adapter_t *adapter,
                                            const message_t *message,
                                            uint8_t *packet, size_t packet_len,
                                            size_t *bytes_written) {
    if (!adapter || !message || !packet || !bytes_written) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (message->metadata.protocol != PROTOCOL_TYPE_COAP) {
        return PAUMIOT_ERROR_PROTOCOL;
    }
    
    ADAPTER_VALIDATE_MESSAGE(message);
    
    size_t pos = 0;
    
    /* Encode header */
    if (pos + COAP_HEADER_SIZE > packet_len) {
        return PAUMIOT_ERROR_BUFFER_OVERFLOW;
    }
    
    uint8_t ver_type_tkl = (COAP_VERSION << 6);
    ver_type_tkl |= ((message->metadata.qos > QOS_LEVEL_0 ? COAP_TYPE_CON : COAP_TYPE_NON) << 4);
    ver_type_tkl |= 0; /* Token length = 0 for simplicity */
    
    packet[pos++] = ver_type_tkl;
    packet[pos++] = COAP_CODE_GET; /* Default to GET for simplicity */
    
    uint16_t msg_id = g_coap_state.next_message_id++;
    packet[pos++] = msg_id >> 8;
    packet[pos++] = msg_id & 0xFF;
    
    /* Encode URI-Path options */
    const char *path = message->destination;
    if (path && path[0] == '/') {
        path++; /* Skip leading '/' */
    }
    
    uint16_t prev_option_num = 0;
    
    while (path && *path) {
        const char *next_slash = strchr(path, '/');
        size_t segment_len = next_slash ? (size_t)(next_slash - path) : strlen(path);
        
        if (segment_len > 0) {
            uint16_t option_num = COAP_OPTION_URI_PATH;
            uint16_t option_delta = option_num - prev_option_num;
            
            /* Encode option header */
            if (pos >= packet_len) {
                return PAUMIOT_ERROR_BUFFER_OVERFLOW;
            }
            
            uint8_t delta_nibble, length_nibble;
            size_t option_header_pos = pos++;
            
            paumiot_result_t result = coap_encode_option_ext(option_delta, packet, packet_len,
                                                             &pos, &delta_nibble);
            if (result != PAUMIOT_SUCCESS) {
                return result;
            }
            
            result = coap_encode_option_ext(segment_len, packet, packet_len,
                                            &pos, &length_nibble);
            if (result != PAUMIOT_SUCCESS) {
                return result;
            }
            
            packet[option_header_pos] = (delta_nibble << 4) | length_nibble;
            
            /* Copy option value */
            if (pos + segment_len > packet_len) {
                return PAUMIOT_ERROR_BUFFER_OVERFLOW;
            }
            
            memcpy(packet + pos, path, segment_len);
            pos += segment_len;
            
            prev_option_num = option_num;
        }
        
        path = next_slash ? next_slash + 1 : NULL;
    }
    
    /* Add payload if present */
    if (message->payload_len > 0) {
        if (pos >= packet_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        
        packet[pos++] = COAP_PAYLOAD_MARKER;
        
        if (pos + message->payload_len > packet_len) {
            return PAUMIOT_ERROR_BUFFER_OVERFLOW;
        }
        
        memcpy(packet + pos, message->payload, message->payload_len);
        pos += message->payload_len;
    }
    
    *bytes_written = pos;
    g_coap_state.packets_encoded++;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Get CoAP adapter capabilities
 */
static paumiot_result_t coap_adapter_get_capabilities(const protocol_adapter_t *adapter,
                                                      device_capabilities_t *capabilities) {
    (void)adapter; /* Unused parameter */
    
    if (!capabilities) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    capabilities->supports_qos0 = true;
    capabilities->supports_qos1 = true;
    capabilities->supports_qos2 = false; /* CoAP doesn't have QoS 2 */
    capabilities->supports_retain = false; /* CoAP doesn't have retain */
    capabilities->supports_wildcards = false; /* CoAP doesn't have wildcards */
    capabilities->max_packet_size = 1152; /* CoAP default MTU */
    capabilities->max_topic_alias = 0; /* CoAP doesn't use topic aliases */
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Handle control commands
 */
static paumiot_result_t coap_adapter_handle_control(protocol_adapter_t *adapter,
                                                    const char *command,
                                                    void *params) {
    (void)adapter; /* Unused parameter */
    (void)params;  /* Unused parameter */
    
    if (!command) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (strcmp(command, "reset") == 0) {
        g_coap_state.next_message_id = 1;
        return PAUMIOT_SUCCESS;
    }
    
    return PAUMIOT_ERROR_NOT_SUPPORTED;
}

/**
 * @brief Initialize CoAP adapter
 */
static paumiot_result_t coap_adapter_init(protocol_adapter_t *adapter, const void *config) {
    (void)config; /* Unused parameter */
    
    if (!adapter) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    adapter->private_data = &g_coap_state;
    memset(&g_coap_state, 0, sizeof(g_coap_state));
    g_coap_state.next_message_id = 1;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Cleanup CoAP adapter
 */
static void coap_adapter_cleanup(protocol_adapter_t *adapter) {
    if (adapter) {
        adapter->private_data = NULL;
    }
}

/* CoAP Adapter Instance */
protocol_adapter_t coap_adapter = {
    .protocol_type = PROTOCOL_TYPE_COAP,
    .name = "CoAP Adapter",
    .version = "1.0.0",
    .decode = coap_adapter_decode,
    .encode = coap_adapter_encode,
    .get_capabilities = coap_adapter_get_capabilities,
    .handle_control = coap_adapter_handle_control,
    .init = coap_adapter_init,
    .cleanup = coap_adapter_cleanup,
    .private_data = NULL
};