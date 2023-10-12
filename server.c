/*
 * File: chat.c
 */

#include <stdint.h>
#include <stdio.h>              /* standard input/output library functions */
#include <stdlib.h>             /* standard library definitions (macros) */
#include <unistd.h>             /* standard symbolic constants and types */
#include <string.h>             /* string operations (strncpy, memset..) */

#include <sys/epoll.h>  /* epoll */
#include <sys/socket.h> /* sockets operations */
#include <sys/un.h>             /* definitions for UNIX domain sockets */


struct information {
                uint8_t destination_host;
                char message[255];
};
void server(char *socket_lower)
{
                struct sockaddr_un addr;
                int        sd, rc;
                char   buf[255];

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

                printf("*PING SERVER*\n");

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

                printf("[PING SERVER] Waiting for a response....\n");
                do
                {
                                rc = epoll_wait(epoll_fd, events, 1, -1);
                                if(rc < 0)
                                {
                                                perror("epoll_wait");
                                                break;
                                }
                                if(rc == 0)
                                {
                                                printf("Timeout\n");
                                                break;
                                }
                                memset(buf,0, sizeof(buf));
                                if(events[0].events & EPOLLIN)
                                {
                                                struct information info;

                                                int read_rc = read(sd,&info,sizeof(struct information));
                                                if(read_rc <= 0)
                                                {
                                                                close(sd);
                                                                printf("<%d< left the chat...\n", sd);
                                                                break;
                                                }

                                                printf("PONG_SERVER_RECEIVED MESSAGE: [%s]\n", buf);
                                                //char pong_message[256]; // Assuming a maximum message length of 256, adjust as needed
                                                //sprintf(pong_message, "PONG:%s", buf);
                                                info.message[1] = 'O';

                                                printf("Message Relaying to daemon [%s]\n", info.message);

                                                int write_rc = write(sd, &info, sizeof(struct information));
                                }
                } while (1);

                close(sd);
}

int main (int argc, char *argv[])
{
                
        char *socket_lower = argv[1];
        printf("Socket Lower: %s\n", socket_lower);
        server(socket_lower);
        return 0;
}
