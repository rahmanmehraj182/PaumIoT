/**
 * @file pal.c
 * @brief Protocol Adaptation Layer core implementation
 * @details Implements PAL context management, adapter registration, and message utilities
 */

#include "pal/pal.h"
#include "common/errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <uuid/uuid.h>

/* Message Utilities Implementation */

/**
 * @brief Generate unique message ID (UUID v4)
 */
paumiot_result_t message_generate_id(char *buffer, size_t buf_len) {
    if (!buffer || buf_len < 37) { /* UUID string length + null terminator */
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
#ifdef __linux__
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, buffer);
#else
    /* Fallback UUID generation for systems without libuuid */
    static uint32_t counter = 0;
    snprintf(buffer, buf_len, "%08x-%04x-%04x-%04x-%08x%04x",
             (uint32_t)time(NULL),
             (uint16_t)(rand() & 0xFFFF),
             (uint16_t)((rand() & 0x0FFF) | 0x4000), /* Version 4 */
             (uint16_t)((rand() & 0x3FFF) | 0x8000), /* Variant 10 */
             (uint32_t)getpid(),
             (uint16_t)(++counter & 0xFFFF));
#endif
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Generate ISO8601 timestamp
 */
paumiot_result_t message_generate_timestamp(char *buffer, size_t buf_len) {
    if (!buffer || buf_len < 21) { /* ISO8601 length + null terminator */
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    time_t now = time(NULL);
    struct tm *utc_tm = gmtime(&now);
    
    if (!utc_tm) {
        return PAUMIOT_ERROR_GENERAL;
    }
    
    strftime(buffer, buf_len, "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Create a new message
 */
message_t *message_create(void) {
    message_t *msg = calloc(1, sizeof(message_t));
    if (!msg) {
        return NULL;
    }
    
    /* Generate unique ID and timestamp */
    char id_buffer[64];
    char timestamp_buffer[32];
    
    if (message_generate_id(id_buffer, sizeof(id_buffer)) == PAUMIOT_SUCCESS) {
        msg->message_id = strdup(id_buffer);
    }
    
    if (message_generate_timestamp(timestamp_buffer, sizeof(timestamp_buffer)) == PAUMIOT_SUCCESS) {
        msg->timestamp = strdup(timestamp_buffer);
    }
    
    /* Set default metadata */
    msg->metadata.qos = QOS_LEVEL_0;
    msg->metadata.protocol = PROTOCOL_TYPE_UNKNOWN;
    msg->metadata.retain = false;
    msg->metadata.ttl = 0;
    
    return msg;
}

/**
 * @brief Free message and all its resources
 */
void message_free(message_t *message) {
    if (!message) {
        return;
    }
    
    free(message->message_id);
    free(message->timestamp);
    free(message->source);
    free(message->destination);
    free(message->payload);
    free(message->metadata.content_type);
    free(message);
}

/**
 * @brief Create a deep copy of a message
 */
message_t *message_copy(const message_t *original) {
    if (!original) {
        return NULL;
    }
    
    message_t *copy = message_create();
    if (!copy) {
        return NULL;
    }
    
    /* Copy strings */
    if (original->message_id) {
        free(copy->message_id);
        copy->message_id = strdup(original->message_id);
    }
    
    if (original->timestamp) {
        free(copy->timestamp);
        copy->timestamp = strdup(original->timestamp);
    }
    
    if (original->source) {
        copy->source = strdup(original->source);
    }
    
    if (original->destination) {
        copy->destination = strdup(original->destination);
    }
    
    if (original->metadata.content_type) {
        copy->metadata.content_type = strdup(original->metadata.content_type);
    }
    
    /* Copy payload */
    if (original->payload && original->payload_len > 0) {
        copy->payload = malloc(original->payload_len);
        if (copy->payload) {
            memcpy(copy->payload, original->payload, original->payload_len);
            copy->payload_len = original->payload_len;
        }
    }
    
    /* Copy metadata */
    copy->metadata.qos = original->metadata.qos;
    copy->metadata.protocol = original->metadata.protocol;
    copy->metadata.retain = original->metadata.retain;
    copy->metadata.ttl = original->metadata.ttl;
    
    return copy;
}

/**
 * @brief Set message payload
 */
paumiot_result_t message_set_payload(message_t *message, const uint8_t *payload, size_t payload_len) {
    if (!message || (!payload && payload_len > 0)) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    /* Free existing payload */
    free(message->payload);
    message->payload = NULL;
    message->payload_len = 0;
    
    /* Set new payload */
    if (payload_len > 0) {
        message->payload = malloc(payload_len);
        if (!message->payload) {
            return PAUMIOT_ERROR_OUT_OF_MEMORY;
        }
        
        memcpy(message->payload, payload, payload_len);
        message->payload_len = payload_len;
    }
    
    return PAUMIOT_SUCCESS;
}

/* PAL Core Implementation */

/**
 * @brief Initialize PAL context
 */
pal_context_t *pal_init(const pal_config_t *config) {
    (void)config; /* May be NULL for default config */
    
    pal_context_t *ctx = calloc(1, sizeof(pal_context_t));
    if (!ctx) {
        return NULL;
    }
    
    /* Allocate adapter array */
    ctx->adapters = calloc(PAL_MAX_ADAPTERS, sizeof(protocol_adapter_t*));
    if (!ctx->adapters) {
        free(ctx);
        return NULL;
    }
    
    ctx->max_adapters = PAL_MAX_ADAPTERS;
    ctx->num_adapters = 0;
    ctx->initialized = true;
    
    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    
    return ctx;
}

/**
 * @brief Cleanup PAL context
 */
void pal_cleanup(pal_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    /* Cleanup all registered adapters */
    for (size_t i = 0; i < ctx->num_adapters; i++) {
        if (ctx->adapters[i] && ctx->adapters[i]->cleanup) {
            ctx->adapters[i]->cleanup(ctx->adapters[i]);
        }
    }
    
    free(ctx->adapters);
    free(ctx->config);
    free(ctx);
}

/**
 * @brief Register a protocol adapter
 */
paumiot_result_t pal_register_adapter(pal_context_t *ctx, protocol_adapter_t *adapter) {
    if (!ctx || !adapter) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->initialized) {
        return PAUMIOT_ERROR_NOT_INITIALIZED;
    }
    
    if (ctx->num_adapters >= ctx->max_adapters) {
        return PAUMIOT_ERROR_OUT_OF_MEMORY;
    }
    
    /* Check if adapter for this protocol is already registered */
    for (size_t i = 0; i < ctx->num_adapters; i++) {
        if (ctx->adapters[i]->protocol_type == adapter->protocol_type) {
            return PAUMIOT_ERROR_ALREADY_INITIALIZED;
        }
    }
    
    /* Initialize adapter if needed */
    if (adapter->init) {
        paumiot_result_t result = adapter->init(adapter, NULL);
        if (result != PAUMIOT_SUCCESS) {
            return result;
        }
    }
    
    /* Register adapter */
    ctx->adapters[ctx->num_adapters] = adapter;
    ctx->num_adapters++;
    
    return PAUMIOT_SUCCESS;
}

/**
 * @brief Unregister a protocol adapter
 */
paumiot_result_t pal_unregister_adapter(pal_context_t *ctx, protocol_type_t protocol_type) {
    if (!ctx) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->initialized) {
        return PAUMIOT_ERROR_NOT_INITIALIZED;
    }
    
    /* Find and remove adapter */
    for (size_t i = 0; i < ctx->num_adapters; i++) {
        if (ctx->adapters[i]->protocol_type == protocol_type) {
            /* Cleanup adapter */
            if (ctx->adapters[i]->cleanup) {
                ctx->adapters[i]->cleanup(ctx->adapters[i]);
            }
            
            /* Shift remaining adapters */
            for (size_t j = i; j < ctx->num_adapters - 1; j++) {
                ctx->adapters[j] = ctx->adapters[j + 1];
            }
            
            ctx->adapters[ctx->num_adapters - 1] = NULL;
            ctx->num_adapters--;
            
            return PAUMIOT_SUCCESS;
        }
    }
    
    return PAUMIOT_ERROR_PROTOCOL_UNKNOWN;
}

/**
 * @brief Find adapter by protocol type
 */
protocol_adapter_t *pal_find_adapter(pal_context_t *ctx, protocol_type_t protocol_type) {
    if (!ctx || !ctx->initialized) {
        return NULL;
    }
    
    for (size_t i = 0; i < ctx->num_adapters; i++) {
        if (ctx->adapters[i]->protocol_type == protocol_type) {
            return ctx->adapters[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Decode packet using appropriate adapter
 */
paumiot_result_t pal_decode_packet(pal_context_t *ctx, protocol_type_t protocol_type,
                                   const uint8_t *packet, size_t packet_len,
                                   message_t **message) {
    if (!ctx || !packet || !message) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->initialized) {
        return PAUMIOT_ERROR_NOT_INITIALIZED;
    }
    
    protocol_adapter_t *adapter = pal_find_adapter(ctx, protocol_type);
    if (!adapter) {
        return PAUMIOT_ERROR_PROTOCOL_UNKNOWN;
    }
    
    if (!adapter->decode) {
        return PAUMIOT_ERROR_NOT_SUPPORTED;
    }
    
    paumiot_result_t result = adapter->decode(adapter, packet, packet_len, message);
    
    /* Update statistics */
    if (result == PAUMIOT_SUCCESS) {
        ctx->stats.messages_received++;
        ctx->stats.bytes_received += packet_len;
    } else {
        ctx->stats.errors++;
    }
    
    return result;
}

/**
 * @brief Encode message using appropriate adapter
 */
paumiot_result_t pal_encode_message(pal_context_t *ctx, const message_t *message,
                                    uint8_t *packet, size_t packet_len,
                                    size_t *bytes_written) {
    if (!ctx || !message || !packet || !bytes_written) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->initialized) {
        return PAUMIOT_ERROR_NOT_INITIALIZED;
    }
    
    protocol_adapter_t *adapter = pal_find_adapter(ctx, message->metadata.protocol);
    if (!adapter) {
        return PAUMIOT_ERROR_PROTOCOL_UNKNOWN;
    }
    
    if (!adapter->encode) {
        return PAUMIOT_ERROR_NOT_SUPPORTED;
    }
    
    paumiot_result_t result = adapter->encode(adapter, message, packet, packet_len, bytes_written);
    
    /* Update statistics */
    if (result == PAUMIOT_SUCCESS) {
        ctx->stats.messages_sent++;
        ctx->stats.bytes_sent += *bytes_written;
    } else {
        ctx->stats.errors++;
    }
    
    return result;
}

/**
 * @brief Get PAL statistics
 */
paumiot_result_t pal_get_stats(pal_context_t *ctx, system_stats_t *stats) {
    if (!ctx || !stats) {
        return PAUMIOT_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->initialized) {
        return PAUMIOT_ERROR_NOT_INITIALIZED;
    }
    
    *stats = ctx->stats;
    return PAUMIOT_SUCCESS;
}