/*
 * File: chat.c
 */

#include <stdint.h>
#include <stdio.h>		/* standard input/output library functions */
#include <stdlib.h>		/* standard library definitions (macros) */
#include <unistd.h>		/* standard symbolic constants and types */
#include <string.h>		/* string operations (strncpy, memset..) */

#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>		/* definitions for UNIX domain sockets */

#include "ping_client.h"
#include <sys/time.h>

struct information {
    uint8_t destination_host;
    char message[256];
};


void client(char *socket_lower, uint8_t *destination_host, char *message, size_t buf_len)
{
		struct sockaddr_un addr;
		int	   sd, rc;
		char   buf[256];

		struct timeval start, end;

		// ping_client [-h] <socket_lower> <destination_host> <message>:
		
		sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
		if (sd < 0) {
				perror("socket");
				exit(EXIT_FAILURE);
		}

		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, socket_lower, sizeof(addr.sun_path) - 1);

		rc = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
		if ( rc < 0) {
				perror("connect");
				close(sd);
				exit(EXIT_FAILURE);
		}

		printf("debuf:\n");
		printf("PING:%s\n",message);
		
		int epoll_fd = epoll_create1(0);
		if(epoll_fd == -1)
		{
				perror("epoll_create1");
				close(sd);
				return;
		}

		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = sd;
		if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd, &event) == -1)
		{
				perror("epoll_ctl");
				close(epoll_fd);
				close(sd);
				return;
		}
		struct epoll_event events[1];
		

		char *ping_message = (char *)malloc(strlen(message) + 6); //+6 because of PING:
		if (ping_message == NULL) {
				fprintf(stderr, "Memory allocation failed.\n");
				return ;
		}
		strcpy(ping_message, "PING:");
		strcat(ping_message, message);
		printf("%s\n", ping_message);
		
		struct information info;
		info.destination_host = *destination_host;
		strncpy(info.message, ping_message, sizeof(info.message));
		info.message[sizeof(info.message)-1] = '\0'; // Ensure null-termination
		
		gettimeofday(&start, NULL);
		rc = write(sd, &info, sizeof(struct information));
		printf("Waiting for a response....\n");
		do 
		{
				rc = epoll_wait(epoll_fd, events, 1, 1000);
				if(rc < 0)
				{
						perror("epoll_wait");
						break;
				}
				printf("WE ount the wait!\n");
				if(rc == 0) 
				{
						printf("Timeout\n");
						break;
				}
				memset(buf,0, sizeof(buf));
				if(events[0].events & EPOLLIN)
				{
						int read_rc = read(sd,buf,sizeof(buf));
						gettimeofday(&end, NULL);
						double time_taken = end.tv_sec + end.tv_usec / 1e6 -start.tv_sec - start.tv_usec / 1e6; // in seconds
						if(read_rc <= 0)
						{
								close(sd);
								printf("<%d< left the chat...\n", sd);
								break;
						}
						printf("%s\nTime taken for execution [%f] seconds.\n", buf, time_taken);
						break;
				}	
				/*
				memset(buf, 0, sizeof(buf));
				// Wait for a response from the server
				rc = read(sd, buf, sizeof(buf) - 1);
				if (rc > 0) {
						buf[rc] = '\0'; // Ensure null-termination
						printf("Received: %s\n", buf);
				} else if (rc == 0) {
						printf("Server closed the connection.\n");
						break;
				} else {
						perror("read");
						close(sd);
						exit(EXIT_FAILURE);
				}*/
				//rc = write(sd, buf, strlen(buf));
				/*if (rc < 0) {
						perror("write");
						close(sd);
						exit(EXIT_FAILURE);
				}*/
		} while (1);

		close(sd);
}
