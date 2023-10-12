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
#include "ping_utilities.h"
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
		printf("Index: %d\n", cache->entries[i].index); //interface.sll_addr, 6);
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


int handle_arp_packet(struct Cache *cache, struct ifs_data *ifs, uint8_t *my_mip_addr)
{
	struct sockaddr_ll so_name;
	struct ether_frame	frame_hdr;
	struct msghdr	msg = {0};
	struct iovec	msgvec[3];
	struct mip_header	header;
	uint8_t buf[255];

	int rc;
	memset(buf, 0, sizeof(buf));	
	/* Frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);
	
	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);

	msgvec[2].iov_base	= buf;
	msgvec[2].iov_len	= 255;//sizeof(struct mip_sdu);
	
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
	printf("----%s----\n", buf);
	uint8_t valz;

	//printf("Before memcpy: buf[0] = %u, valz = %u\n", buf[0], valz);
	memcpy(&valz, buf, 1);
	//printf("After memcpy: buf[0] = %u, valz = %u\n", buf[0], valz);

	printf("***\n Received Message from link: ");
	print_mac_addr(frame_hdr.src_addr, 6);
	printf("***\n\n");
	
	printf("******************Checking received package***********************\n");
	print_mip_header(&header);
//	printf("Value: %u\n", *((unsigned int *)buf));
	printf("Value: %u\n", valz);

	printf("header.len = %d\n", header.sdu_len);
	printf("Checking if the the Message is meant for me?\n");
	if(header.sdu_type == MIP_ARP)
	{
		printf("Its an ARP Message\n");
		if(header.dst_addr == *my_mip_addr){
			printf("For Me :)\n");
			if(valz == header.src_addr){
				printf("Response to a broadcast\n");
				int index;
				for (int i = 0; i < ifs->ifn; i++) {
					if (ifs->addr[i].sll_ifindex == (&so_name)->sll_ifindex){
						index = (&so_name)->sll_ifindex; 
					}
				}
				printf("Cache index %d\n", index);
				addToCache(cache, header.src_addr, frame_hdr.src_addr, index);
				print_cache(cache);
			}
		}

		if(header.dst_addr == 0xFF){
			printf("This is a broadcast message\n");
			if(valz == *my_mip_addr){
				printf("Broadcast was meant for me!\n");
				
				struct sockaddr_ll tmp;
				int index;
				for (int i = 0; i < ifs->ifn; i++) {
					if (ifs->addr[i].sll_ifindex == (&so_name)->sll_ifindex){
						index = (&so_name)->sll_ifindex;
						memcpy(tmp.sll_addr, ifs->addr[i].sll_addr, 6);
					}
				}
				printf("Cache index %d == %d\n", index, tmp.sll_ifindex);
				addToCache(cache, header.src_addr, frame_hdr.src_addr, index);
				print_cache(cache);
				send_arp_response(ifs,&so_name, frame_hdr, &header,*my_mip_addr, header.sdu_len);
			}else {
				printf("Broadcast was not meant for me, ignoring...\n");
			}
		}
	}else if(header.sdu_type == MIP_PING)
	{
		printf("*** ITS A TYPE PING!*** \n");
		printf("Printing buffer [%s]\n", buf);

		struct information info;
		memcpy(info.message,buf,sizeof(info.message));
		info.destination_host = header.src_addr; //Who we will reply to later

	        rc = write(ifs->unix_sock, &info, sizeof(struct information));
		printf("Printing UNIX: %d\n", ifs->unix_sock);
		if(rc <= 0)
		{
			close(ifs->unix_sock);
			printf("<%d> has lef thte chat...\n", ifs->unix_sock);
			return -1;
		}
		printf("Sending message to Pong_server Application. Adios!\n");
	}else {
		printf("Undefined message.\n");
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
 * m_header: MIP header pointer from request:
 * m_sdu: MIP SDU pointer from request
 * my_mip_addr: the MIP address of of the running daemon
 *
 * Returns a positve number if all goes well. Otherwise -1
 *
 *
 */
int send_arp_response(struct ifs_data *ifs, struct sockaddr_ll *so_name, struct ether_frame frame, struct mip_header *m_header, uint8_t my_mip_addr, size_t len)
{
	struct msghdr *msg;
	struct iovec msgvec[3];
	struct mip_header response_header;
	//struct mip_sdu response_sdu;
	int rc;
	uint8_t *address = &my_mip_addr;
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
	response_header = construct_mip_header(m_header->src_addr, my_mip_addr, 12, len, MIP_ARP);
	//response_sdu	= construct_mip_sdu(MIP_TYPE_RESPONSE, m_sdu->mip_addr);
//	uint8_t sdu_mip = m_sdu->mip_addr;

	/* Point to frame header */
	msgvec[0].iov_base	= &frame;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	msgvec[1].iov_base	= &response_header;
	msgvec[1].iov_len	= sizeof(struct mip_header);

	msgvec[2].iov_base	= address;// &response_sdu;
	msgvec[2].iov_len	= sizeof(address); //sizeof(struct mip_sdu);
	
	printf("\n[Printing ARP_RESPONSE]\n\n");
	print_mip_header(&response_header);
	printf("SDU: [%d]\nSize: [%lu]\n", *address, len);
	//print_mip_sdu(&response_sdu);

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
int send_arp_request(struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip)
{
	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	int rc;
	uint8_t *address = &dst_mip;
		
	uint8_t dst_addr[] = ETH_BROADCAST;
	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;// htons(ETH_P_MIP);

	struct mip_header	header;
	//struct mip_sdu		sdu;
	
	header = construct_mip_header(0xFF, *src_mip, 1, sizeof(uint8_t), MIP_ARP);
	//sdu = construct_mip_sdu(MIP_TYPE_REQUEST, *dst_mip);		

	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);
	
	/* MIP SDU */
	msgvec[2].iov_base	= address;
	msgvec[2].iov_len	= sizeof(address);

	/*
	 * We loop over all of the interfaces and send a message on each interface.
	 */
	int i;

	printf("---\tSending Broadcast:ARP-REQUEST to find %u\n", dst_mip);	
	print_mip_header(&header);
	printf("SDU: [%d] SIZE: [%lu]", dst_mip, sizeof(dst_mip));
	//print_mip_sdu(&sdu);
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

int send_ping_message(struct CacheEntry *cache_entry, struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip, char *buf, size_t buf_len)
{

	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	int rc;

	struct sockaddr_ll socka;

	/* Find the MAC address of the interface where the broadcast packet came
	 * from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == cache_entry->index){
			memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
			print_mac_addr(ifs->addr[i].sll_addr,6);
			socka = ifs->addr[i];
		}
	}
	printf("Creating PING MEssage\n");
//	memcpy(frame_hdr.src_addr, cache_entry->interface.sll_addr, 6);
	memcpy(frame_hdr.dst_addr, cache_entry->mac_address, 6);

//	printf("Interface: ");
//	print_mac_addr(cache_entry->interface.sll_addr, 6);
	printf("Mac: ");
	print_mac_addr(cache_entry->mac_address, 6);
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;// htons(ETH_P_MIP);

	struct mip_header	header;
	struct mip_sdu		sdu;
	
	header = construct_mip_header(cache_entry->mip_address, *src_mip, 1, 32, MIP_PING);
	//sdu = construct_mip_sdu(MIP_TYPE_REQUEST, *dst_mip);		

	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);
		
	/* MIP SDU */
	msgvec[2].iov_base	= buf;
	msgvec[2].iov_len	= buf_len;


	print_mip_header(&header);
	printf("Buffer: [%s]\nStrlen buffer: [%lu]\n", buf, strlen(buf));
	printf("-----------------------\n");

	printf("***\nSending [PING] messge on interface-> ");
//	print_mac_addr(cache_entry->interface.sll_addr, 6);
//	memcpy(frame_hdr.src_addr, cache_entry->interface.sll_addr, 6);
	
	msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
	msg->msg_name		= &(socka);
	msg->msg_namelen	= sizeof(struct sockaddr_ll);
	msg->msg_iovlen		= 3;
	msg->msg_iov		= msgvec;
	printf("Sending Msg\n");

	rc = sendmsg(ifs->rsock, msg, 0);
	if(rc == -1)
	{
		perror("sendmsg");
		free(msg);
		return -1;
	}
	free(msg);
	
	return rc;


// Define the structure for a cache entry
}

int send_msg(struct Cache *cache, struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, char *buf, size_t buf_len)
{
	int rc;
	struct CacheEntry *cache_entry = isInCache(cache, dst_mip);
	int in_cache = cache_entry != NULL;
	printf("ParamCheck: [%d] [IN_CACHE: %d]\n", dst_mip, in_cache);
	
	if(in_cache)
	{
		
		//send over known interface
		printf("The mippo is in the cacho\n");
		printf("printing ping i am tryint to send [%s]\n", buf);
		send_ping_message(cache_entry, ifs, src_mip, &dst_mip, buf, buf_len);
		
	}else{
		//we need to broadcast a message ...... 
		printf("Damn, cache miss.... Need to broadcast.\n");
		rc = send_arp_request(ifs, src_mip, dst_mip); 
	}

	return rc;
}
