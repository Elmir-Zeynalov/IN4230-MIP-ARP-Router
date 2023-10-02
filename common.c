#include <bits/types/struct_iovec.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>		/* getifaddrs */
#include <arpa/inet.h>		/* htons */
#include <stdint.h>
#include <sys/socket.h>

#include "common.h"
#include "mip.h"


/*
 * Print MAC address in hex format
 */
void print_mac_addr(uint8_t *addr, size_t len)
{
	size_t i;

	for (i = 0; i < len - 1; i++) {
		printf("%02x:", addr[i]);
	}
	printf("%02x\n", addr[i]);
}

/*
 * This function stores struct sockaddr_ll addresses for all interfaces of the
 * node (except loopback interface)
 */
void get_mac_from_interfaces(struct ifs_data *ifs)
{
	struct ifaddrs *ifaces, *ifp;
	int i = 0;

	/* Enumerate interfaces: */
	/* Note in man getifaddrs that this function dynamically allocates
	   memory. It becomes our responsability to free it! */
	if (getifaddrs(&ifaces)) {
		perror("getifaddrs");
		exit(-1);
	}

	/* Walk the list looking for ifaces interesting to us */
	for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
		/* We make sure that the ifa_addr member is actually set: */
		if (ifp->ifa_addr != NULL &&
		    ifp->ifa_addr->sa_family == AF_PACKET &&
		    strcmp("lo", ifp->ifa_name))
		/* Copy the address info into the array of our struct */
		memcpy(&(ifs->addr[i++]),
		       (struct sockaddr_ll*)ifp->ifa_addr,
		       sizeof(struct sockaddr_ll));
	}
	/* After the for loop, the address info of all interfaces are stored */
	/* Update the counter of the interfaces */
	ifs->ifn = i;

	/* Free the interface list */
	freeifaddrs(ifaces);
}

void init_ifs(struct ifs_data *ifs, int rsock)
{
	/* Walk through the interface list */
	get_mac_from_interfaces(ifs);

	/* We use one RAW socket per node */
	ifs->rsock = rsock;
}

int create_raw_socket(void)
{
	int sd;
	short unsigned int protocol = 0xFFFF;

	sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if(sd == -1)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}
	return sd;
}


/*
 * Function to check if a given MAC address matches the broadcast address
 *
 * return: true (1) or false (0)
 */
int check_if_broadcast(uint8_t *mac_address)
{
	uint8_t broadcast[] = ETH_BROADCAST;
	int i;

	for(i = 0; i < MAC_ADDR_LENGTH; i++){
		if(mac_address[i] != broadcast[i])
			return 0;
	}
	return 1;
}

int handle_arp_packet(struct ifs_data *ifs)
{
	struct sockaddr_ll so_name;
	
	struct ether_frame	frame_hdr;
	struct mip_frame	mip_hdr;

	struct msghdr	msg = {0};
	struct iovec	msgvec[2];
	int rc;

	/* Frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);
	
	/* MIP header */
	msgvec[1].iov_base	= &mip_hdr;
	msgvec[1].iov_len	= sizeof(struct mip_frame);
	

	msg.msg_name	=	&so_name;
	msg.msg_namelen	=	sizeof(struct sockaddr_ll);
	msg.msg_iovlen	=	2;
	msg.msg_iov	=	msgvec;
	
	printf("Receving message with MIP header\n");
	/* Receive ARP message */
	rc = recvmsg(ifs->rsock, &msg, 0);
	if(rc == -1)
	{
		perror("sendmsg");
		return -1;
	}

	printf("Checking  header\n");
	print_mip_frame(&mip_hdr);

	return 1;
}


int send_arp_request(struct ifs_data *ifs)
{
	struct sockaddr_ll so_name;
	
	struct ether_frame	frame_hdr;
	struct mip_frame	mip_hdr;

	struct msghdr	*msg;
	struct iovec	msgvec[2];
	int rc;
	
	uint8_t dst_addr[] = ETH_BROADCAST;
	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	memcpy(frame_hdr.src_addr, ifs->addr[0].sll_addr, 6);
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;

	memcpy(mip_hdr.dst_addr, dst_addr, 6);
	memcpy(mip_hdr.src_addr, ifs->addr[0].sll_addr, 6);
	mip_hdr.ttl = 1;
	mip_hdr.sdu_len = 32;
	mip_hdr.sdu_type = 1;

	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &mip_hdr;
	msgvec[1].iov_len	= sizeof(struct mip_frame);

	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

	/*
	 * Might have to send the message on ALL the interfaces ?? 
	 */
If you want to send a message on multiple interfaces, you'll need to call sendmsg separately for each interface.

	printf("Interfaces: %d\n", ifs->ifn);	
	msg->msg_name		= &(ifs->addr[0]);
	msg->msg_namelen	= sizeof(struct sockaddr_ll);
	msg->msg_iovlen		= 2;
	msg->msg_iov		= msgvec;

	printf("We are sending the arp request message (aka. Broadcast message)\n");
	rc = sendmsg(ifs->rsock, msg, 0);
	if(rc == -1)
	{
		perror("sendmsg");
		free(msg);
		return -1;
	}

	free(msg);
	return rc;
}
