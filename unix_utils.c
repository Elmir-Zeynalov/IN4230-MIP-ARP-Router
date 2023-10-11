#include <stdio.h>		/* standard input/output library functions */
#include <stdlib.h>		/* standard library definitions (macros) */
#include <unistd.h>		/* standard symbolic constants and types */
#include <string.h>		/* string operations (strncpy, memset..) */

#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>		/* definitions for UNIX domain sockets */

#include "unix_utils.h"
/**
 * Prepare (create, bind, listen) the server listening socket
 *
 * Returns the file descriptor of the server socket.
 *
 * In C, you declare a function as static when you want to limit its scope to
 * the current source file. When a function is declared as static, it can only
 * be called from within the same source file where it is defined.
 */
int prepare_server_sock(char *path)
{
		struct sockaddr_un addr;
		int sd = -1, rc = -1;

		sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
		if (sd	== -1) {
				perror("socket()");
				return rc;
		}

		memset(&addr, 0, sizeof(struct sockaddr_un));

		/* 2. Bind socket to socket name. */
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
		/* Unlink the socket so that we can reuse the program.
		 * This is bad hack! Better solution with a lock file,
		 * or signal handling.
		 * Check https://gavv.github.io/articles/unix-socket-reuse
		 */
		unlink(path);

		rc = bind(sd, (const struct sockaddr *)&addr, sizeof(addr));
		if (rc == -1) {
				perror("bind");
				close(sd);
				return rc;
		}

		/*
		 * 3. Prepare for accepting incomming connections.
		 * The backlog size is set to MAX_CONNS.
		 * So while one request is being processed other requests
		 * can be waiting.
		 */

		rc = listen(sd, MAX_CONNS);
		if (rc == -1) {
				perror("listen()");
				close(sd);
				return rc;
		}

		return sd;
}

int add_to_epoll_table(int efd, struct epoll_event *ev, int fd)
{
		int rc = 0;

		/* EPOLLIN event: The associated file is available for read(2)
		 * operations. Check man epoll in Linux for more useful events.
		 */
		ev->events = EPOLLIN;
		ev->data.fd = fd;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, ev) == -1) {
				perror("epoll_ctl");
				rc = -1;
		}

		return rc;
}
