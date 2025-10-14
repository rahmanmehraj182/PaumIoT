#!/usr/bin/env python3

# Debug MQTT packet structure
mqtt_packet = [
    0x30, 0x18,  # PUBLISH, remaining length = 24
    0x00, 0x0A,  # Topic length = 10
    # Topic: "test/topic" 
    0x74, 0x65, 0x73, 0x74, 0x2F, 0x74, 0x6F, 0x70, 0x69, 0x63,
    # Payload: "Hello, World!" (13 bytes, not 12!)
    0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21
]

print(f"Total packet length: {len(mqtt_packet)}")
print(f"Header bytes: 2")  
print(f"Remaining length field: {mqtt_packet[1]}")
print(f"Expected packet size: {2 + mqtt_packet[1]} = {2 + mqtt_packet[1]}")

# Calculate payload
topic_len_field = 2
topic_len = 10
payload_start = 2 + topic_len_field + topic_len
payload = mqtt_packet[payload_start:]
print(f"Payload bytes: {len(payload)}")
print(f"Payload: {''.join(chr(b) for b in payload)}")

# Check if lengths match
expected_remaining = topic_len_field + topic_len + len(payload)
print(f"Expected remaining length: {expected_remaining}")
print(f"Actual remaining length: {mqtt_packet[1]}")
print(f"Match: {expected_remaining == mqtt_packet[1]}")