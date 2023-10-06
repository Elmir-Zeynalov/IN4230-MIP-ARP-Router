#include <bits/types/struct_iovec.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
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
	short unsigned int protocol = 0xFFFF;//ETH_P_MIP;

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

int handle_arp_packet(struct ifs_data *ifs, uint8_t *my_mip_addr)
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
	printf("***\n Received Message from link: ");
	print_mac_addr(frame_hdr.src_addr, 6);
	printf("***\n\n");
	printf("Checking  header\n");
	print_mip_frame(&mip_hdr);

	printf("Checking if the the Message is meant for me?\n");
	int mip_meant_for_me = mip_hdr.dst_addr == *my_mip_addr;
	printf("Result: %d\n", mip_meant_for_me);
	if(mip_meant_for_me)
	{
		send_arp_response(ifs,&so_name, frame_hdr, mip_hdr);
	}

	return 1;
}

int send_arp_response(struct ifs_data *ifs, struct sockaddr_ll *so_name, struct ether_frame frame, struct mip_frame mip_hdr)
{
	struct msghdr *msg;
	struct iovec msgvec[2];

	int rc;

	/* Swap MAC addresses of the ether_frame to send back (unicast) the ARP
	 * response */
	memcpy(frame.dst_addr, frame.src_addr, 6);

	/* Find the MAC address of the interface where the broadcast packet came
	 * from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == so_name->sll_ifindex)
		memcpy(frame.src_addr, ifs->addr[i].sll_addr, 6);
	}
	/* Match the ethertype in packet_socket.c: */
	frame.eth_proto[0] = frame.eth_proto[1] = 0xFF;

	/* Point to frame header */
	msgvec[0].iov_base = &frame;
	msgvec[0].iov_len  = sizeof(struct ether_frame);

	msgvec[1].iov_base	= &mip_hdr;
	msgvec[1].iov_len	= sizeof(struct mip_frame);
	
	printf("\n[Printing ARP_RESPONNEE]\n\n");
	print_mip_frame(&mip_hdr);
	printf("**********************\n\n");
	/* Allocate a zeroed-out message info struct */
	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

	/* Fill out message metadata struct */
	msg->msg_name	 = so_name;
	msg->msg_namelen = sizeof(struct sockaddr_ll);
	msg->msg_iovlen	 = 2;
	msg->msg_iov	 = msgvec;

	/* Construct and send message */
	rc = sendmsg(ifs->rsock, msg, 0);
	if (rc == -1) {
		perror("sendmsg");
		free(msg);
		return -1;
	}

	printf("Nice to meet you ");
	print_mac_addr(frame.dst_addr, 6);

	printf("I am ");
	print_mac_addr(frame.src_addr, 6);

	/* Remember that we allocated this on the heap; free it */
	free(msg);

	return rc;
}






/*
 * Method that sends a broadcast message on all interfaces.
 *
 * 
 */
int send_arp_request(struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip)
{
	struct sockaddr_ll so_name;
	
	struct ether_frame	frame_hdr;
	struct mip_frame	mip_hdr;

	struct msghdr	*msg;
	struct iovec	msgvec[2];
	int rc;
	
	uint8_t dst_addr[] = ETH_BROADCAST;
	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	//memcpy(frame_hdr.src_addr, ifs->addr[0].sll_addr, 6);
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;// htons(ETH_P_MIP);
	
	printf("printing some stuff [DST_MIP] = %u\n", *dst_mip);
	printf("printing some stuff [SRC_MIP] = %u\n", *src_mip);
	mip_hdr.dst_addr = *dst_mip;
	mip_hdr.src_addr = *src_mip;

	mip_hdr.ttl = 1;
	mip_hdr.sdu_len = 32;
	mip_hdr.sdu_type = MIP_ARP;
	
	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &mip_hdr;
	msgvec[1].iov_len	= sizeof(struct mip_frame);

	/*
	 * We loop over all of the interfaces and send a message on each interface.
	 */
	int i;
	printf("Interfaces: %d\n", ifs->ifn);	
	printf("Sending on interface: \n");
	for(i = 0; i < ifs->ifn; i++)
	{
		print_mac_addr(ifs->addr[i].sll_addr,6);
		printf("\t-%d: %d:%d:%d:%d:%d:%d\n",i, 
			ifs->addr[i].sll_addr[0],
			ifs->addr[i].sll_addr[1],
			ifs->addr[i].sll_addr[2],
			ifs->addr[i].sll_addr[3],
			ifs->addr[i].sll_addr[4],
			ifs->addr[i].sll_addr[5]);
		memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);

		msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));

		msg->msg_name		= &(ifs->addr[i]);
		msg->msg_namelen	= sizeof(struct sockaddr_ll);
		msg->msg_iovlen		= 2;
		msg->msg_iov		= msgvec;
		printf("Message is constructed!\n");
		
		printf("***\nWe are sending the arp request message (aka. Broadcast message)" 
			"On interface: ");
		print_mac_addr(ifs->addr[i].sll_addr, 6);
		printf("***\n\n");

		rc = sendmsg(ifs->rsock, msg, 0);
		if(rc == -1)
		{
			perror("sendmsg");
			free(msg);
			return -1;
		}
		free(msg);
	}


	//free(msg);
	return rc;
}
