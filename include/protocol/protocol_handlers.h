#ifndef PROTOCOL_HANDLERS_H
#define PROTOCOL_HANDLERS_H

#include "../core/types.h"
#include <stddef.h>

// Protocol message handlers
void handle_mqtt_message(connection_t *conn, const uint8_t *data, size_t len);
void handle_coap_message(connection_t *conn, const uint8_t *data, size_t len);
void handle_http_message(connection_t *conn, const uint8_t *data, size_t len);
void handle_dns_message(connection_t *conn, const uint8_t *data, size_t len, struct sockaddr_in *client_addr);

// Generic protocol message processor
void process_protocol_message(connection_t *conn, const uint8_t *data, size_t len);

#endif // PROTOCOL_HANDLERS_H
