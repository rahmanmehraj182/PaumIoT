#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#include "../core/types.h"

// Global server state
extern server_state_t g_server;
extern volatile int g_running;

// Server operations
int server_init(void);
void server_run(void);
void server_cleanup(void);

// Statistics and monitoring
void print_server_stats(void);

#endif // SERVER_CORE_H
