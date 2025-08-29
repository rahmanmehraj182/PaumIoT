# PaumIoT - Protocol-Agnostic Universal Middleware for IoT

## Overview

PaumIoT is a high-performance, event-driven middleware server that automatically detects and handles multiple IoT protocols including HTTP, MQTT, CoAP, DNS, TLS, and QUIC. It features advanced congestion control, session management, and **enhanced real-time protocol detection** using libpcap for packet capture and analysis.

## Features

- **Multi-Protocol Support**: HTTP, MQTT, CoAP, DNS, TLS, QUIC
- **Enhanced Protocol Detection**: libpcap-based packet capture with state tracking
- **Event-Driven Architecture**: Uses epoll for high-performance I/O
- **Automatic Protocol Detection**: Intelligent packet analysis with confidence scoring
- **TCP Connection State Tracking**: Remembers protocols across packet streams
- **Congestion Control**: TCP-inspired flow control for all protocols
- **Session Management**: State tracking and cleanup
- **Non-blocking I/O**: Handles thousands of concurrent connections
- **Real-time Statistics**: Connection and protocol monitoring with confidence metrics

## Enhanced Protocol Detection

The system now includes a sophisticated protocol detection engine that:

- **Captures live network packets** using libpcap on the "any" interface
- **Tracks TCP connection states** to maintain protocol context across packets
- **Provides confidence scoring** for each protocol detection (High/Medium/Low)
- **Handles packet fragmentation** and multi-packet protocols
- **Supports both TCP and UDP** protocols with appropriate detection strategies
- **Maintains detection statistics** including success rates and confidence distribution

### Detection Capabilities

| Protocol | Transport | Detection Method                        | Confidence |
| -------- | --------- | --------------------------------------- | ---------- |
| HTTP     | TCP       | Method/header pattern matching          | High       |
| MQTT     | TCP       | Message type and structure validation   | High       |
| CoAP     | UDP       | Version and code validation             | Medium     |
| DNS      | TCP/UDP   | Header structure and query validation   | High       |
| TLS      | TCP       | Record header and handshake validation  | High       |
| QUIC     | UDP       | Version and header structure validation | Medium     |

## Architecture

```
PaumIoT/
├── include/
│   ├── core/
│   │   ├── types.h           # data structures and constants
│   │   └── server_core.h     # Main server interface
│   ├── protocol/
│   │   ├── protocol_detector.h   # protocol detection engine
│   │   └── protocol_handlers.h   # Protocol message handlers
│   ├── network/
│   │   └── network_manager.h     # Network I/O and connection management
│   └── session/
│       └── session_manager.h     # Session state management
├── src/
│   ├── core/
│   │   └── server_core.c
│   ├── protocol/
│   │   ├── protocol_detector.c   # Enhanced with libpcap integration
│   │   └── protocol_handlers.c   # Added TLS/QUIC handlers
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
- **libpcap development library** (for enhanced packet capture)

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install libpcap-dev

# CentOS/RHEL/Fedora
sudo yum install libpcap-devel

# macOS
brew install libpcap
```

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
# Run with enhanced protocol detection
./paumiot
```

The server listens on port 8080 for both TCP and UDP connections and captures packets on all interfaces for protocol analysis.

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

#### TLS/HTTPS

```bash
curl -k https://localhost:8080/test
```

## Enhanced Protocol Detection

The system uses a multi-stage detection process:

1. **Packet Capture**: Live capture using libpcap on "any" interface
2. **State Tracking**: TCP connection state table for protocol persistence
3. **Enhanced Analysis**: Detailed packet structure validation
4. **Confidence Scoring**: Weighted scoring based on packet quality
5. **Protocol Prioritization**: TLS detection first (can encapsulate others)

### Detection Rules

- **HTTP**: Extended method list + header patterns + mid-stream detection
- **DNS**: Enhanced header validation + TCP/UDP support + query validation
- **MQTT**: Message type validation + remaining length encoding + protocol name check
- **CoAP**: Version field + token length + comprehensive code validation
- **TLS**: Record header validation + version checking + handshake type validation
- **QUIC**: Long/short header detection + version matching + connection ID parsing

### State Management

- **TCP Connections**: Hash table with timeout-based cleanup
- **Protocol Persistence**: Remember detected protocols across packets
- **Confidence Tracking**: Maintain detection confidence levels
- **Statistics**: Real-time metrics on detection success rates

## Session Management

Each connection maintains:

- Unique session ID
- Protocol-specific state data
- Activity timestamps
- Message counters
- Error tracking
- **Detection confidence and attempts**

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
#define STATE_TABLE_SIZE 1024   // TCP connection state table size
#define CONNECTION_TIMEOUT 300  // Connection timeout (5 minutes)
```

## Enhanced Monitoring

The server provides comprehensive real-time statistics:

- Active connection count
- Protocol distribution with confidence levels
- Bytes processed
- Congestion control status
- Session state information
- **Detection confidence distribution**
- **TCP connection tracking statistics**
- **UDP packet processing metrics**

Statistics are printed every 100 packets, or can be triggered by sending SIGUSR1.

## Error Handling

- Graceful connection cleanup
- Protocol validation errors
- Resource exhaustion protection
- Signal-based shutdown (SIGINT/SIGTERM)
- **Packet capture error recovery**
- **State table corruption protection**

## Performance

Optimizations included:

- Event-driven architecture (epoll)
- Non-blocking I/O
- Cache-aligned data structures
- Efficient protocol detection
- Memory pool management
- **libpcap integration for high-performance packet capture**
- **Hash table-based state tracking**
- **Confidence-based detection optimization**

## Contributing

When adding new protocols:

1. Add detection logic to `protocol_detector.c`
2. Implement message handler in `protocol_handlers.c`
3. Update protocol enumeration in `types.h`
4. Add test cases
5. **Update confidence scoring if needed**

## License

This project is part of the PaumIoT research initiative. See LICENSE file for details.

## Troubleshooting

### Common Issues

1. **Port already in use**: Change SERVER_PORT or kill existing processes
2. **Permission denied**: Run with appropriate privileges or use port > 1024
3. **Protocol not detected**: Check packet format and detection rules
4. **libpcap not found**: Install libpcap development package
5. **Packet capture fails**: Run with root privileges or configure capabilities

### Debug Mode

Build with debug symbols and enable verbose logging:

```bash
make debug
./paumiot
```

### Testing

Run the enhanced test suite:

```bash
make test
```

This performs comprehensive connectivity tests for all supported protocols including the new TLS and QUIC detection.

## Advanced Features

### Packet Capture Integration

The enhanced system integrates libpcap for:

- **Live packet capture** on all network interfaces
- **Real-time protocol analysis** without affecting application traffic
- **State tracking** across packet boundaries
- **Confidence scoring** based on packet quality
- **Statistics collection** for detection accuracy

### Protocol State Tracking

TCP connections are tracked using:

- **Hash table** for efficient lookup
- **Timeout-based cleanup** to prevent memory leaks
- **Protocol persistence** across multiple packets
- **Confidence scoring** for detection reliability
- **Statistics tracking** for performance monitoring
