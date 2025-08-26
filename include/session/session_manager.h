#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include "../core/types.h"
#include <netinet/in.h>

// Session management functions
int create_session(int socket_fd, struct sockaddr_in *client_addr, protocol_type_t protocol);
connection_t *get_session_by_socket(int socket_fd);
void remove_session(int socket_fd);
void update_session_activity(connection_t *session);

// Session table operations
void print_session_table(void);
const char *state_to_string(session_state_t state);

// Session cleanup
void cleanup_stale_connections(void);

#endif // SESSION_MANAGER_H
