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
#include "unix_utils.h"

void usage(char *arg)
{
	printf("Usage: %s [-h] [-d] <socket_upper> <MIP_address>\n"
		"Options:\n"
		"\t-h: prints help and exits program\n"
		"\t-d: enables debugging mode\nRequired Arguments:\n"
		"\tsocket_upper: pathname of the UNIX socket used to interface with upper layers\n"
		"\tMIP address: the MIP address to assign to this host\n", arg);
}

struct information {
    uint8_t destination_host;
    char message[256];
};

void handle_client(struct Cache *cache, struct Queue *queue, int fd, struct ifs_data local_ifs, uint8_t MIP_address)
{
	char buf[256];
	int rc;
	
	struct information received_info;


	memset(buf,0,sizeof(buf));
	//rc = read(fd,buf,sizeof(buf));
	rc = read(fd, &received_info, sizeof(struct information));
	if(rc <= 0)
	{
		close(fd);
		printf("<%d> has lef thte chat...\n", fd);
		return;
	}
	printf("Received destination host: [%d]\n", received_info.destination_host);
	printf("Received message: [%s]\n", received_info.message);
	
	if (strncmp(received_info.message, "PONG:", 4) == 0) {
		printf("The first 4 characters match! So we got a PONG!!\n");
		printf("RELAYYYYY\n");
		printf("PongMsg: [%s]\nInResponseToHost: [%d]\n", received_info.message, received_info.destination_host);
		printf("Trying to relay the PONG response.\n");
		send_msg(cache, queue, &local_ifs, &MIP_address, received_info.destination_host, received_info.message, 255); 	
	} else {
		printf("The first 4 characters do not match.\n");
		send_msg(cache, queue, &local_ifs, &MIP_address, received_info.destination_host, received_info.message, 255);
	}
}


int main(int argc, char *argv[])
{
	int opt;
	int hflag = 0;
	int dflag = 0;
	char *socket_upper = NULL;
	uint8_t MIP_address;
	
	struct Cache cache;
	struct Queue queue;

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
	MIP_address = atoi(argv[dflag ? 3 : 2]);

	printf("Socket Upper: %s\n", socket_upper);
	printf("MIP Address: %d\n", MIP_address);
	
	raw_sock = create_raw_socket();

	printf("Printing all interfaces\n");
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
	printf("epollCreated\n");
	/* Add RAW socket to epoll */
	ev.events = EPOLLIN|EPOLLHUP;
	ev.data.fd = raw_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, raw_sock, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		close(raw_sock);
		exit(EXIT_FAILURE);
	}
	
	unix_sock = prepare_server_sock(socket_upper);
	add_to_epoll_table(epollfd, &ev, unix_sock);

	//local_ifs.unix_sock = unix_sock;	
	printf("UNIX FROM MAIN:%d\n", unix_sock);	
	/*
	 * TODO: Remove this 
	 * This is only used for testing purposes....
	 *
	 */

	uint8_t dest_MIP = 10;
	char buffer[] = "ZZZZZufer Data";
	if(strcmp(argv[3], "send") == 0)
	{
		printf("Sending Broadcast on all interfazes\n");
		//send_arp_request(&local_ifs, &MIP_address, &dest_MIP);
		//send_msg(&cache,&local_ifs, &MIP_address, dest_MIP, buffer, strlen(buffer));
	}
	int accepted_sd = -1;

	/* epoll_wait forever for incoming packets */
	while(1) {
		rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			break;
		} else if (events->data.fd == raw_sock) {
			printf("\n<info> The neighbor is initiating a handshake\n");
			rc = handle_arp_packet(&cache, &queue,  &local_ifs, &MIP_address);
			if (rc < 1) {
				perror("recv");
				break;
			}
		} else if(events->data.fd == unix_sock){
			//A UNIX connection
			//PING CLIENT OR PING SERVER IS trying to connect on main listening socket.
				
			accept_sd = accept(unix_sock, NULL, NULL);
			if (accept_sd == -1) {
				perror("accept");
				continue;
			}

			printf("<%d> joined the chat...\n", accept_sd);

			/* Add the new socket to epoll table */
			rc = add_to_epoll_table(epollfd, &ev, accept_sd);
			if (rc == -1) {
				close(unix_sock);
				exit(EXIT_FAILURE);
			}
			accepted_sd = accept_sd;
			local_ifs.unix_sock = accepted_sd;
		}else{
			//could be either PING or PONG
			printf("Events.data.fd = %d\nAccepted_sd = %d\n", events->data.fd, accepted_sd);
			handle_client(&cache, &queue, events->data.fd, local_ifs, MIP_address);
		}
	}

	close(raw_sock);
	unlink(SOCKET_NAME);
	return 0;
}
