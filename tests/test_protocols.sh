#!/bin/bash

# PaumIoT Protocol Testing Script
echo "==================================="
echo "PaumIoT Protocol Detection Tests"
echo "==================================="

# Start the server
echo "Starting PaumIoT server..."
./paumiot &
SERVER_PID=$!
sleep 2

echo "Server started with PID: $SERVER_PID"

# Test HTTP
echo ""
echo "1. Testing HTTP Protocol..."
echo "Sending HTTP GET request..."
HTTP_RESPONSE=$(curl -s -w "%{http_code}" http://localhost:8080/api/test 2>/dev/null)
if [ $? -eq 0 ]; then
    echo "✅ HTTP test successful - Response received"
else
    echo "❌ HTTP test failed"
fi

# Test UDP with a simple packet (simulating CoAP)
echo ""
echo "2. Testing UDP/CoAP Protocol..."
echo "Sending CoAP-like UDP packet..."
# CoAP GET request: Version=1, Type=CON(0), Token=0, Code=GET(1), Message ID=12345
printf '\x40\x01\x30\x39' | nc -u -w1 localhost 8080 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ UDP/CoAP test sent successfully"
else
    echo "❌ UDP/CoAP test failed"
fi

# Test with different HTTP methods
echo ""
echo "3. Testing Different HTTP Methods..."
for method in POST PUT DELETE; do
    echo "Testing $method..."
    curl -s -X $method http://localhost:8080/test >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "✅ $method test successful"
    else
        echo "❌ $method test failed"
    fi
done

# Let the server run for a bit to show statistics
echo ""
echo "4. Waiting for server statistics (5 seconds)..."
sleep 5

# Clean shutdown
echo ""
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "==================================="
echo "Protocol Detection Tests Complete"
echo "==================================="
