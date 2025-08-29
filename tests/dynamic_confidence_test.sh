#!/bin/bash

# Dynamic Confidence System Test Script
# Tests the new dynamic confidence calculation system

set -e

echo "=========================================="
echo "Dynamic Confidence System Test"
echo "=========================================="

# Build the project
echo "Building project..."
make clean
make

# Start server in background
echo "Starting server..."
./paumiot > dynamic_confidence.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 3

echo "Testing dynamic confidence system..."

# Test 1: HTTP with different packet qualities
echo "Test 1: HTTP packets with varying quality..."
echo "  - Perfect HTTP GET request"
printf "GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc -w1 localhost 8080

echo "  - HTTP with minimal headers"
printf "GET / HTTP/1.1\r\n\r\n" | nc -w1 localhost 8080

echo "  - HTTP with many headers"
printf "GET /test HTTP/1.1\r\nHost: localhost\r\nUser-Agent: TestClient\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nAccept: */*\r\n\r\n" | nc -w1 localhost 8080

# Test 2: MQTT with different packet types
echo "Test 2: MQTT packets with different types..."
echo "  - MQTT CONNECT (should have high confidence)"
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc -w1 localhost 8080

echo "  - MQTT PUBLISH (should have medium confidence)"
printf '\x30\x0A\x00\x05topic\x00\x01a' | nc -w1 localhost 8080

echo "  - MQTT PINGREQ (should have lower confidence)"
printf '\xC0\x00' | nc -w1 localhost 8080

# Test 3: CoAP with different structures
echo "Test 3: CoAP packets with different structures..."
echo "  - CoAP GET request"
printf '\x40\x01\x30\x39' | nc -u -w1 localhost 8080

echo "  - CoAP POST request"
printf '\x42\x02\x30\x39' | nc -u -w1 localhost 8080

# Test 4: DNS with different query types
echo "Test 4: DNS packets with different query types..."
echo "  - DNS A record query"
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc -w1 localhost 8080

echo "  - DNS MX record query"
printf '\x00\x02\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x0F\x00\x01' | nc -w1 localhost 8080

# Test 5: TLS with different content types
echo "Test 5: TLS packets with different content types..."
echo "  - TLS Client Hello"
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc -w1 localhost 8080

echo "  - TLS Application Data"
printf '\x17\x03\x01\x00\x10' | nc -w1 localhost 8080

# Test 6: QUIC packets
echo "Test 6: QUIC packets..."
echo "  - QUIC long header packet"
printf '\x80\x00\x00\x00\x00\x00\x00\x00\x00' | nc -u -w1 localhost 8080

# Wait for processing
sleep 2

# Stop server
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Analyze results
echo ""
echo "=========================================="
echo "Dynamic Confidence Analysis"
echo "=========================================="

echo "Server log analysis:"
echo ""

# Check for dynamic confidence features
echo "1. Checking for dynamic confidence features:"
if grep -q "entropy_score\|pattern_strength\|validation_depth" dynamic_confidence.log; then
    echo "  ✓ Dynamic confidence features detected"
else
    echo "  ✗ Dynamic confidence features not found"
fi

# Check for confidence calibration
echo ""
echo "2. Checking for confidence calibration:"
if grep -q "Calibration factor\|Average confidence error" dynamic_confidence.log; then
    echo "  ✓ Confidence calibration system active"
else
    echo "  ✗ Confidence calibration not found"
fi

# Check for protocol accuracy tracking
echo ""
echo "3. Checking for protocol accuracy tracking:"
if grep -q "Protocol Accuracy Statistics\|Accuracy=" dynamic_confidence.log; then
    echo "  ✓ Protocol accuracy tracking active"
else
    echo "  ✗ Protocol accuracy tracking not found"
fi

# Show confidence distribution
echo ""
echo "4. Confidence distribution analysis:"
echo "  High confidence detections:"
grep -c "confidence: [8-9][0-9]" dynamic_confidence.log 2>/dev/null || echo "    0"
echo "  Medium confidence detections:"
grep -c "confidence: [6-7][0-9]" dynamic_confidence.log 2>/dev/null || echo "    0"
echo "  Low confidence detections:"
grep -c "confidence: [3-5][0-9]" dynamic_confidence.log 2>/dev/null || echo "    0"

# Show sample detections with confidence
echo ""
echo "5. Sample detections with confidence scores:"
grep "Detected.*confidence:" dynamic_confidence.log | head -5

echo ""
echo "=========================================="
echo "Test Complete"
echo "=========================================="

# Cleanup
rm -f dynamic_confidence.log

echo "Dynamic confidence system test completed!"
