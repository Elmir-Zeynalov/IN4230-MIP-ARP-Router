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

#include "common.h"

void usage(char *arg)
{
	printf("Usage: %s [-h] [-d] <socket_upper> <MIP_address>\n"
		"Options:\n"
		"\t-h: prints help and exits program\n"
		"\t-d: enables debugging mode\nRequired Arguments:\n"
		"\tsocket_upper: pathname of the UNIX socket used to interface with upper layers\n"
		"\tMIP address: the MIP address to assign to this host\n", arg);
}

int main(int argc, char *argv[])
{
	int opt;
	int hflag = 0;
	int dflag = 0;
	char *socket_upper = NULL;
	char *MIP_address = NULL;
	
	struct ifs_data local_ifs;
	int raw_sock, rc; 

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

	printf("argc: %d\n", argc);
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
	MIP_address = argv[dflag ? 3 : 2];

	printf("Socket Upper: %s\n", socket_upper);
	printf("MIP Address: %s\n", MIP_address);
	
	raw_sock = create_raw_socket();
	init_ifs(&local_ifs, raw_sock);

	for(int i = 0; i < local_ifs.ifn; i++) {
		print_mac_addr(local_ifs.addr[i].sll_addr,6);
	}
	
	/* Set up epoll */
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		close(raw_sock);
		exit(EXIT_FAILURE);
	}

	/* Add RAW socket to epoll */
	ev.events = EPOLLIN|EPOLLHUP;
	ev.data.fd = raw_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, raw_sock, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		close(raw_sock);
		exit(EXIT_FAILURE);
	}


	/* Send ARP request to know the neighbor next door */
	if(strcmp(argv[3], "send"))
	{
		printf("Sending Broadcast on all interfazes\n");
		send_arp_request(&local_ifs);
	}

	/* epoll_wait forever for incoming packets */
	while(1) {
		rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			break;
		} else if (events->data.fd == raw_sock) {
			printf("\n<info> The neighbor is initiating a handshake\n");
			rc = handle_arp_packet(&local_ifs);
			if (rc < 1) {
				perror("recv");
				break;
			}
		}
	}

	close(raw_sock);

	return 0;
}
