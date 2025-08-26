# PaumIoT - Protocol-Agnostic Universal Middleware for IoT

## Overview

PaumIoT is a high-performance, event-driven middleware server that automatically detects and handles multiple IoT protocols including HTTP, MQTT, CoAP, and DNS. It features advanced congestion control, session management, and real-time protocol detection.

## Features

- **Multi-Protocol Support**: HTTP, MQTT, CoAP, DNS
- **Event-Driven Architecture**: Uses epoll for high-performance I/O
- **Automatic Protocol Detection**: Intelligent packet analysis
- **Congestion Control**: TCP-inspired flow control for all protocols
- **Session Management**: State tracking and cleanup
- **Non-blocking I/O**: Handles thousands of concurrent connections
- **Real-time Statistics**: Connection and protocol monitoring

## Architecture

```
PaumIoT/
├── include/
│   ├── core/
│   │   ├── types.h           # Common data structures and constants
│   │   └── server_core.h     # Main server interface
│   ├── protocol/
│   │   ├── protocol_detector.h   # Protocol detection engine
│   │   └── protocol_handlers.h   # Protocol message handlers
│   ├── network/
│   │   └── network_manager.h     # Network I/O and connection management
│   └── session/
│       └── session_manager.h     # Session state management
├── src/
│   ├── core/
│   │   └── server_core.c
│   ├── protocol/
│   │   ├── protocol_detector.c
│   │   └── protocol_handlers.c
│   ├── network/
│   │   └── network_manager.c
│   └── session/
│       └── session_manager.c
└── main.c                    # Application entry point
```

## Building

### Prerequisites

- GCC compiler with C99 support
- POSIX-compliant system (Linux, macOS, BSD)
- pthread library

### Compilation

```bash
# Build the project
make

# Build with debug symbols
make debug

# Build optimized release
make release

# Clean build files
make clean
```

## Usage

### Starting the Server

```bash
./paumiot
```

The server listens on port 8080 for both TCP and UDP connections.

### Testing Different Protocols

#### HTTP

```bash
curl http://localhost:8080/api/test
```

#### MQTT (requires mosquitto-clients)

```bash
mosquitto_pub -h localhost -p 8080 -t test/topic -m "hello world"
```

#### CoAP (requires coap-client)

```bash
coap-client -m get coap://localhost:8080/status
```

#### DNS (requires dig)

```bash
dig @localhost -p 8080 example.com
```

## Protocol Detection

The system uses a multi-stage detection process:

1. **Fast Detection**: Quick heuristics for common patterns
2. **Deep Analysis**: Detailed packet structure validation
3. **Scoring System**: Confidence-based protocol selection

### Detection Rules

- **HTTP**: Method keywords + HTTP version string + CRLF
- **DNS**: Valid header structure + question/answer counts
- **MQTT**: Message type validation + remaining length encoding
- **CoAP**: Version field + token length + code class validation

## Session Management

Each connection maintains:

- Unique session ID
- Protocol-specific state data
- Activity timestamps
- Message counters
- Error tracking

## Congestion Control

TCP-inspired congestion control for all protocols:

- **Slow Start**: Exponential window growth
- **Congestion Avoidance**: Linear growth after threshold
- **Rate Limiting**: Per-connection message limits
- **Queue Management**: Depth-based flow control

## Configuration

Key parameters in `include/core/types.h`:

```c
#define MAX_CLIENTS 10000       // Maximum concurrent connections
#define BUFFER_SIZE 4096        // I/O buffer size
#define SERVER_PORT 8080        // Server listening port
#define MAX_MSGS_PER_SEC 100    // Rate limit per connection
#define SLOW_START_THRESHOLD 64 // Congestion control threshold
```

## Monitoring

The server provides real-time statistics:

- Active connection count
- Protocol distribution
- Bytes processed
- Congestion control status
- Session state information

Statistics are printed every 60 seconds, or can be triggered by sending SIGUSR1.

## Error Handling

- Graceful connection cleanup
- Protocol validation errors
- Resource exhaustion protection
- Signal-based shutdown (SIGINT/SIGTERM)

## Performance

Optimizations included:

- Event-driven architecture (epoll)
- Non-blocking I/O
- Cache-aligned data structures
- Efficient protocol detection
- Memory pool management

## Contributing

When adding new protocols:

1. Add detection logic to `protocol_detector.c`
2. Implement message handler in `protocol_handlers.c`
3. Update protocol enumeration in `types.h`
4. Add test cases

## License

This project is part of the PaumIoT research initiative. See LICENSE file for details.

## Troubleshooting

### Common Issues

1. **Port already in use**: Change SERVER_PORT or kill existing processes
2. **Permission denied**: Run with appropriate privileges or use port > 1024
3. **Protocol not detected**: Check packet format and detection rules

### Debug Mode

Build with debug symbols and enable verbose logging:

```bash
make debug
./paumiot
```

### Testing

Run the test suite:

```bash
make test
```

This performs basic connectivity tests for all supported protocols.
