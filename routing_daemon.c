#include <bits/time.h>
#include <stdint.h>
#include <stdio.h>              /* standard input/output library functions */
#include <stdlib.h>             /* standard library definitions (macros) */
#include <time.h>
#include <unistd.h>             /* standard symbolic constants and types */
#include <string.h>             /* string operations (strncpy, memset..) */
#include <sys/epoll.h>  /* epoll */
#include <sys/socket.h> /* sockets operations */
#include <sys/un.h>             /* definitions for UNIX domain sockets */

#include "routing_utils.h"
#include "routing_table.h"
#include "routing_interface_to_mip_daemon.h"

void usage_router(char *arg)
{
        printf("Usage: %s [-h] <socket_lower>\n"
                "Options:\n"
                "\t-h: prints help and exits program\n"
                "\tsocket_lower: pathname of the UNIX socket used to interface with upper layers\n", arg);
}

/*
 * Here we identify ourselves to the MIP daemon. 
 * Since this is the Routing Daemon the SDU_TYPE we send to the MIP-Daemon is 0x04.
 * This should be sent as soon as we establish a connection with the MIP-Daemon.
 *
 * 
 */
int identify_myself(int sd)
{
                uint8_t my_id = 0x04; //SDU_TYPE = ROUTING 
                int rc;

                rc = write(sd, &my_id, sizeof(uint8_t));
                if(rc < 0) {
                                perror("write() error");
                                return -1;
                }
                return 1;
}
double diff_time_ms(struct timespec start, struct timespec end)
{
	double s, ms, ns;

	s  = (double)end.tv_sec  - (double)start.tv_sec;
	ns = (double)end.tv_nsec - (double)start.tv_nsec;

	if (ns < 0) { // clock underflow
		--s;
		ns += 1000000000;
	}

	ms = ((s) * 1000 + ns/1000000.0);

	return ms;
}

int routing_daemon_init(char *socket_lower, struct Table *routing_table)
{
                struct sockaddr_un addr;
                char   buf[255];
                int        sd, rc;
                
                struct epoll_event ev, events[10];
                int epollfd;

                enum state s_state;
                struct timespec lastHelloSent;
                struct timespec lastHelloRecv;
                struct timespec timenow;


                
                struct timespec lastTableUpdateSent;


                sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
                if (sd < 0) {
                                perror("socket");
                                return -1;
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

                printf("*Running the Routing Daemon*\n");

                int epoll_fd = epoll_create1(0);
                if(epoll_fd == -1)
                {
                                perror("epoll_create1");
                                close(sd);
                                return -1;
                }

                ev.events = EPOLLIN;
                ev.data.fd = sd;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1)
                {
                                perror("epoll_ctl");
                                close(epoll_fd);
                                close(sd);
                                return -1; 
                }

                if(identify_myself(sd) < 0)
                {
                                perror("Couldnt identify myself to daemon...");
                                close(sd);
                                return -1;
                }
                
                
                send_routing_hello(sd, 99);
                clock_gettime(CLOCK_REALTIME, &lastHelloSent);


                struct packet_ux pu;
                rc = epoll_wait(epoll_fd, events, 10, -1);
                if(rc == -1){
                                perror("epoll_wait");
                                close(sd);
                                return -1;
                }else{
                                printf("Received the first hello ... entering INIT state\n");
                                read(sd, &pu, sizeof(struct packet_ux));

                                printf("RECEIVE [%d]\n[%d]\n[%s]\n", pu.mip, pu.ttl, pu.msg);
                                
                                s_state = INIT;
                                clock_gettime(CLOCK_REALTIME, &lastHelloRecv);
                }
                while(1)
                {
                                printf("------ Start WW --------\n\n");
                                switch (s_state) {
                                                case INIT:
                                                                printf("STATE [INIT]\n\n"); 
                                                                send_routing_hello(sd,99);
                                                                s_state = WAIT;
                                                                clock_gettime(CLOCK_REALTIME, &lastHelloSent);
                                                                break;
                                                case WAIT:
                                                                printf("STATE [WAIT]\n\n");
                                                                rc = epoll_wait(epoll_fd, events, 10, 1000);
                                                                if(rc == -1){
                                                                                perror("epoll_wait");
                                                                                close(sd);
                                                                                return -1;
                                                                }

                                                                else if(rc == 0)
                                                                {
                                                                                printf("Epoll Timeout--\n");
                                                                                printf("\n");
                                                                                clock_gettime(CLOCK_REALTIME, &timenow);
                                                                                printf("TimeNow: %lld\n", (long long)timenow.tv_sec);
                                                                                printf("lastHelloSent: %lld\n", (long long)lastHelloSent.tv_sec);
                                                                                printf("lastTableUpdate: %lld\n", (long long)lastTableUpdateSent.tv_sec);

                                                                                if(diff_time_ms(lastHelloSent, timenow) > HELLO_TIMEOUT)
                                                                                {
                                                                                                s_state = INIT;
                                                                                }else if(diff_time_ms(lastTableUpdateSent, timenow) > TABLE_UPDATE_TIMEOUT){
                                                                                                printf("its been a while since we advertised our Routing Table []\n");
                                                                                                s_state = ADVERTISE_ROUTING_TABLE;
                                                                                }
                                                                }
                                                                else{
                                                                                printf("Receive MESSAGE ... entering INIT state\n");
                                                                                rc = read(sd, &pu, sizeof(struct packet_ux));
                                                                                if(rc == 0){
                                                                                                printf("Connection closed by MIP DAEMON :( \n");
                                                                                                s_state = EXIT;
                                                                                }else if(rc == -1){
                                                                                                perror("read");
                                                                                }else{
                                                                                                printf("RECEIVE [%d]\n[%d]\n[%s]\n", pu.mip, pu.ttl, pu.msg);
                                                                                                handle_routing_message(sd, &pu, routing_table);
                                                                                }
                                                                }

                                                                break;

                                                case ADVERTISE_ROUTING_TABLE:
                                                                printf("STATE: ADVERTISING ROUTES\n"); 
                                                                advertise_my_routing_table(sd, routing_table);
                                                                clock_gettime(CLOCK_REALTIME,&lastTableUpdateSent);
                                                                s_state = WAIT;
                                                                break;
                                                case EXIT:
                                                                printf("WE EXITING\n");
                                                                close(sd);
                                                                exit(EXIT_SUCCESS);
                                                                break;

                                                default:
                                                                printf("ERROR\n");
                                                                close(sd);
                                                                exit(EXIT_SUCCESS);
                                                                break;
                                }
                }
                
                /*

                while (1) {
                                switch (s_state) {
                                case INIT:
                                                //send_routing_hello(sd);
                                                printf("[<info>] SENT HELLO TO MIP-DAEMON [<info>]\n");
                                                clock_gettime(CLOCK_REALTIME, &lastHelloSent);
                                                s_state = WAIT;
                                                break;
                                case WAIT:

                                                printf("WAITINNNNN\n");
                                                rc = epoll_wait(epollfd, events, 10, -1);
                                                if (rc == -1) {
                                                                perror("epoll_wait");
                                                                exit(EXIT_FAILURE);
                                                } else if (rc == 0) {
                                                                /
                                                                clock_gettime(CLOCK_REALTIME, &timenow);
                                                                if (diff_time_ms(lastHelloRecv, timenow) > HELLO_TIMEOUT) {
                                                                printf("Timeout expired and didn't get any Hello\n"); 
                                                                                s_state = EXIT;
                                                                                continue;
                                                                } else {
                                                                                if (diff_time_ms(lastHelloSent, timenow) >= HELLO_INTERVAL) {
                                                                                 printf(Time to send HELLO\n");
                                                                                                s_state = INIT;
                                                                                                continue;
                                                                                }
                                                                }
                                                                /
                                                } else {
                                                                //if (recv_raw_packet(raw_sock, buf, BUF_SIZE) <= 0)
                                                                s_state = EXIT;
                                                                // else {
                                                                clock_gettime(CLOCK_REALTIME, &lastHelloRecv);
                                                                //  }
                                                }
                                                break;
                                case EXIT:
                                                close(sd);
                                                exit(EXIT_SUCCESS);
                                                break;
                                default:
                                                // undefined state
                                                exit(EXIT_FAILURE);
                                                break;
                                }
                }
                */
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
                                usage_router(argv[0]);
                                exit(EXIT_FAILURE);
                }
        }

        if(hflag)
        {
                usage_router(argv[0]);
                exit(EXIT_SUCCESS);
        }

        if(argc < 2)
        {
                usage_router(argv[0]);
                exit(EXIT_SUCCESS);
        }

        char *socket_lower = argv[1];
        printf("Socket Lower: %s\n", socket_lower);
        struct Table routing_table;
                
        initializeTable(&routing_table);
        //NEED TO REMOVE
        addToTable(&routing_table, 10, 10, 1);
        // 
        
        routing_daemon_init(socket_lower, &routing_table);
        return 0;
}
