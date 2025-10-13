# PaumIoT Development Guide

## Project Structure

```
PaumIoT/
├── src/                        # Source code
│   ├── smp/                    # State Management Plane
│   │   ├── protocol_state_table/
│   │   ├── connection_registry/
│   │   ├── qos_metrics/
│   │   ├── health_monitor/
│   │   └── config_manager/
│   ├── pdl/                    # Protocol Demultiplexing Layer
│   │   ├── packet_classifier/
│   │   ├── message_broker/
│   │   ├── protocol_router/
│   │   └── buffer_management/
│   ├── pal/                    # Protocol Adaptation Layer
│   │   ├── adapters/
│   │   │   ├── mqtt/
│   │   │   ├── coap/
│   │   │   └── custom/
│   │   ├── codecs/
│   │   └── state_machine/
│   ├── bll/                    # Business Logic Layer
│   │   ├── request_handler/
│   │   ├── command_processor/
│   │   ├── response_generator/
│   │   ├── data_aggregator/
│   │   └── cache_manager/
│   └── dal/                    # Data Acquisition Layer
│       ├── sensor_interface/
│       ├── data_collection/
│       ├── database/
│       ├── log_manager/
│       └── device_registry/
├── include/                    # Header files
│   ├── common/                 # Common definitions
│   ├── smp/, pdl/, pal/, bll/, dal/  # Layer headers
│   └── paumiot.h              # Main API header
├── tests/                      # Test suites
│   ├── unit/                   # Unit tests
│   ├── integration/            # Integration tests
│   ├── protocol/               # Protocol-specific tests
│   ├── performance/            # Performance benchmarks
│   └── mocks/                  # Test mocks
├── config/                     # Configuration files
├── docs/                       # Documentation
├── examples/                   # Example applications
├── scripts/                    # Build and utility scripts
└── build/                      # Build artifacts
```

## Architecture Overview

### Layered Design Principles

1. **Separation of Concerns**: Each layer has a specific responsibility
2. **Modular Communication**: Layers communicate via well-defined APIs
3. **Protocol Agnostic**: Core layers independent of specific protocols
4. **Resource Efficient**: Optimized for embedded environments
5. **Extensible**: Plugin architecture for new protocols

### Data Flow

```
Sensors → DAL → BLL → PAL → PDL → Network
Network → PDL → PAL → BLL → DAL → Storage
```

### State Management

The SMP provides horizontal coordination across all layers:
- Session lifecycle tracking
- Resource monitoring
- Configuration management
- Health checking
- Metrics collection

## Layer Implementation Guide

### State Management Plane (SMP)

**Purpose**: System-wide orchestration and monitoring

**Key Components**:
- **Protocol State Table**: Tracks connection states and metadata
- **Connection Registry**: Device inventory and capabilities
- **QoS Metrics Collector**: Performance monitoring
- **System Health Monitor**: Resource utilization tracking
- **Configuration Manager**: Runtime parameter management

**Implementation Notes**:
- Use FreeRTOS queues for cross-layer communication
- Implement eventual consistency for distributed deployments
- Heartbeat mechanisms for liveness detection

### Protocol Demultiplexing Layer (PDL)

**Purpose**: Network packet routing and classification

**Key Components**:
- **Packet Classifier**: Deep packet inspection (DPI) for protocol detection
- **Protocol Detection Engine**: Heuristic-based protocol identification
- **Message Broker**: Topic-based routing with delivery guarantees
- **Protocol Router**: Load balancing and priority queuing
- **Buffer Management**: Ingress/egress with flow control

**Implementation Notes**:
- Port-based classification (MQTT: 1883/8883, CoAP: 5683/5684)
- Pattern matching for protocol handshakes
- Circular buffers for efficient memory usage

### Protocol Adaptation Layer (PAL)

**Purpose**: Protocol translation and normalization

**Key Components**:
- **MQTT Adapter**: Full MQTT v5.0 support with QoS handling
- **CoAP Adapter**: RFC 7252 compliant with observe and blockwise
- **Custom Adapters**: Framework for new protocol integration
- **Packet Codecs**: Serialization (Protocol Buffers, JSON, CBOR)
- **State Machines**: Per-connection state management

**Uniform Adapter Interface**:
```c
typedef struct {
    paumiot_result_t (*decode)(const uint8_t *packet, size_t len, message_t **msg);
    paumiot_result_t (*encode)(const message_t *msg, uint8_t *buf, size_t *len);
    protocol_type_t (*getProtocolType)(void);
    uint32_t (*getCapabilities)(void);
    paumiot_result_t (*handleControl)(const char *cmd);
} protocol_adapter_t;
```

### Business Logic Layer (BLL)

**Purpose**: Application processing and orchestration

**Key Components**:
- **Request Handler**: Command validation and access control
- **Command Processor**: Workflow orchestration and execution
- **Response Generator**: Reply formatting with error handling
- **Data Aggregator**: Temporal/spatial aggregation and statistics
- **Cache Manager**: In-memory caching with TTL

**Processing Patterns**:
- Request-response for synchronous operations
- Stream processing for continuous data flows
- Event-driven for threshold-based actions

### Data Acquisition Layer (DAL)

**Purpose**: Physical interface and data persistence

**Key Components**:
- **Sensor Interface Manager**: HAL for I2C/SPI/UART protocols
- **Data Collection Service**: Sampling, validation, and buffering
- **Time-Series Database**: Optimized storage with compression
- **Log Manager**: Structured logging with rotation
- **Device Registry**: Metadata and lifecycle management

## Protocol Implementation Details

### MQTT Adapter

**Supported Features**:
- MQTT v3.1.1 and v5.0
- All QoS levels (0, 1, 2)
- Session persistence
- Retain messages
- Will messages
- Topic aliases (v5.0)

**Packet Structure Handling**:
```c
// Fixed header: Type/Flags + Remaining Length (variable)
// Variable header: Protocol-specific fields
// Payload: Message content
```

**State Machine**:
```
INIT → CONNECTING → CONNECTED → [IDLE] → DISCONNECTED
```

### CoAP Adapter

**Supported Features**:
- RFC 7252 core protocol
- Confirmable/Non-confirmable messages
- Observe pattern (RFC 7641)
- Block-wise transfers (RFC 7959)
- Core options (Uri-Path, Content-Format, etc.)

**Message Structure**:
```c
// 4-byte header: Ver|T|TKL|Code|Message ID
// Token: Variable length (0-8 bytes)
// Options: Delta-encoded TLV format
// Payload: Optional with 0xFF marker
```

## Memory Management

### Static Allocation Strategy

For embedded environments, use static allocation wherever possible:

```c
#define MAX_CONNECTIONS 100
#define MAX_MESSAGES 1000

static connection_info_t connections[MAX_CONNECTIONS];
static message_t message_pool[MAX_MESSAGES];
static uint8_t buffer_pool[BUFFER_POOL_SIZE];
```

### Memory Pools

Implement memory pools for frequent allocations:

```c
typedef struct {
    void *pool;
    size_t block_size;
    size_t num_blocks;
    uint32_t free_mask;
} memory_pool_t;
```

## Threading Model

### FreeRTOS Integration

```c
// Layer tasks with priorities
xTaskCreate(pdl_task, "PDL", 2048, NULL, configMAX_PRIORITIES-1, NULL);
xTaskCreate(pal_task, "PAL", 2048, NULL, configMAX_PRIORITIES-2, NULL);
xTaskCreate(bll_task, "BLL", 2048, NULL, configMAX_PRIORITIES-3, NULL);
xTaskCreate(dal_task, "DAL", 1024, NULL, configMAX_PRIORITIES-4, NULL);
```

### Synchronization Primitives

- **Queues**: Inter-layer message passing
- **Semaphores**: Resource protection
- **Mutexes**: Critical section protection
- **Event Groups**: State synchronization

## Security Implementation

### TLS/DTLS Integration

```c
#ifdef USE_MBEDTLS
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
} tls_context_t;
#endif
```

### Authentication Methods

1. **Certificate-based**: X.509 certificates with PKI
2. **Pre-Shared Key**: Symmetric key authentication
3. **Username/Password**: MQTT-style authentication

## Testing Strategy

### Unit Tests

Focus on individual functions and modules:
```c
void test_mqtt_decode(void) {
    uint8_t packet[] = {0x30, 0x0C, ...}; // PUBLISH packet
    message_t *msg = NULL;
    
    paumiot_result_t result = mqtt_decode(packet, sizeof(packet), &msg);
    ASSERT_SUCCESS(result);
    ASSERT_STR_EQ("test/topic", msg->destination);
    
    message_free(msg);
}
```

### Integration Tests

Test layer interactions:
```c
void test_mqtt_to_coap_conversion(void) {
    // Decode MQTT → Internal Format → Encode CoAP
}
```

### Protocol Tests

Use real protocol clients:
```bash
# MQTT testing
mosquitto_pub -t "test/topic" -m "Hello World"

# CoAP testing
coap-client -m get coap://localhost/test
```

## Performance Optimization

### Zero-Copy Operations

Minimize memory copies by using buffer references:
```c
typedef struct {
    const uint8_t *data;
    size_t len;
    size_t offset;
} buffer_view_t;
```

### Batch Processing

Process multiple messages together:
```c
void process_message_batch(message_t *messages[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        // Process messages in batch
    }
}
```

### CPU Optimization

- Use bit manipulation for flag operations
- Implement state machines with jump tables
- Optimize hot paths with assembly if needed

## Debugging and Monitoring

### Logging Levels

```c
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

#define LOG_DEBUG(fmt, ...) log_message(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
```

### Metrics Collection

Track key performance indicators:
- Messages processed per second
- Memory usage
- CPU utilization
- Error rates
- Latency percentiles

### Debug Interfaces

- Serial console for embedded targets
- Network interface for remote debugging
- Memory dump capabilities
- State inspection commands

## Deployment Considerations

### Configuration Management

Use hierarchical configuration:
1. Compile-time defaults
2. Configuration file
3. Environment variables
4. Runtime API calls

### Update Mechanisms

For embedded deployments:
- OTA update framework
- Rollback capabilities
- Integrity verification
- Incremental updates

### Monitoring Integration

- Prometheus metrics export
- Health check endpoints
- Log aggregation
- Alert integration

This guide provides the foundation for implementing and extending the PaumIoT middleware architecture.