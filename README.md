

// g++ -o middleware main.cpp -lpthread
// ./middleware


// # MQTT test
// mosquitto_pub -h localhost -p 8080 -t test/topic -m "hello"

// # CoAP test  
// coap-client -m get coap://localhost:8080/test


## For Event Driven:
```
# Compile
gcc -o event_middleware event_middleware.c

# Test HTTP
curl -X GET http://localhost:8080/test

# Test with load
for i in {1..100}; do curl http://localhost:8080/ & done

# Test UDP (DNS)
dig @localhost -p 8080 example.com

# Monitor congestion control
watch -n 1 'netstat -an | grep :8080'
```