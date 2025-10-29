# PaumIoT Middleware

**Low-Level IoT Middleware for Heterogeneous Protocol Environments**

PaumIoT is a high-performance, protocol-agnostic IoT middleware designed from the ground up to enable seamless communication between devices using different protocols (MQTT, CoAP, HTTP, etc.) while maintaining efficiency and scalability.

---

## 🚀 Quick Start - Implementation Guide

### 📖 Start Here!

This project is being built using an **iterative, test-driven approach**. Follow these documents in order:

1. **[GETTING_STARTED.md](docs/GETTING_STARTED.md)** ← **START HERE**
   - Prerequisites and setup
   - Your first task (60 minutes)
   - Complete walkthrough of Phase 1, Day 1

2. **[DETAILED_IMPLEMENTATION_PLAN.md](docs/DETAILED_IMPLEMENTATION_PLAN.md)**
   - Full implementation plan with Implement→Test→Finalize→Confirm methodology
   - Mini-step breakdowns for every component
   - Test cases and validation commands

3. **[ROADMAP_VISUAL.md](docs/ROADMAP_VISUAL.md)**
   - Visual timeline and progress charts
   - Architecture dependency diagrams
   - Daily workflow templates

4. **[PROGRESS.md](PROGRESS.md)**
   - Track your implementation progress
   - Daily checklists and task completion
   - Quality metrics tracker

5. **[ARCHITECTURE.md](docs/ARCHITECTURE.md)**
   - Complete system architecture
   - Component descriptions
   - Data flows and optimizations

### 🎯 Implementation Methodology: ITFC

Every component follows this strict workflow:

```
┌─────────────┐
│  IMPLEMENT  │ → Write 50-200 lines of focused code
└──────┬──────┘
       │
       ▼
┌─────────────┐
│    TEST     │ → Write & run unit tests immediately
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  FINALIZE   │ → Fix bugs, refactor, optimize
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   CONFIRM   │ → Full test suite, no regressions
└─────────────┘
```

**Quality Gates**: All tests pass ✅ Memory leaks = 0 ✅ Compiler warnings = 0 ✅

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    CLIENT DEVICES                        │
│              (with PaumIoT Client Plugin)                │
└────────────────────┬────────────────────────────────────┘
                     │ Network (TCP/UDP/TLS/DTLS)
                     ▼
┌─────────────────────────────────────────────────────────┐
│                  PAUMIOT MIDDLEWARE                      │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │           INITIATOR LAYER                          │ │
│  │  Protocol Detection | Connection Mgmt | Routing   │ │
│  └──────────────────┬─────────────────────────────────┘ │
│                     ▼                                    │
│  ┌────────────────────────────────────────────────────┐ │
│  │     PROTOCOL ADAPTATION LAYER (PAL)                │ │
│  │  MQTT Adapter | CoAP Adapter | Future Protocols   │ │
│  └──────────────────┬─────────────────────────────────┘ │
│                     ▼                                    │
│  ┌────────────────────────────────────────────────────┐ │
│  │              ENGINE                                │ │
│  │  Request Processing | Traffic Mgmt | Orchestration│ │
│  └─────────┬──────────────────────┬───────────────────┘ │
│            │                      │                      │
│            ▼                      ▼                      │
│  ┌──────────────────┐  ┌───────────────────────────┐   │
│  │ SENSOR MANAGER   │  │  STATE MANAGEMENT         │   │
│  │                  │◄─┤                           │   │
│  └──────────────────┘  └───────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## 📦 Project Structure

```
PaumIoT/
├── client-plugin/              # Client-side installable plugin
│   ├── include/
│   │   └── paumiot_client.h    # Public client API
│   ├── src/
│   │   ├── client_api.c        # Subscribe/Publish/Unsubscribe APIs
│   │   ├── protocol_tagger.c   # Protocol identification
│   │   ├── connection.c        # Connection management
│   │   └── buffer.c            # Client-side buffering
│   ├── tests/
│   └── examples/
│
├── middleware/                  # Core middleware components
│   ├── include/
│   │   ├── paumiot_core.h      # Core middleware API
│   │   ├── initiator/
│   │   │   └── initiator.h
│   │   ├── pal/
│   │   │   └── pal.h
│   │   ├── engine/
│   │   │   └── engine.h
│   │   ├── sensor_manager/
│   │   │   └── sensor_manager.h
│   │   └── state/
│   │       └── state_management.h
│   │
│   ├── src/
│   │   ├── initiator/          # Entry point layer
│   │   ├── pal/                # Protocol adapters
│   │   │   └── adapters/
│   │   │       ├── mqtt/
│   │   │       └── coap/
│   │   ├── engine/             # Core processing
│   │   ├── sensor_manager/     # Sensor data management
│   │   └── state/              # State storage
│   │
│   └── tests/
│       ├── unit/
│       ├── integration/
│       └── performance/
│
├── common/                      # Shared utilities
│   ├── include/
│   └── src/
│
├── config/                      # Configuration files
├── docs/                        # Documentation
│   ├── ARCHITECTURE.md
│   └── PROJECT_STRUCTURE.md
├── scripts/                     # Build scripts
└── examples/                    # Example applications
```

## 🚀 Key Features

### Client Plugin
- **Protocol-Agnostic API**: Simple pub/sub interface regardless of underlying protocol
- **Automatic Protocol Tagging**: Seamlessly identify and route packets
- **Lightweight**: Minimal footprint for resource-constrained devices
- **Async & Sync Operations**: Flexible API for different use cases
- **Auto-Reconnect**: Automatic reconnection with exponential backoff

### Middleware Layers

#### 1. Initiator Layer
- **Fast Protocol Detection**: Sub-millisecond protocol identification
- **Connection Pooling**: Efficient connection management
- **Load Balancing**: Distribute load across worker threads
- **Rate Limiting**: Per-client and global rate limits
- **DDoS Protection**: Built-in protection mechanisms

#### 2. Protocol Adaptation Layer (PAL)
- **Multi-Protocol Support**: MQTT v3.1.1/v5.0, CoAP RFC 7252
- **Zero-Copy Parsing**: High-performance packet processing
- **Protocol State Management**: Maintain protocol-specific state
- **Extensible**: Easy addition of new protocol adapters

#### 3. Engine
- **Request Orchestration**: Coordinate all middleware operations
- **Traffic Management**: Priority queues, rate limiting, backpressure
- **Pub/Sub Routing**: Efficient topic matching and message delivery
- **Business Logic**: Authorization, transformation, validation

#### 4. Sensor Manager
- **Sensor Registry**: Centralized sensor catalog
- **Data Caching**: LRU cache with configurable TTL
- **Health Monitoring**: Track sensor status and availability
- **Historical Data**: Time-series data storage and queries

#### 5. State Management
- **Session Tracking**: Manage client sessions and connections
- **Subscription Management**: Track active subscriptions
- **Protocol State**: Maintain packet IDs, message IDs, etc.
- **Persistence**: Write-ahead log for durability
- **Distributed**: Redis backend for horizontal scaling

## 🛠️ Build & Installation

### Prerequisites
```bash
# Required
- CMake 3.15+
- C99 compiler (GCC/Clang)
- pthread

# Optional
- Redis (for distributed state)
- OpenSSL (for TLS/DTLS)
```

### Build Middleware
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Build Client Plugin
```bash
cd client-plugin
mkdir build && cd build
cmake ..
make
sudo make install  # Installs libpaumiot_client.so
```

### Run Tests
```bash
make test
# or
ctest --output-on-failure
```

## 📚 Usage Examples

### Client Application (C)

```c
#include <paumiot_client.h>

void on_message(paumiot_client_t *client, const paumiot_message_t *msg, void *userdata) {
    printf("Received: %s -> %.*s\n", msg->topic, (int)msg->payload_len, msg->payload);
}

int main() {
    // Configure client
    paumiot_client_config_t config;
    paumiot_client_config_init(&config);
    config.host = "localhost";
    config.port = 1883;
    config.protocol = PAUMIOT_PROTOCOL_MQTT;
    config.client_id = "my_client";
    
    // Create and connect
    paumiot_client_t *client = paumiot_client_create(&config);
    paumiot_client_connect(client);
    
    // Subscribe
    paumiot_client_subscribe(client, "sensors/temperature", 
                            PAUMIOT_QOS_1, on_message, NULL);
    
    // Publish
    const char *payload = "{\"temp\": 23.5}";
    paumiot_client_publish(client, "sensors/temperature",
                          (uint8_t*)payload, strlen(payload),
                          PAUMIOT_QOS_1, false);
    
    // Event loop
    paumiot_client_loop_start(client);
    
    // ... application logic ...
    
    paumiot_client_loop_stop(client);
    paumiot_client_disconnect(client);
    paumiot_client_destroy(client);
    return 0;
}
```

### Running the Middleware

```bash
# Using config file
./build/paumiot_middleware --config config/default.yaml

# With environment variables
export PAUMIOT_MQTT_PORT=1883
export PAUMIOT_COAP_PORT=5683
./build/paumiot_middleware
```

### Configuration Example (config/default.yaml)

```yaml
server:
  host: "0.0.0.0"
  mqtt_port: 1883
  coap_port: 5683
  io_threads: 2
  worker_threads: 4

engine:
  max_queue_size: 10000
  request_timeout_ms: 5000
  rate_limit_window_ms: 1000
  max_burst_size: 100

state:
  backend: "memory"  # or "redis" or "persistent"
  persistence: true
  db_path: "/var/lib/paumiot/state.db"

sensor_manager:
  cache_size: 1000
  cache_ttl_ms: 60000
  enable_historical: true

logging:
  level: "info"
  format: "json"
  file: "/var/log/paumiot/middleware.log"
```

## 🔍 Architecture Deep Dive

### Data Flow: Publish Operation

```
Client → Plugin (Tag Protocol) → Network
    ↓
Initiator (Detect Protocol) → PAL (Decode MQTT/CoAP)
    ↓
Engine (Process Request) → State (Match Subscriptions)
    ↓
Engine (Build Responses) → PAL (Encode)
    ↓
Initiator (Route) → Network → Subscribers
```

### Protocol Detection Strategy

1. **Plugin Metadata** (Fastest): Explicit protocol tag from client
2. **First-Byte Analysis**: 
   - MQTT: `0x10-0xF0` (packet type in upper nibble)
   - CoAP: `0x40-0x7F` (version 1, type 0-3)
3. **Port-Based Heuristics** (Fallback)

### Performance Optimizations

- **Zero-Copy**: Direct buffer passing between layers
- **Object Pools**: Pre-allocated structures to avoid malloc/free overhead
- **Lock-Free Queues**: Minimize contention in multi-threaded environment
- **Radix Tree**: Fast topic matching for subscriptions
- **Arena Allocators**: Bulk allocation for related objects

## 📊 Monitoring

### Metrics Exposed

- Request rates (pub/sub ops per second)
- Latency percentiles (P50, P95, P99)
- Active sessions and subscriptions
- Protocol error rates
- Resource usage (CPU, memory, network)

### Health Endpoints

```bash
# Get middleware stats
curl http://localhost:8080/stats

# Get sensor manager status
curl http://localhost:8080/sensors/status

# Get active sessions
curl http://localhost:8080/sessions
```

## 🔒 Security

- **TLS/DTLS**: Encrypted transport for all protocols
- **Client Authentication**: Certificate-based or token-based
- **ACL**: Topic-based access control
- **Rate Limiting**: Prevent abuse and DDoS

## 🧪 Testing

### Unit Tests
```bash
./build/middleware/tests/unit/unit_tests
```

### Integration Tests
```bash
./build/middleware/tests/integration/integration_tests
```

### Performance Tests
```bash
./scripts/benchmark.sh
```

## 📖 Documentation

- [Architecture](docs/ARCHITECTURE.md) - Detailed system architecture
- [Project Structure](docs/PROJECT_STRUCTURE.md) - Code organization
- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Protocol Specs](docs/PROTOCOL_SPECS.md) - Protocol implementation details

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📄 License

[MIT License](LICENSE)

## 🙏 Acknowledgments

Built with inspiration from MQTT brokers (Mosquitto, EMQX) and CoAP implementations (libcoap), designed specifically for low-level network efficiency and protocol heterogeneity.

---

**Status**: 🚧 Active Development | **Version**: 1.0.0 | **Last Updated**: October 2025
