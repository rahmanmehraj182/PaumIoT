#!/bin/bash

# PaumIoT Packet Library Demo Script
# Demonstrates how to send various protocol packets to test the dynamic confidence system

set -e

echo "=========================================="
echo "PAUMIOT PACKET LIBRARY DEMO"
echo "=========================================="

# Build the project
echo "Building project..."
make clean
make

# Start server in background
echo "Starting server..."
./paumiot > packet_demo.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 3

echo ""
echo "Testing High Quality Packets (Expected: 75-95% confidence)"
echo "=========================================================="

# HTTP GET with full headers
echo "1. HTTP GET with full headers:"
curl -s http://localhost:8080/test > /dev/null
echo "   ✓ Sent HTTP GET request"

# MQTT CONNECT
echo "2. MQTT CONNECT packet:"
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent MQTT CONNECT packet"

# DNS A record query
echo "3. DNS A record query:"
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent DNS A record query"

# TLS Client Hello
echo "4. TLS Client Hello:"
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent TLS Client Hello"

# CoAP GET (UDP)
echo "5. CoAP GET request (UDP):"
printf '\x40\x01\x30\x39\x12' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent CoAP GET request"

# QUIC Initial (UDP)
echo "6. QUIC Initial packet (UDP):"
printf '\x80\x51\x30\x30\x30\x08\x08\x12\x34\x56\x78\x90\xAB\xCD\xEF\xFE\xDC\xBA\x09\x87\x65\x43\x21' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent QUIC Initial packet"

echo ""
echo "Testing Medium Quality Packets (Expected: 60-80% confidence)"
echo "============================================================="

# HTTP POST with minimal headers
echo "7. HTTP POST with minimal headers:"
printf "POST /data HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\n\r\nhello" | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent HTTP POST request"

# MQTT PUBLISH
echo "8. MQTT PUBLISH packet:"
printf '\x30\x0A\x00\x05topic\x00\x01a' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent MQTT PUBLISH packet"

# DNS MX record query
echo "9. DNS MX record query:"
printf '\x00\x02\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x0F\x00\x01' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent DNS MX record query"

# TLS Application Data
echo "10. TLS Application Data:"
printf '\x17\x03\x01\x00\x10' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent TLS Application Data"

# CoAP POST (UDP)
echo "11. CoAP POST request (UDP):"
printf '\x42\x02\x30\x39\x12\x34' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent CoAP POST request"

# QUIC Long header (UDP)
echo "12. QUIC Long header packet (UDP):"
printf '\x80\x00\x00\x00\x00\x00\x00\x00\x00' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent QUIC Long header packet"

echo ""
echo "Testing Low Quality Packets (Expected: 30-60% confidence)"
echo "=========================================================="

# Minimal HTTP request
echo "13. Minimal HTTP request:"
printf "GET / HTTP/1.1\r\n\r\n" | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent minimal HTTP request"

# MQTT PINGREQ
echo "14. MQTT PINGREQ packet:"
printf '\xC0\x00' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent MQTT PINGREQ packet"

# MQTT DISCONNECT
echo "15. MQTT DISCONNECT packet:"
printf '\xE0\x00' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent MQTT DISCONNECT packet"

# TLS Alert
echo "16. TLS Alert packet:"
printf '\x15\x03\x01\x00\x02\x02\x28' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent TLS Alert packet"

# CoAP DELETE (UDP)
echo "17. CoAP DELETE request (UDP):"
printf '\x40\x04\x30\x39' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent CoAP DELETE request"

echo ""
echo "Testing Mixed Protocol Streams"
echo "==============================="

# HTTP + TLS mixed stream
echo "18. HTTP + TLS mixed stream:"
printf "GET /secure HTTP/1.1\r\nHost: localhost:8080\r\nConnection: Upgrade\r\nUpgrade: TLS/1.0\r\n\r\n" | nc -w1 localhost 8080 > /dev/null
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent HTTP + TLS mixed stream"

# MQTT + DNS mixed stream
echo "19. MQTT + DNS mixed stream:"
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc -w1 localhost 8080 > /dev/null
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc -w1 localhost 8080 > /dev/null
echo "   ✓ Sent MQTT + DNS mixed stream"

# CoAP + QUIC mixed stream (UDP)
echo "20. CoAP + QUIC mixed stream (UDP):"
printf '\x40\x01\x30\x39\x12' | nc -u -w1 localhost 8080 > /dev/null
printf '\x80\x51\x30\x30\x30\x08\x08\x12\x34\x56\x78\x90\xAB\xCD\xEF\xFE\xDC\xBA\x09\x87\x65\x43\x21' | nc -u -w1 localhost 8080 > /dev/null
echo "   ✓ Sent CoAP + QUIC mixed stream"

# Wait for processing
sleep 2

# Stop server
echo ""
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Analyze results
echo ""
echo "=========================================="
echo "PACKET DEMO RESULTS ANALYSIS"
echo "=========================================="

echo "Server log analysis:"
echo ""

# Check protocol detection
echo "1. Protocol Detection Summary:"
PROTOCOLS=("HTTP" "MQTT" "DNS" "TLS" "CoAP" "QUIC")
for protocol in "${PROTOCOLS[@]}"; do
    COUNT=$(grep -c "Protocol detected: $protocol" packet_demo.log 2>/dev/null || echo "0")
    echo "   $protocol: $COUNT detections"
done

# Check confidence distribution
echo ""
echo "2. Confidence Distribution:"
HIGH_CONF=$(grep -c "confidence: [8-9][0-9]" packet_demo.log 2>/dev/null || echo "0")
MED_CONF=$(grep -c "confidence: [6-7][0-9]" packet_demo.log 2>/dev/null || echo "0")
LOW_CONF=$(grep -c "confidence: [3-5][0-9]" packet_demo.log 2>/dev/null || echo "0")
echo "   High confidence (80-99%): $HIGH_CONF"
echo "   Medium confidence (60-79%): $MED_CONF"
echo "   Low confidence (30-59%): $LOW_CONF"

# Show sample confidence values
echo ""
echo "3. Sample Confidence Values:"
echo "   HTTP confidence values:"
grep "Protocol detected: HTTP" packet_demo.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No HTTP detections found"

echo "   MQTT confidence values:"
grep "Protocol detected: MQTT" packet_demo.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No MQTT detections found"

echo "   DNS confidence values:"
grep "Protocol detected: DNS" packet_demo.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No DNS detections found"

echo "   TLS confidence values:"
grep "Protocol detected: TLS" packet_demo.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No TLS detections found"

# Performance metrics
echo ""
echo "4. Performance Metrics:"
TOTAL_PACKETS=$(grep "Total packets:" packet_demo.log | tail -1 | awk '{print $3}' 2>/dev/null || echo "0")
IDENTIFIED_PACKETS=$(grep "Identified packets:" packet_demo.log | tail -1 | awk '{print $3}' 2>/dev/null || echo "0")
echo "   Total packets processed: $TOTAL_PACKETS"
echo "   Identified packets: $IDENTIFIED_PACKETS"

if [ "$TOTAL_PACKETS" -gt 0 ]; then
    IDENTIFICATION_RATE=$(echo "scale=2; $IDENTIFIED_PACKETS * 100 / $TOTAL_PACKETS" | bc 2>/dev/null || echo "N/A")
    echo "   Identification rate: $IDENTIFICATION_RATE%"
fi

# Dynamic confidence system status
echo ""
echo "5. Dynamic Confidence System Status:"
if grep -q "DYNAMIC CONFIDENCE SYSTEM" packet_demo.log; then
    echo "   ✓ Dynamic confidence system active"
    CALIBRATION_FACTOR=$(grep "Calibration factor:" packet_demo.log | tail -1 | awk '{print $3}' 2>/dev/null || echo "N/A")
    echo "   ✓ Calibration factor: $CALIBRATION_FACTOR"
else
    echo "   ✗ Dynamic confidence system not found"
fi

echo ""
echo "=========================================="
echo "PACKET DEMO COMPLETED!"
echo "Check packet_demo.log for detailed output"
echo "=========================================="

# Cleanup
rm -f packet_demo.log
