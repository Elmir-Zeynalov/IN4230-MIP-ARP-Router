#include <stdint.h>
#include <stdio.h>              /* standard input/output library functions */
#include <stdlib.h>             /* standard library definitions (macros) */
#include <unistd.h>             /* standard symbolic constants and types */
#include <string.h>             /* string operations (strncpy, memset..) */

#include <sys/epoll.h>  /* epoll */
#include <sys/socket.h> /* sockets operations */
#include <sys/un.h>             /* definitions for UNIX domain sockets */


void usage(char *arg)
{
        printf("Usage: %s [-h] <socket_lower>\n"
                "Options:\n"
                "\t-h: prints help and exits program\n"
                "\tsocket_lower: pathname of the UNIX socket used to interface with upper layers\n", arg);
}

int identify_myself(int sd)
{
                uint8_t my_id = 0x02; //SDU_TYPE = PING
                int rc;

                rc = write(sd, &my_id, sizeof(uint8_t));
                if(rc < 0) {
                                perror("write() error");
                                return -1;
                }
                return 1;
}

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

                if(identify_myself(sd) < 0)
                {
                                perror("Couldnt identify myself to daemon...");
                                close(sd);
                                return;
                }

                printf("[PONG SERVER] Waiting for a request....\n");
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

                                                int read_rc = read(sd,&info,sizeof(struct information));
                                                if(read_rc <= 0)
                                                {
                                                                close(sd);
                                                                printf("<%d< left the chat...\n", sd);
                                                                break;
                                                }

                                                info.message[1] = 'O';
                                                printf("%s\n", info.message);

                                                rc = write(sd, &info, sizeof(struct information));
                                                if(rc <= 0)
                                                {
                                                                close(sd);
                                                                printf("issue writing....");
                                                                break;
                                                }
                                }
                } while (1);

                close(sd);
}

int main (int argc, char *argv[])
{

        int hflag = 0;
        int opt;
        while((opt = getopt(argc, argv, "h")) != -1)
        {
                switch (opt) {
                        case 'h':
                                hflag = 1;
                                break;
                        default:
                                usage(argv[0]);
                                exit(EXIT_FAILURE);
                }
        }

        if(hflag)
        {
                usage(argv[0]);
                exit(EXIT_SUCCESS);
        }

        if(argc < 2)
        {
                usage(argv[0]);
                exit(EXIT_SUCCESS);
        }


        char *socket_lower = argv[1];
        printf("Socket Lower: %s\n", socket_lower);
        server(socket_lower);
        return 0;
}
