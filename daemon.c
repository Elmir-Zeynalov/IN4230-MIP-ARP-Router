#include <stdint.h>
#include <stdlib.h>		/* free */
#include <stdio.h>		/* printf */
#include <unistd.h>		/* fgets */
#include <string.h>		/* memset */
#include <sys/socket.h>	/* socket */
#include <fcntl.h>
#include <sys/epoll.h>			/* epoll */
#include <linux/if_packet.h>	/* AF_PACKET */
#include <net/ethernet.h>		/* ETH_* */
#include <arpa/inet.h>			/* htons */
#include <ifaddrs.h>			/* getifaddrs */

#include "cache.h"
#include "common.h"
#include "queue.h"
#include "unix_utils.h"
#include "ping_utilities.h"
#include "debug.h"

int main(int argc, char *argv[])
{
	int opt;
	int hflag = 0;
	int dflag = 0;
	char *socket_upper = NULL;
	uint8_t MIP_address;
	
	struct Cache cache;
	struct Queue queue;
	struct Queue broadcast_queue; 
	
	//Init queues + cache
	initializeCache(&cache);
	initializeQueue(&queue);
	initializeQueue(&broadcast_queue);

	struct ifs_data local_ifs;
	int raw_sock, rc;
	int unix_sock;
	int accept_sd;

	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd;

	while((opt = getopt(argc, argv, "dh")) != -1)
	{
		switch (opt) {
			case 'h':
				hflag = 1;
				break;
			case 'd':
				dflag = 1;
				break;
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	debug_flag = dflag; 
	if(hflag)
	{
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if(dflag && argc < 4)
	{
		printf("Dflag and missing reqs\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if(!dflag && argc < 3)
	{
		printf("NO Dflag and missing args\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	socket_upper = argv[dflag ? 2 : 1];
	MIP_address = atoi(argv[dflag ? 3 : 2]);
	
	if(dflag) 
	{
		printf("[<info>] Socket Upper: %s [<info>]\n", socket_upper);
		printf("[<info>] MIP Address: %d [<info>]\n", MIP_address);
	}

	raw_sock = create_raw_socket();

	init_ifs(&local_ifs, raw_sock);
	if(dflag)
	{

		printf("[<info>] Printing all interfaces [<info>]\n");
		for(int i = 0; i < local_ifs.ifn; i++) {
			printf("[*] ");
			print_mac_addr(local_ifs.addr[i].sll_addr,6);
			printf("[*]\n");
		}
		printf("\n");
	}

	/* Set up epoll */
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		close(raw_sock);
		exit(EXIT_FAILURE);
	}


	/* Add RAW socket to epoll table */
	ev.events = /*EPOLLIN|*/EPOLLHUP;
	ev.data.fd = raw_sock;
	rc = add_to_epoll_table(epollfd, &ev, raw_sock);
	if(rc)
	{

	}
	/* Add Unix socket to epoll table */
	unix_sock = prepare_server_sock(socket_upper);
	
	rc = add_to_epoll_table(epollfd, &ev, unix_sock);
	if(rc)
	{

	}
	/* epoll_wait forever for incoming packets */
	while(1) {
		rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			break;
		} else if (events->data.fd == raw_sock) {
			/* Someone triggered event on raw socket */
			if(debug_flag) printf("[<info>] [RAW-SOCKET] The neighbor is initiating a MIP-ARP handshake [<info>]\n");

			//rc = handle_arp_packet(&cache, &queue,  &local_ifs, &MIP_address);
			rc = forwarding_engine(&cache, &queue, &broadcast_queue, &local_ifs, &MIP_address);
			if (rc < 1) {
				perror("recv");
				break;
			}
		} else if(events->data.fd == unix_sock){
			/* Someone knocking on the UNIX socket */
			accept_sd = accept(unix_sock, NULL, NULL);
			if (accept_sd == -1) {
				perror("accept");
				continue;
			}
			if(1) printf("[<info>] [UNIX-SOCKET] Fd: %d: Has connected to the Daemon [<info>]\n", accept_sd);
			
			/* Add the new socket to epoll table */
			rc = add_to_epoll_table(epollfd, &ev, accept_sd);
			if (rc == -1) {
				close(unix_sock);
				exit(EXIT_FAILURE);
			}
		}else{
			/* Someone has triggered an event on an existing connection */
			/* Who exactly is sending messages is stored in local_ifs.unix_sock */
			if(debug_flag) printf("[<info>] [UNIX-SOCKET] Existing client has sent a message [<info>]\n");
			
			/* if the socket is one we already know then we just handle the message. 
			* Otherwise we try identify if its the routing-daemon or the ping/pong client
			*/
			if(events->data.fd == local_ifs.routin_sock || events->data.fd == local_ifs.unix_sock)
			{
				//if we are then we know the the incoming message is on one of the UNIX sockets
				handle_client(&cache, &queue, &broadcast_queue, events->data.fd, &local_ifs, MIP_address);
			}else {
				//upon first connection  (we try to figure out who is who)
				determine_unix_connection(events->data.fd, &local_ifs);
			}
		}
	}

	close(raw_sock);
	unlink(SOCKET_NAME);
	return 0;
}
