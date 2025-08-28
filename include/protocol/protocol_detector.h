#ifndef PROTOCOL_DETECTOR_H
#define PROTOCOL_DETECTOR_H

#include "../core/types.h"
#include <stddef.h>
#include <pcap.h>

// Enhanced Protocol Detection Functions
protocol_type_t detect_protocol(const uint8_t *data, size_t len);
protocol_type_t detect_protocol_fast(const uint8_t *data, size_t len);
protocol_type_t detect_protocol_enhanced(const uint8_t *data, size_t len, 
                                        uint8_t *confidence, int is_tcp);

// Dynamic Confidence System Functions
uint8_t calculate_dynamic_confidence(protocol_type_t protocol, const uint8_t *data, size_t len, 
                                   int is_tcp, confidence_features_t *features);
void extract_confidence_features(const uint8_t *data, size_t len, int is_tcp, 
                               protocol_type_t protocol, confidence_features_t *features);
float calculate_entropy_score(const uint8_t *data, size_t len);
float calculate_pattern_strength(const uint8_t *data, size_t len, protocol_type_t protocol);
float calculate_validation_depth(const uint8_t *data, size_t len, protocol_type_t protocol);
float calculate_header_consistency(const uint8_t *data, size_t len, protocol_type_t protocol);
float calculate_payload_structure(const uint8_t *data, size_t len, protocol_type_t protocol);
float calculate_transport_compatibility(int is_tcp, protocol_type_t protocol);
float calculate_context_relevance(const uint8_t *data, size_t len, protocol_type_t protocol);
float get_historical_accuracy(protocol_type_t protocol);
float calculate_false_positive_risk(const uint8_t *data, size_t len, protocol_type_t protocol);
float calculate_protocol_specificity(const uint8_t *data, size_t len, protocol_type_t protocol);

// Confidence Learning and Adaptation
void update_protocol_accuracy(protocol_type_t protocol, uint8_t predicted_confidence, 
                             uint8_t actual_confidence, uint8_t was_correct);
void add_confidence_history(protocol_type_t protocol, uint8_t predicted_confidence, 
                          uint8_t actual_confidence, uint8_t was_correct, 
                          const confidence_features_t *features);
void calibrate_confidence_system(void);
float calculate_confidence_error(uint8_t predicted, uint8_t actual);
void update_confidence_calibration(float error);

// Individual protocol detectors (enhanced)
int detect_mqtt_enhanced(const uint8_t *data, size_t len);
int detect_coap_enhanced(const uint8_t *data, size_t len);
int detect_http_enhanced(const uint8_t *data, size_t len);
int detect_dns_enhanced(const uint8_t *data, size_t len);
int detect_tls_enhanced(const uint8_t *data, size_t len);
int detect_quic_enhanced(const uint8_t *data, size_t len);

// Legacy protocol detectors (kept for compatibility)
int detect_mqtt(const uint8_t *data, size_t len);
int detect_coap(const uint8_t *data, size_t len);
int detect_http(const uint8_t *data, size_t len);
int detect_dns(const uint8_t *data, size_t len);

// Enhanced TCP Connection State Management
uint32_t connection_hash(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port);
void update_connection_state(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, 
                           uint16_t dst_port, protocol_type_t protocol, uint32_t seq_number, uint8_t confidence);
protocol_type_t find_connection_state(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port);
void cleanup_old_connections(void);

// Protocol identification with state tracking
protocol_type_t identify_protocol_with_state(int is_tcp, uint32_t src_ip, uint32_t dst_ip, 
                                            uint16_t src_port, uint16_t dst_port, uint32_t seq_number,
                                            const uint8_t *payload, uint32_t payload_len, uint8_t *confidence);

// Utility functions
const char *protocol_to_string(protocol_type_t protocol);
void print_enhanced_stats(void);

// Packet capture integration
int init_packet_capture(void);
void cleanup_packet_capture(void);
void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);

#endif // PROTOCOL_DETECTOR_H
