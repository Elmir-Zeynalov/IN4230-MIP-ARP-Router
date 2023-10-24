#ifndef _UNIX_UTILS_H
#define _UNIX_UTILS_H

/* MACROs */
#define SOCKET_NAME "server.socket"
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */

int prepare_server_sock(char *path);
int add_to_epoll_table(int efd, struct epoll_event *ev, int fd);

int create_raw_socket(void);

#endif
