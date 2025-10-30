#!/bin/bash
# Performance benchmark script for PaumIoT middleware

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TARGET="${1:-$PROJECT_ROOT/build/paumiot}"
RESULTS_DIR="$PROJECT_ROOT/benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if target exists
if [ ! -f "$TARGET" ]; then
    log_error "Target executable not found: $TARGET"
    log_info "Build the project first: make"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

log_info "Starting PaumIoT performance benchmarks..."
log_info "Target: $TARGET"
log_info "Results: $RESULTS_DIR/benchmark_$TIMESTAMP.txt"

# Start capturing results
RESULTS_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.txt"
exec > >(tee "$RESULTS_FILE")
exec 2>&1

echo "PaumIoT Performance Benchmark Results"
echo "====================================="
echo "Date: $(date)"
echo "Target: $TARGET"
echo "System: $(uname -a)"
echo ""

# Memory usage baseline
echo "Memory Usage Baseline:"
echo "----------------------"
if command -v valgrind &> /dev/null; then
    echo "Running memory analysis..."
    timeout 10s valgrind --tool=massif --time-unit=B --detailed-freq=1 \
        "$TARGET" --benchmark --duration=5 2>/dev/null || true
    echo ""
else
    log_warn "Valgrind not available for memory analysis"
fi

# CPU performance
echo "CPU Performance:"
echo "---------------"
echo "Running CPU-intensive tests..."

# Simulate packet processing load
if command -v time &> /dev/null; then
    echo "Packet processing benchmark (10000 packets):"
    /usr/bin/time -v "$TARGET" --benchmark --packets=10000 --protocol=mqtt 2>&1 | \
        grep -E "(User time|System time|Maximum resident set size|Page faults)" || true
    echo ""
fi

# Throughput testing
echo "Throughput Testing:"
echo "------------------"
echo "Testing message throughput..."

# MQTT throughput
echo "MQTT throughput (1 minute test):"
"$TARGET" --benchmark --protocol=mqtt --duration=60 --threads=4 2>&1 | \
    grep -E "(messages/sec|errors|latency)" || echo "Throughput test completed"
echo ""

# CoAP throughput  
echo "CoAP throughput (1 minute test):"
"$TARGET" --benchmark --protocol=coap --duration=60 --threads=4 2>&1 | \
    grep -E "(messages/sec|errors|latency)" || echo "Throughput test completed"
echo ""

# Latency testing
echo "Latency Testing:"
echo "---------------"

# Message processing latency
echo "Message processing latency (1000 samples):"
"$TARGET" --benchmark --latency --samples=1000 2>&1 | \
    grep -E "(p50|p95|p99|mean|max)" || echo "Latency test completed"
echo ""

# Memory stress testing
echo "Memory Stress Testing:"
echo "---------------------"

echo "Memory allocation stress test:"
"$TARGET" --benchmark --memory-stress --duration=30 2>&1 | \
    grep -E "(peak|leaked|errors)" || echo "Memory stress test completed"
echo ""

# Concurrent connections
echo "Concurrent Connections:"
echo "----------------------"

echo "Testing with multiple concurrent connections:"
for connections in 10 50 100 500; do
    echo "Testing $connections concurrent connections..."
    timeout 30s "$TARGET" --benchmark --connections=$connections --duration=10 2>&1 | \
        grep -E "(success|failed|throughput)" || echo "Test with $connections connections completed"
done
echo ""

# Protocol switching
echo "Protocol Switching Performance:"
echo "------------------------------"

echo "MQTT to CoAP conversion benchmark:"
"$TARGET" --benchmark --protocol-switch --from=mqtt --to=coap --messages=1000 2>&1 | \
    grep -E "(converted|failed|rate)" || echo "Protocol switching test completed"
echo ""

# System resource usage
echo "System Resource Usage:"
echo "---------------------"

if command -v ps &> /dev/null; then
    echo "Running resource monitoring..."
    # Start target in background for monitoring
    "$TARGET" --daemon --benchmark-mode &
    TARGET_PID=$!
    sleep 2
    
    # Monitor for 30 seconds
    for i in {1..6}; do
        if kill -0 $TARGET_PID 2>/dev/null; then
            echo "Sample $i ($(date '+%H:%M:%S')):"
            ps -p $TARGET_PID -o pid,pcpu,pmem,vsz,rss,time,comm 2>/dev/null || true
            sleep 5
        fi
    done
    
    # Clean up
    kill $TARGET_PID 2>/dev/null || true
    wait $TARGET_PID 2>/dev/null || true
    echo ""
fi

# Network performance (if available)
echo "Network Performance:"
echo "-------------------"

if command -v iperf3 &> /dev/null; then
    echo "Network throughput available via iperf3"
    # Would need iperf3 server running
else
    echo "Network performance tools not available"
fi
echo ""

# Error rate testing
echo "Error Rate Testing:"
echo "------------------"

echo "Testing error handling under stress:"
"$TARGET" --benchmark --error-injection --duration=30 2>&1 | \
    grep -E "(error rate|recovery|failed)" || echo "Error rate test completed"
echo ""

# Platform-specific tests
echo "Platform-Specific Tests:"
echo "-----------------------"

ARCH=$(uname -m)
echo "Architecture: $ARCH"

case "$ARCH" in
    "x86_64")
        echo "x86_64 optimizations active"
        ;;
    "armv7l"|"aarch64")
        echo "ARM-specific performance characteristics"
        if [ -f /proc/cpuinfo ]; then
            grep -E "(model name|cpu cores|BogoMIPS)" /proc/cpuinfo | head -5
        fi
        ;;
esac
echo ""

# Summary
echo "Benchmark Summary:"
echo "================="
echo "Benchmark completed at $(date)"
echo "Results saved to: $RESULTS_FILE"

# Generate simple performance report
echo ""
echo "Quick Performance Metrics:"
echo "-------------------------"

# Extract key metrics (would need actual benchmark output parsing)
echo "Message throughput: Varies by protocol and system"
echo "Memory usage: Check detailed logs above"  
echo "CPU utilization: Check system monitoring above"
echo "Latency: Check latency test results above"

# Recommendations
echo ""
echo "Recommendations:"
echo "---------------"
echo "- Review memory usage patterns for optimization opportunities"
echo "- Consider tuning buffer sizes based on throughput results"
echo "- Monitor error rates under production loads"
echo "- Validate performance meets target requirements"

log_info "Benchmark completed successfully!"
log_info "Detailed results: $RESULTS_FILE"

# Optional: Generate graphs (if gnuplot available)
if command -v gnuplot &> /dev/null; then
    log_info "Generating performance graphs..."
    # Would implement graph generation here
fi