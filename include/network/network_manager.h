#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "../core/types.h"
#include <sys/epoll.h>

// Socket management
int make_socket_nonblocking(int fd);
int add_to_epoll(int epoll_fd, int fd, uint32_t events, void *data);

// Connection management
connection_t *allocate_connection(void);
void free_connection(connection_t *conn);

// Network event handlers
int handle_tcp_accept(void);
int handle_udp_message(void);
int handle_connection_read(connection_t *conn);
int handle_connection_write(connection_t *conn);

// Congestion control
void init_congestion_control(congestion_ctrl_t *cc);
int apply_congestion_control(connection_t *conn);
void update_congestion_window(connection_t *conn, int success);

#endif // NETWORK_MANAGER_H
