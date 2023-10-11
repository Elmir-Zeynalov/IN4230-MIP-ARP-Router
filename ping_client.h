#ifndef _PING_CLIENT_H
#define _PING_CLIENT_H
#include <stddef.h>

#include <stdint.h>
/* MACROs */
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */
#define SOCKET_NAME "app"
void client(char *socket_lower, uint8_t *destination_host, char *message, size_t buf_len);
#endif /* _CHAT_H */
