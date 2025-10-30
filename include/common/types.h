/**
 * @file types.h
 * @brief Common type definitions for PaumIoT middleware
 */

#ifndef PAUMIOT_COMMON_TYPES_H
#define PAUMIOT_COMMON_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* Basic type definitions */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

/* Protocol types */
typedef enum {
    PROTOCOL_TYPE_UNKNOWN = 0,
    PROTOCOL_TYPE_MQTT,
    PROTOCOL_TYPE_COAP,
    PROTOCOL_TYPE_HTTP,
    PROTOCOL_TYPE_WEBSOCKET,
    PROTOCOL_TYPE_CUSTOM
} protocol_type_t;

/* Quality of Service levels */
typedef enum {
    QOS_LEVEL_0 = 0,  /* At most once */
    QOS_LEVEL_1,      /* At least once */
    QOS_LEVEL_2       /* Exactly once */
} qos_level_t;

/* Message format for internal communication */
typedef struct {
    char *message_id;        /* Unique message identifier */
    char *timestamp;         /* ISO8601 timestamp */
    char *source;           /* Source device/client ID */
    char *destination;      /* Destination topic/endpoint */
    uint8_t *payload;       /* Message payload */
    size_t payload_len;     /* Payload length */
    
    struct {
        qos_level_t qos;           /* Quality of Service */
        protocol_type_t protocol;  /* Source protocol */
        char *content_type;        /* MIME content type */
        bool retain;               /* Retain flag */
        uint32_t ttl;             /* Time to live (seconds) */
    } metadata;
} message_t;

/* Session lifecycle states */
typedef enum {
    SESSION_STATE_INIT = 0,
    SESSION_STATE_CONNECTING,
    SESSION_STATE_ACTIVE,
    SESSION_STATE_IDLE,
    SESSION_STATE_TERMINATED
} session_state_t;

/* Device capabilities */
typedef struct {
    bool supports_qos0;
    bool supports_qos1;
    bool supports_qos2;
    bool supports_retain;
    bool supports_wildcards;
    uint32_t max_packet_size;
    uint16_t max_topic_alias;
} device_capabilities_t;

/* Connection information */
typedef struct {
    char *client_id;
    char *remote_addr;
    uint16_t remote_port;
    protocol_type_t protocol;
    session_state_t state;
    device_capabilities_t capabilities;
    uint64_t connect_time;
    uint64_t last_activity;
} connection_info_t;

/* Buffer management */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    size_t head;
    size_t tail;
    bool full;
} buffer_t;

/* Statistics and metrics */
typedef struct {
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t errors;
    uint32_t active_connections;
    double cpu_usage;
    size_t memory_usage;
} system_stats_t;

/* Configuration structures forward declarations */
typedef struct paumiot_config paumiot_config_t;
typedef struct smp_config smp_config_t;
typedef struct pdl_config pdl_config_t;
typedef struct pal_config pal_config_t;
typedef struct bll_config bll_config_t;
typedef struct dal_config dal_config_t;

/* Context structures forward declarations */
typedef struct smp_context smp_context_t;
typedef struct pdl_context pdl_context_t;
typedef struct pal_context pal_context_t;
typedef struct bll_context bll_context_t;
typedef struct dal_context dal_context_t;

/* Function pointer types */
typedef void (*message_handler_t)(const message_t *msg, void *user_data);
typedef void (*error_handler_t)(int error_code, const char *error_msg, void *user_data);
typedef void (*event_handler_t)(int event_type, void *event_data, void *user_data);

/* Helper macros */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Memory management helpers */
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_COMMON_TYPES_H */