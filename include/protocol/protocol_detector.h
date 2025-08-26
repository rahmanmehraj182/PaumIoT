#ifndef PROTOCOL_DETECTOR_H
#define PROTOCOL_DETECTOR_H

#include "../core/types.h"
#include <stddef.h>

// Protocol detection functions
protocol_type_t detect_protocol(const uint8_t *data, size_t len);
protocol_type_t detect_protocol_fast(const uint8_t *data, size_t len);

// Individual protocol detectors
int detect_mqtt(const uint8_t *data, size_t len);
int detect_coap(const uint8_t *data, size_t len);
int detect_http(const uint8_t *data, size_t len);
int detect_dns(const uint8_t *data, size_t len);

// Utility functions
const char *protocol_to_string(protocol_type_t protocol);

#endif // PROTOCOL_DETECTOR_H
