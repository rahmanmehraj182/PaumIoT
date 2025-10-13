/**
 * @file pal.h
 * @brief Protocol Adaptation Layer (PAL) interface
 * @details Provides uniform adapter interface for MQTT, CoAP and custom protocols
 */

#ifndef PAUMIOT_PAL_H
#define PAUMIOT_PAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common/types.h"
#include "common/errors.h"

/* Forward declarations */
struct protocol_adapter;
typedef struct protocol_adapter protocol_adapter_t;

/**
 * @brief Protocol adapter interface structure
 * 
 * This structure defines the uniform interface that all protocol adapters
 * must implement. It enables polymorphic behavior for different protocols.
 */
struct protocol_adapter {
    /* Adapter identification */
    protocol_type_t protocol_type;      /**< Protocol type this adapter handles */
    const char *name;                   /**< Human-readable adapter name */
    const char *version;                /**< Adapter version */
    
    /* Core adapter functions */
    
    /**
     * @brief Decode protocol-specific packet to internal message format
     * 
     * @param adapter Adapter instance
     * @param packet Raw packet data
     * @param packet_len Packet length
     * @param message Output message (allocated by function)
     * @return PAUMIOT_SUCCESS on success, error code on failure
     */
    paumiot_result_t (*decode)(
        const protocol_adapter_t *adapter,
        const uint8_t *packet, 
        size_t packet_len,
        message_t **message
    );
    
    /**
     * @brief Encode internal message to protocol-specific packet format
     * 
     * @param adapter Adapter instance
     * @param message Input message
     * @param packet Output packet buffer
     * @param packet_len Packet buffer length
     * @param bytes_written Number of bytes written to packet
     * @return PAUMIOT_SUCCESS on success, error code on failure
     */
    paumiot_result_t (*encode)(
        const protocol_adapter_t *adapter,
        const message_t *message,
        uint8_t *packet,
        size_t packet_len,
        size_t *bytes_written
    );
    
    /**
     * @brief Get adapter capabilities
     * 
     * @param adapter Adapter instance
     * @param capabilities Output capabilities structure
     * @return PAUMIOT_SUCCESS on success, error code on failure
     */
    paumiot_result_t (*get_capabilities)(
        const protocol_adapter_t *adapter,
        device_capabilities_t *capabilities
    );
    
    /**
     * @brief Handle control commands
     * 
     * @param adapter Adapter instance
     * @param command Control command string
     * @param params Command parameters (optional)
     * @return PAUMIOT_SUCCESS on success, error code on failure
     */
    paumiot_result_t (*handle_control)(
        protocol_adapter_t *adapter,
        const char *command,
        void *params
    );
    
    /**
     * @brief Initialize adapter
     * 
     * @param adapter Adapter instance
     * @param config Adapter-specific configuration
     * @return PAUMIOT_SUCCESS on success, error code on failure
     */
    paumiot_result_t (*init)(
        protocol_adapter_t *adapter,
        const void *config
    );
    
    /**
     * @brief Cleanup adapter resources
     * 
     * @param adapter Adapter instance
     */
    void (*cleanup)(protocol_adapter_t *adapter);
    
    /* Private adapter data */
    void *private_data;  /**< Adapter-specific private data */
};

/**
 * @brief PAL context structure
 */
struct pal_context {
    /* Registered adapters */
    protocol_adapter_t **adapters;     /**< Array of registered adapters */
    size_t num_adapters;               /**< Number of registered adapters */
    size_t max_adapters;               /**< Maximum number of adapters */
    
    /* State management */
    bool initialized;                  /**< Initialization status */
    
    /* Configuration */
    pal_config_t *config;             /**< PAL configuration */
    
    /* Statistics */
    system_stats_t stats;             /**< PAL statistics */
};

/* PAL management functions */

/**
 * @brief Initialize the Protocol Adaptation Layer
 * 
 * @param config PAL configuration
 * @return PAL context on success, NULL on failure
 */
pal_context_t *pal_init(const pal_config_t *config);

/**
 * @brief Cleanup PAL resources
 * 
 * @param ctx PAL context
 */
void pal_cleanup(pal_context_t *ctx);

/**
 * @brief Register a protocol adapter
 * 
 * @param ctx PAL context
 * @param adapter Protocol adapter to register
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t pal_register_adapter(pal_context_t *ctx, protocol_adapter_t *adapter);

/**
 * @brief Unregister a protocol adapter
 * 
 * @param ctx PAL context
 * @param protocol_type Protocol type to unregister
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t pal_unregister_adapter(pal_context_t *ctx, protocol_type_t protocol_type);

/**
 * @brief Find adapter for protocol type
 * 
 * @param ctx PAL context
 * @param protocol_type Protocol type
 * @return Adapter instance or NULL if not found
 */
protocol_adapter_t *pal_find_adapter(pal_context_t *ctx, protocol_type_t protocol_type);

/**
 * @brief Decode packet using appropriate adapter
 * 
 * @param ctx PAL context
 * @param protocol_type Protocol type
 * @param packet Packet data
 * @param packet_len Packet length
 * @param message Output message
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t pal_decode_packet(
    pal_context_t *ctx,
    protocol_type_t protocol_type,
    const uint8_t *packet,
    size_t packet_len,
    message_t **message
);

/**
 * @brief Encode message using appropriate adapter
 * 
 * @param ctx PAL context
 * @param message Input message
 * @param packet Output packet buffer
 * @param packet_len Packet buffer length
 * @param bytes_written Number of bytes written
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t pal_encode_message(
    pal_context_t *ctx,
    const message_t *message,
    uint8_t *packet,
    size_t packet_len,
    size_t *bytes_written
);

/* Utility functions */

/**
 * @brief Create a new message
 * 
 * @return New message instance or NULL on failure
 */
message_t *message_create(void);

/**
 * @brief Free a message and its resources
 * 
 * @param message Message to free
 */
void message_free(message_t *message);

/**
 * @brief Copy a message
 * 
 * @param src Source message
 * @return Copied message or NULL on failure
 */
message_t *message_copy(const message_t *src);

/**
 * @brief Set message payload
 * 
 * @param message Message instance
 * @param payload Payload data
 * @param payload_len Payload length
 * @return PAUMIOT_SUCCESS on success, error code on failure
 */
paumiot_result_t message_set_payload(message_t *message, const uint8_t *payload, size_t payload_len);

/* Built-in adapters */
extern protocol_adapter_t mqtt_adapter;  /**< MQTT adapter instance */
extern protocol_adapter_t coap_adapter;  /**< CoAP adapter instance */

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_PAL_H */