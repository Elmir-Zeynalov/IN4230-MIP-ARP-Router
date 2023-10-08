#include <bits/types/struct_iovec.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>		/* getifaddrs */
#include <arpa/inet.h>		/* htons */
#include <stdint.h>
#include <sys/socket.h>

#include "cache.h"
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

void print_cache(struct Cache *cache)
{
	printf("[*Printing Cache*]\n");
	int i;
	for(i = 0; i < cache->num_entries; i++)
	{
		printf("MIP: %d\n", cache->entries[i].mip_address);
		printf("MAC: ");
		print_mac_addr(cache->entries[i].mac_address, 6);
		printf("Interface: ");
		print_mac_addr(cache->entries[i].interface.sll_addr, 6);
	}
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

int handle_arp_packet(struct Cache *cache, struct ifs_data *ifs, uint8_t *my_mip_addr)
{
	struct sockaddr_ll so_name;
	struct ether_frame	frame_hdr;
	struct msghdr	msg = {0};
	struct iovec	msgvec[3];
	struct mip_header	header;
	struct mip_sdu		sdu;
	int rc;
	
	/* Frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);
	
	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);

	msgvec[2].iov_base	= &sdu;
	msgvec[2].iov_len	= sizeof(struct mip_sdu);

	msg.msg_name	=	&so_name;
	msg.msg_namelen	=	sizeof(struct sockaddr_ll);
	msg.msg_iovlen	=	3;
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
	
	printf("******************Checking received package***********************\n");
	print_mip_header(&header);
	print_mip_sdu(&sdu);

	printf("Checking if the the Message is meant for me?\n");
	
	if(header.dst_addr == *my_mip_addr)
	{
		printf("This message was meant for me\n");
		//handle message;
		if(sdu.type == MIP_TYPE_RESPONSE)
		{
			printf("IT is a response to a Broadcast from me!\nResponse is from : %d\n", sdu.mip_addr);
			//update cache();
			//
			struct sockaddr_ll tmp;

			for (int i = 0; i < ifs->ifn; i++) {
				if (ifs->addr[i].sll_ifindex == (&so_name)->sll_ifindex)
				memcpy(tmp.sll_addr, ifs->addr[i].sll_addr, 6);
			}
			addToCache(cache, sdu.mip_addr, frame_hdr.src_addr, tmp);
			print_cache(cache);
		}

	}else if(header.dst_addr == 0xFF && sdu.type == MIP_TYPE_REQUEST)
	{
		printf("This is a broadcast/MIP-ARP message\n");
		if(sdu.mip_addr == *my_mip_addr)
		{
			printf("This broadcast was looking for Me!!\n");
			//update cache();
			
			struct sockaddr_ll tmp;

			for (int i = 0; i < ifs->ifn; i++) {
				if (ifs->addr[i].sll_ifindex == (&so_name)->sll_ifindex)
				memcpy(tmp.sll_addr, ifs->addr[i].sll_addr, 6);
			}
			addToCache(cache, header.src_addr, frame_hdr.src_addr, tmp);
			print_cache(cache);
			send_arp_response(ifs,&so_name, frame_hdr, &header, &sdu, my_mip_addr);
		}else
		{
			printf("This boradcast was not meant for me :(\n");
		}
	}

	return 1;
}


/*
 * Sends an arp response to a requester on the same interface 
 * on which the request came from. The message send back contains
 * a filled out PDU (header + SDU).
 *
 * ifs: pointer to interfaces
 * so_name: 
 * frame: ethernet frame that we reuse
 * m_header: MIP header pointer from request
 * m_sdu: MIP SDU pointer from request
 * my_mip_addr: the MIP address of of the running daemon
 *
 * Returns a positve number if all goes well. Otherwise -1
 *
 *
 */
int send_arp_response(struct ifs_data *ifs, struct sockaddr_ll *so_name, struct ether_frame frame, struct mip_header *m_header, struct mip_sdu *m_sdu, uint8_t *my_mip_addr)
{
	struct msghdr *msg;
	struct iovec msgvec[3];
	struct mip_header response_header;
	struct mip_sdu response_sdu;
	
	int rc;
	
	//Stolen code from Plenum
	/* Swap MAC addresses of the ether_frame to send back (unicast) the ARP response */
	memcpy(frame.dst_addr, frame.src_addr, 6);

	/* Find the MAC address of the interface where the broadcast packet came
	 * from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == so_name->sll_ifindex)
		memcpy(frame.src_addr, ifs->addr[i].sll_addr, 6);
	}
	/* Match the ethertype in packet_socket.c: */
	frame.eth_proto[0] = frame.eth_proto[1] = 0xFF;
	
	//Building the MIP PDU
	response_header = construct_mip_header(m_header->src_addr, *my_mip_addr, 12, 32, MIP_ARP);
	response_sdu	= construct_mip_sdu(MIP_TYPE_RESPONSE, m_sdu->mip_addr);

	/* Point to frame header */
	msgvec[0].iov_base	= &frame;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	msgvec[1].iov_base	= &response_header;
	msgvec[1].iov_len	= sizeof(struct mip_header);

	msgvec[2].iov_base	= &response_sdu;
	msgvec[2].iov_len	= sizeof(struct mip_sdu);
	
	printf("\n[Printing ARP_RESPONSE]\n\n");
	print_mip_header(&response_header);
	print_mip_sdu(&response_sdu);

	/* Allocate a zeroed-out message info struct */
	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

	/* Fill out message metadata struct */
	msg->msg_name	 = so_name;
	msg->msg_namelen = sizeof(struct sockaddr_ll);
	msg->msg_iovlen	 = 3;
	msg->msg_iov	 = msgvec;

	/* Construct and send message */
	rc = sendmsg(ifs->rsock, msg, 0);
	if (rc == -1) {
		perror("sendmsg");
		free(msg);
		return -1;
	}

	/*
	printf("Nice to meet you ");
	print_mac_addr(frame.dst_addr, 6);

	printf("I am ");
	print_mac_addr(frame.src_addr, 6);
	*/

	/* Remember that we allocated this on the heap; free it */
	free(msg);

	return rc;
}


/*
 * Method that sends a ARP Request(broadcast) on all interfaces.
 * 
 * ifs: interfaces available on current host
 * src_mip: the MIP address of the current host
 * dst_mip: the MIP address we are trying to reach/find
 *
 *  Here we construct a MIP package with a header and SDU, and broadcast it
 * on all interfaces in order to find the dst_mip
 *
 * return if error then -1
 *	else some non negative number
 *
 * Uppon sucess we end up sending a MIP-Request message on all interfaces.
 */
int send_arp_request(struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip)
{
	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	int rc;
	
	uint8_t dst_addr[] = ETH_BROADCAST;
	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;// htons(ETH_P_MIP);

	struct mip_header	header;
	struct mip_sdu		sdu;
	
	header = construct_mip_header(0xFF, *src_mip, 1, 32, MIP_ARP);
	sdu = construct_mip_sdu(MIP_TYPE_REQUEST, *dst_mip);		

	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);
	
	/* MIP SDU */
	msgvec[2].iov_base	= &sdu;
	msgvec[2].iov_len	= sizeof(struct mip_sdu);

	/*
	 * We loop over all of the interfaces and send a message on each interface.
	 */
	int i;

	printf("---\tSending Broadcast:ARP-REQUEST to find %u\n", *dst_mip);	
	print_mip_header(&header);
	print_mip_sdu(&sdu);
	printf("-----------------------\n");

	for(i = 0; i < ifs->ifn; i++)
	{
		printf("***\nSending ARP-REQUEST messge on interface-> ");
		print_mac_addr(ifs->addr[i].sll_addr, 6);
		memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);

		msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
		msg->msg_name		= &(ifs->addr[i]);
		msg->msg_namelen	= sizeof(struct sockaddr_ll);
		msg->msg_iovlen		= 3;
		msg->msg_iov		= msgvec;
		
		rc = sendmsg(ifs->rsock, msg, 0);
		if(rc == -1)
		{
			perror("sendmsg");
			free(msg);
			return -1;
		}
		free(msg);
	}
	
	return rc;
}


int send_msg(struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip, char *buf, size_t buf_len)
{
	int in_cache = 0;
	int rc;
	if(in_cache)
	{
		//send over known interface
		printf("The mippo is in the cacho\n");

	}else{
		//we need to broadcast a message ...... 
		printf("Damn, cache miss.... Need to broadcast.\n");
		rc = send_arp_request(ifs, src_mip, dst_mip); 
	}

	return rc;
}
