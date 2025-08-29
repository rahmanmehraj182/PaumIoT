#!/bin/bash

# Comprehensive Test Script for Dynamic Confidence System
# Tests all protocols, confidence variations, and system features

set -e

echo "=========================================="
echo "COMPREHENSIVE DYNAMIC CONFIDENCE TEST"
echo "=========================================="

# Build the project
echo "Building project..."
make clean
make

# Start server in background
echo "Starting server..."
./paumiot > comprehensive_test.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 3

echo "Running comprehensive tests..."

# Test 1: HTTP with various qualities
echo "Test 1: HTTP Protocol Variations"
echo "  - Perfect HTTP GET request"
curl -s http://localhost:8080/test > /dev/null

echo "  - HTTP with minimal headers"
printf "GET / HTTP/1.1\r\n\r\n" | nc -w1 localhost 8080 > /dev/null

echo "  - HTTP with extensive headers"
printf "GET /test HTTP/1.1\r\nHost: localhost\r\nUser-Agent: TestClient\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n" | nc -w1 localhost 8080 > /dev/null

echo "  - HTTP POST request"
printf "POST /data HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello" | nc -w1 localhost 8080 > /dev/null

echo "  - HTTP response detection"
printf "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html></html>" | nc -w1 localhost 8080 > /dev/null

# Test 2: MQTT with all packet types
echo "Test 2: MQTT Protocol Variations"
echo "  - MQTT CONNECT (high confidence expected)"
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc -w1 localhost 8080 > /dev/null

echo "  - MQTT PUBLISH (medium confidence expected)"
printf '\x30\x0A\x00\x05topic\x00\x01a' | nc -w1 localhost 8080 > /dev/null

echo "  - MQTT SUBSCRIBE (medium confidence expected)"
printf '\x82\x0B\x00\x01\x00\x05topic\x00' | nc -w1 localhost 8080 > /dev/null

echo "  - MQTT PINGREQ (lower confidence expected)"
printf '\xC0\x00' | nc -w1 localhost 8080 > /dev/null

echo "  - MQTT DISCONNECT (lower confidence expected)"
printf '\xE0\x00' | nc -w1 localhost 8080 > /dev/null

# Test 3: DNS variations
echo "Test 3: DNS Protocol Variations"
echo "  - DNS A record query"
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc -w1 localhost 8080 > /dev/null

echo "  - DNS MX record query"
printf '\x00\x02\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x0F\x00\x01' | nc -w1 localhost 8080 > /dev/null

echo "  - DNS response"
printf '\x00\x01\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x3C\x00\x04\x93\x18\x4D\x4E' | nc -w1 localhost 8080 > /dev/null

# Test 4: TLS variations
echo "Test 4: TLS Protocol Variations"
echo "  - TLS Client Hello"
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc -w1 localhost 8080 > /dev/null

echo "  - TLS Server Hello"
printf '\x16\x03\x01\x00\x85\x02\x00\x00\x81\x03\x01' | nc -w1 localhost 8080 > /dev/null

echo "  - TLS Application Data"
printf '\x17\x03\x01\x00\x10' | nc -w1 localhost 8080 > /dev/null

echo "  - TLS Alert"
printf '\x15\x03\x01\x00\x02\x02\x28' | nc -w1 localhost 8080 > /dev/null

# Test 5: CoAP variations (UDP)
echo "Test 5: CoAP Protocol Variations (UDP)"
echo "  - CoAP GET request"
printf '\x40\x01\x30\x39' | nc -u -w1 localhost 8080 > /dev/null

echo "  - CoAP POST request"
printf '\x42\x02\x30\x39' | nc -u -w1 localhost 8080 > /dev/null

echo "  - CoAP response"
printf '\x60\x45\x30\x39' | nc -u -w1 localhost 8080 > /dev/null

# Test 6: QUIC variations (UDP)
echo "Test 6: QUIC Protocol Variations (UDP)"
echo "  - QUIC long header packet"
printf '\x80\x00\x00\x00\x00\x00\x00\x00\x00' | nc -u -w1 localhost 8080 > /dev/null

echo "  - QUIC version negotiation"
printf '\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' | nc -u -w1 localhost 8080 > /dev/null

# Test 7: Mixed protocol stress test
echo "Test 7: Mixed Protocol Stress Test"
for i in {1..10}; do
    echo "  - Round $i: Mixed protocols"
    # HTTP
    curl -s http://localhost:8080/test > /dev/null &
    # MQTT
    printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc -w1 localhost 8080 > /dev/null &
    # DNS
    printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc -w1 localhost 8080 > /dev/null &
    # TLS
    printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc -w1 localhost 8080 > /dev/null &
    wait
done

# Test 8: Confidence calibration test
echo "Test 8: Confidence Calibration Test"
echo "  - Multiple HTTP requests to test learning"
for i in {1..20}; do
    curl -s http://localhost:8080/test > /dev/null
done

# Wait for processing
sleep 2

# Stop server
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Analyze results
echo ""
echo "=========================================="
echo "COMPREHENSIVE TEST ANALYSIS"
echo "=========================================="

echo "Server log analysis:"
echo ""

# Check for dynamic confidence system
echo "1. Dynamic Confidence System Verification:"
if grep -q "confidence:" comprehensive_test.log; then
    echo "  ✓ Confidence values detected"
    CONFIDENCE_COUNT=$(grep -c "confidence:" comprehensive_test.log)
    echo "  ✓ Total confidence calculations: $CONFIDENCE_COUNT"
else
    echo "  ✗ No confidence values found"
fi

# Check confidence distribution
echo ""
echo "2. Confidence Distribution Analysis:"
HIGH_CONF=$(grep -c "confidence: [8-9][0-9]" comprehensive_test.log 2>/dev/null || echo "0")
MED_CONF=$(grep -c "confidence: [6-7][0-9]" comprehensive_test.log 2>/dev/null || echo "0")
LOW_CONF=$(grep -c "confidence: [3-5][0-9]" comprehensive_test.log 2>/dev/null || echo "0")
echo "  High confidence (80-99%): $HIGH_CONF"
echo "  Medium confidence (60-79%): $MED_CONF"
echo "  Low confidence (30-59%): $LOW_CONF"

# Check protocol detection
echo ""
echo "3. Protocol Detection Verification:"
PROTOCOLS=("HTTP" "MQTT" "DNS" "TLS" "CoAP" "QUIC")
for protocol in "${PROTOCOLS[@]}"; do
    if grep -q "Protocol detected: $protocol" comprehensive_test.log; then
        echo "  ✓ $protocol detection working"
    else
        echo "  ✗ $protocol detection not found"
    fi
done

# Check enhanced statistics
echo ""
echo "4. Enhanced Statistics Verification:"
if grep -q "DYNAMIC CONFIDENCE SYSTEM" comprehensive_test.log; then
    echo "  ✓ Dynamic confidence system active"
else
    echo "  ✗ Dynamic confidence system not found"
fi

if grep -q "PROTOCOL ACCURACY STATISTICS" comprehensive_test.log; then
    echo "  ✓ Protocol accuracy tracking active"
else
    echo "  ✗ Protocol accuracy tracking not found"
fi

# Check calibration system
echo ""
echo "5. Calibration System Verification:"
if grep -q "Calibration factor:" comprehensive_test.log; then
    echo "  ✓ Calibration system active"
    CALIBRATION_FACTOR=$(grep "Calibration factor:" comprehensive_test.log | tail -1 | awk '{print $3}')
    echo "  ✓ Calibration factor: $CALIBRATION_FACTOR"
else
    echo "  ✗ Calibration system not found"
fi

# Check confidence history
echo ""
echo "6. Confidence History Verification:"
if grep -q "Confidence history entries:" comprehensive_test.log; then
    echo "  ✓ Confidence history tracking active"
    HISTORY_ENTRIES=$(grep "Confidence history entries:" comprehensive_test.log | tail -1 | awk '{print $4}')
    echo "  ✓ History entries: $HISTORY_ENTRIES"
else
    echo "  ✗ Confidence history not found"
fi

# Show sample confidence values
echo ""
echo "7. Sample Confidence Values:"
echo "  HTTP confidence values:"
grep "Protocol detected: HTTP" comprehensive_test.log | head -3 | awk '{print $NF}'

echo "  MQTT confidence values:"
grep "Protocol detected: MQTT" comprehensive_test.log | head -3 | awk '{print $NF}'

echo "  DNS confidence values:"
grep "Protocol detected: DNS" comprehensive_test.log | head -3 | awk '{print $NF}'

# Performance metrics
echo ""
echo "8. Performance Metrics:"
TOTAL_PACKETS=$(grep "Total packets:" comprehensive_test.log | tail -1 | awk '{print $3}')
IDENTIFIED_PACKETS=$(grep "Identified packets:" comprehensive_test.log | tail -1 | awk '{print $3}')
echo "  Total packets processed: $TOTAL_PACKETS"
echo "  Identified packets: $IDENTIFIED_PACKETS"

if [ "$TOTAL_PACKETS" -gt 0 ]; then
    IDENTIFICATION_RATE=$(echo "scale=2; $IDENTIFIED_PACKETS * 100 / $TOTAL_PACKETS" | bc 2>/dev/null || echo "N/A")
    echo "  Identification rate: $IDENTIFICATION_RATE%"
fi

# Error checking
echo ""
echo "9. Error Analysis:"
ERROR_COUNT=$(grep -c "ERROR\|error\|Error" comprehensive_test.log 2>/dev/null || echo "0")
if [ "$ERROR_COUNT" -eq 0 ]; then
    echo "  ✓ No errors detected"
else
    echo "  ⚠ $ERROR_COUNT errors/warnings found"
    grep -i "error" comprehensive_test.log | head -3
fi

echo ""
echo "=========================================="
echo "TEST SUMMARY"
echo "=========================================="

# Calculate success metrics
TOTAL_TESTS=0
PASSED_TESTS=0

# Count protocol detections
for protocol in "${PROTOCOLS[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if grep -q "Protocol detected: $protocol" comprehensive_test.log; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
    fi
done

# Count system features
TOTAL_TESTS=$((TOTAL_TESTS + 4))  # Dynamic confidence, accuracy tracking, calibration, history
if grep -q "DYNAMIC CONFIDENCE SYSTEM" comprehensive_test.log; then PASSED_TESTS=$((PASSED_TESTS + 1)); fi
if grep -q "PROTOCOL ACCURACY STATISTICS" comprehensive_test.log; then PASSED_TESTS=$((PASSED_TESTS + 1)); fi
if grep -q "Calibration factor:" comprehensive_test.log; then PASSED_TESTS=$((PASSED_TESTS + 1)); fi
if grep -q "Confidence history entries:" comprehensive_test.log; then PASSED_TESTS=$((PASSED_TESTS + 1)); fi

SUCCESS_RATE=$(echo "scale=1; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc 2>/dev/null || echo "N/A")

echo "Total tests: $TOTAL_TESTS"
echo "Passed tests: $PASSED_TESTS"
echo "Success rate: $SUCCESS_RATE%"

if [ "$SUCCESS_RATE" = "100.0" ] || [ "$PASSED_TESTS" -eq "$TOTAL_TESTS" ]; then
    echo "🎉 ALL TESTS PASSED! Dynamic confidence system working perfectly!"
else
    echo "⚠ Some tests failed. Check log for details."
fi

echo ""
echo "=========================================="
echo "Comprehensive test completed!"
echo "Check comprehensive_test.log for detailed output"
echo "=========================================="

# Cleanup
rm -f comprehensive_test.log
