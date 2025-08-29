#!/bin/bash

# PaumIoT Packet Test Script
# Quick test of all protocols with the dynamic confidence system

echo "=========================================="
echo "PAUMIOT PACKET TEST SCRIPT"
echo "=========================================="

# Check if server is running
if ! pgrep -f "paumiot" > /dev/null; then
    echo "Starting PaumIoT server..."
    make clean && make
    ./paumiot > test_output.log 2>&1 &
    SERVER_PID=$!
    sleep 3
    echo "Server started with PID: $SERVER_PID"
else
    echo "Server already running"
    SERVER_PID=$(pgrep -f "paumiot" | head -1)
fi

echo ""
echo "Testing All Protocols..."
echo "========================"

# HTTP Tests
echo "1. Testing HTTP (Expected: 80-85% confidence)"
curl -s http://localhost:8080/test > /dev/null
echo "   ✓ HTTP GET completed"

# MQTT Tests
echo "2. Testing MQTT (Expected: 80-85% confidence)"
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc localhost 8080 > /dev/null
echo "   ✓ MQTT CONNECT completed"

# DNS Tests
echo "3. Testing DNS (Expected: 80-85% confidence)"
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc localhost 8080 > /dev/null
echo "   ✓ DNS A record query completed"

# TLS Tests
echo "4. Testing TLS (Expected: 85-95% confidence)"
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc localhost 8080 > /dev/null
echo "   ✓ TLS Client Hello completed"

# CoAP Tests (UDP)
echo "5. Testing CoAP (Expected: 60-75% confidence)"
printf '\x40\x01\x30\x39\x12' | nc -u -w1 localhost 8080 > /dev/null 2>&1
echo "   ✓ CoAP GET completed"

# QUIC Tests (UDP)
echo "6. Testing QUIC (Expected: 80-90% confidence)"
printf '\x80\x51\x30\x30\x30\x08\x08\x12\x34\x56\x78\x90\xAB\xCD\xEF\xFE\xDC\xBA\x09\x87\x65\x43\x21' | nc -u -w1 localhost 8080 > /dev/null 2>&1
echo "   ✓ QUIC Initial packet completed"

echo ""
echo "Testing Medium Quality Packets..."
echo "================================="

# Medium quality HTTP
printf "POST /data HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\n\r\nhello" | nc localhost 8080 > /dev/null
echo "   ✓ Medium quality HTTP completed"

# Medium quality MQTT
printf '\x30\x0A\x00\x05topic\x00\x01a' | nc localhost 8080 > /dev/null
echo "   ✓ Medium quality MQTT completed"

# Medium quality DNS
printf '\x00\x02\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x0F\x00\x01' | nc localhost 8080 > /dev/null
echo "   ✓ Medium quality DNS completed"

echo ""
echo "Testing Low Quality Packets..."
echo "=============================="

# Low quality HTTP
printf "GET / HTTP/1.1\r\n\r\n" | nc localhost 8080 > /dev/null
echo "   ✓ Low quality HTTP completed"

# Low quality MQTT
printf '\xC0\x00' | nc localhost 8080 > /dev/null
echo "   ✓ Low quality MQTT completed"

# Low quality TLS
printf '\x15\x03\x01\x00\x02\x02\x28' | nc localhost 8080 > /dev/null
echo "   ✓ Low quality TLS completed"

echo ""
echo "Testing Mixed Protocol Streams..."
echo "=================================="

# HTTP + TLS mixed
printf "GET /secure HTTP/1.1\r\nHost: localhost:8080\r\nConnection: Upgrade\r\nUpgrade: TLS/1.0\r\n\r\n" | nc localhost 8080 > /dev/null
printf '\x16\x03\x01\x00\x85\x01\x00\x00\x81\x03\x01' | nc localhost 8080 > /dev/null
echo "   ✓ HTTP + TLS mixed stream completed"

# MQTT + DNS mixed
printf '\x10\x0C\x00\x04MQTT\x04\xC2\x00\x3C\x00\x05test1' | nc localhost 8080 > /dev/null
printf '\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\x00\x00\x01\x00\x01' | nc localhost 8080 > /dev/null
echo "   ✓ MQTT + DNS mixed stream completed"

# CoAP + QUIC mixed stream (UDP)
printf '\x40\x01\x30\x39\x12' | nc -u -w1 localhost 8080 > /dev/null 2>&1
printf '\x80\x51\x30\x30\x30\x08\x08\x12\x34\x56\x78\x90\xAB\xCD\xEF\xFE\xDC\xBA\x09\x87\x65\x43\x21' | nc -u -w1 localhost 8080 > /dev/null 2>&1
echo "   ✓ CoAP + QUIC mixed stream completed"

echo ""
echo "Waiting for processing..."
sleep 2

echo ""
echo "=========================================="
echo "TEST RESULTS ANALYSIS"
echo "=========================================="

# Check if log file exists
if [ -f "test_output.log" ]; then
    echo "Protocol Detection Summary:"
    echo "==========================="
    echo "HTTP detections: $(grep -c 'Protocol detected: HTTP' test_output.log 2>/dev/null || echo '0')"
    echo "MQTT detections: $(grep -c 'Protocol detected: MQTT' test_output.log 2>/dev/null || echo '0')"
    echo "DNS detections: $(grep -c 'Protocol detected: DNS' test_output.log 2>/dev/null || echo '0')"
    echo "TLS detections: $(grep -c 'Protocol detected: TLS' test_output.log 2>/dev/null || echo '0')"
    echo "CoAP detections: $(grep -c 'Protocol detected: CoAP' test_output.log 2>/dev/null || echo '0')"
    echo "QUIC detections: $(grep -c 'Protocol detected: QUIC' test_output.log 2>/dev/null || echo '0')"
    
    echo ""
    echo "Confidence Distribution:"
    echo "======================="
    echo "High confidence (80-99%): $(grep -c 'confidence: [8-9][0-9]' test_output.log 2>/dev/null || echo '0')"
    echo "Medium confidence (60-79%): $(grep -c 'confidence: [6-7][0-9]' test_output.log 2>/dev/null || echo '0')"
    echo "Low confidence (30-59%): $(grep -c 'confidence: [3-5][0-9]' test_output.log 2>/dev/null || echo '0')"
    
    echo ""
    echo "Sample Confidence Values:"
    echo "========================="
    echo "HTTP confidence values:"
    grep "Protocol detected: HTTP" test_output.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No HTTP detections found"
    
    echo "MQTT confidence values:"
    grep "Protocol detected: MQTT" test_output.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No MQTT detections found"
    
    echo "DNS confidence values:"
    grep "Protocol detected: DNS" test_output.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No DNS detections found"
    
    echo "TLS confidence values:"
    grep "Protocol detected: TLS" test_output.log | head -3 | awk '{print $NF}' 2>/dev/null || echo "   No TLS detections found"
    
    echo ""
    echo "Dynamic Confidence System Status:"
    echo "================================="
    if grep -q "DYNAMIC CONFIDENCE SYSTEM" test_output.log; then
        echo "✓ Dynamic confidence system active"
        CALIBRATION_FACTOR=$(grep "Calibration factor:" test_output.log | tail -1 | awk '{print $3}' 2>/dev/null || echo "N/A")
        echo "✓ Calibration factor: $CALIBRATION_FACTOR"
    else
        echo "✗ Dynamic confidence system not found"
    fi
    
else
    echo "No test output log found. Server may not be running properly."
fi

echo ""
echo "=========================================="
echo "TEST COMPLETED!"
echo "=========================================="

# Ask user if they want to stop the server
read -p "Do you want to stop the server? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null
        echo "Server stopped."
    else
        pkill -f paumiot
        echo "Server stopped."
    fi
else
    echo "Server is still running. Check test_output.log for detailed results."
fi

echo ""
echo "For detailed packet specifications, see:"
echo "- PACKET_LIBRARY.md"
echo "- PACKET_TEST_COMMANDS.md"
echo "- PACKET_QUICK_REFERENCE.md"
