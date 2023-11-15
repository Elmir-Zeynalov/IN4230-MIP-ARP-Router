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

#include "utilities.h"
#include "cache.h"
#include "common.h"
#include "mip.h"
#include "ping_utilities.h"
#include "queue.h"
#include "debug.h"


/*
 * Method that is used by the Daemon to distinguish who the connecting party is.
 * Checks the application_type to determine if its the PING_CLIENT or the ROUTING_DAEMON,
 * and then assigns the correct pointer to the "socket connection" for later use. 
 */
int determine_unix_connection(int fd, struct ifs_data *local_ifs)
{
	int rc;
	uint8_t application_type;
	rc = read(fd, &application_type, sizeof(uint8_t));
	if(rc <= 0)
	{
		perror("sock read()");
		close(fd);
		return -1;
	}

	printf("Upper Layer Application Type: [%s]\n", application_type == 0x04 ? "Routing-Daemon" : "PING/CLIENT");
	
	if(application_type == 0x04)
	{
		local_ifs->routin_sock = fd;
	}
	else{
		local_ifs->unix_sock = fd;
	}
	return rc;
}

/*
 * Method to handle incomming messages on the UNIX socket initiated either by the client or server applications.
 * The procedure here is simple. We just read the incoming message and then pass it on to our send_msg() funciton,
 * which takes care of checking our cache and then making a decision on whether the to fordard the application 
 * message or find the destination host. 
 *
 * cache: MIP cache that stores MIP-addresses and MACs
 * queue: queue of messages that is used when we have to broadcast
 * fd: file descriptor
 * local_ifs: structure that holds data on sockets
 * MIP_address: MIP of the local Daemon. "MY MIP" 
 *
 * Function reads message from unix socket and passes the message to the send_msg() function which handles the checking of 
 * the cache in order to determine if we need to broadcast (to find the MIP address) or if we can send the message
 * directly (because the host is already known to us).
 */
void handle_client(struct Cache *cache, struct Queue *queue, int fd, struct ifs_data *local_ifs, uint8_t MIP_address)
{
        char buf[256];
        int rc;
        struct information received_info;

        memset(buf,0,sizeof(buf));
        //rc = read(fd, &received_info, sizeof(struct information));
	if(fd == local_ifs->unix_sock)
	{
		rc = read(local_ifs->unix_sock, &received_info, sizeof(struct information));
		if(rc <= 0)
		{
			close(fd);
			printf("<%d> has lef the chat...\n", fd);
			local_ifs->unix_sock = -1;
			return;
		}
		if(debug_flag)
		{
			printf("[<info>] Received a *%s* message [<info>]\n", (strncmp(received_info.message, "PONG:", 4) == 0) ? "PONG" : "PING"); 
			printf("[<info>] Received destination host: [%d] [<info>]\n", received_info.destination_host);
			printf("[<info>] Received message: [%s] [<info>]\n", received_info.message);
			printf("[<info>] Received TTL [%d] [<info>]\n", received_info.ttl);
		}
		//pass the information after reading it from the socket
		if((strncmp(received_info.message, "PONG:", 4) == 0)) {
			printf("Overwriting TTL of PONG\n");
			received_info.ttl = DEFAULT_TTL;
		}
		send_msg(cache, queue, local_ifs, &MIP_address, received_info.destination_host, received_info.message, 255, received_info.ttl);
	}else{
		printf("[<info>] Message from routing daemon [<info>]\n"); 
	}
}

/*
* Method that checks whether a given MIP is in the cache and then sends either a PING message or a broadcast
* cache: MIP-Mac cache
* queue: message queue (we use this in case we have to broadcast)
* ifs: structure to hold file descriptors and interfaces
* src_mip: source MIP address
* dst_mip: destination MIP address
* buf: message we are trying to relay
* buf_len: lenght of the message
*
* Method performs a simple check against the cache to see if we have an entry. If yes, then we just have to package the message
* and send it on the correct interface (that we store in our cahce)
* Otherwise, we need to store the message and perform a broadcast to fid the the dst_mip. When the approprate dst_mip responds
* we will send the message from the queue in handle_arp_packet(); -- but that is on arrival of a response to the broadcast! 
*
*/
int send_msg(struct Cache *cache, struct Queue *queue, struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, char *buf, size_t buf_len, uint8_t ttl)
{
	int rc;
	struct CacheEntry *cache_entry = isInCache(cache, dst_mip);
	int in_cache = cache_entry != NULL;
	if(debug_flag) printf("[<info>] Sending Message To [%d] [<info>]\n", dst_mip);
		
	if(in_cache)
	{
		//send over known interface
		if(debug_flag) printf("[<info>] Destination MIP is in cache! Sending Message: [%s] To [%d] [<info>]\n", buf, dst_mip);
	
		send_ping_message(cache_entry, ifs, src_mip, &dst_mip, buf, buf_len, cache, ttl);
	}else{
		//we need to broadcast a message ...... 
		if(1) printf("[<info>] Cache miss, need to send a broadcast to find: %d [<info>]\n", dst_mip);
		rc = send_arp_request(ifs, src_mip, dst_mip, cache); 
		
		//Didnt find the MIP in cache so i ended up broadcasting
		//But i need to store the message. 
		if(1) printf("[<info>] Storing message [%s] in buffer until further notice.[<info>]\n", buf);
		if(1) printf("[<info>] TTL: %d\n", ttl);
		addToQueue(queue, dst_mip, buf, buf_len, ttl);
	}

	return rc;
}

/*
 * Packages the message and sends it to the correct host. This is what is used when we have the dest_mip in our cache.
 * 
 * cache_entry: the cache entry which contains the MIP address, MAC address and on which interface we should send the message
 * ifs: structure to hold info on the file descriptors
 * src_mip: source MIP address
 * dst_mip: destination MIP address
 * buf: message we are sending
 * buf_len: the length of the messagge we are sending
 * cache: our cache (mostly used for printing in debug here)
 * 
 * This method here is called when the dst_mip is actually in our cache, enabling us to send the packet on raw sockets to the 
 * appropriate daemon. 
 *
 */
int send_ping_message(struct CacheEntry *cache_entry, struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip, char *buf, size_t buf_len, struct Cache *cache, uint8_t ttl)
{
	if(dst_mip == NULL) { }
	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	struct mip_header	header;
	int rc;

	struct sockaddr_ll socka;

	/* Find the MAC address of the interface where the broadcast packet came
	 * from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == cache_entry->index){
			memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
			//print_mac_addr(ifs->addr[i].sll_addr,6);
			socka = ifs->addr[i];
		}
	}

	memcpy(frame_hdr.dst_addr, cache_entry->mac_address, 6); //USE the cache interface
	//frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = /*0xFF;*/ htons(ETH_P_MIP);
	frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
	frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;
	
	header = construct_mip_header(cache_entry->mip_address, *src_mip, ttl, 32, MIP_PING);
	if(1) printf("[<info>] ttl we put in cache [%d] [<info>]\n", ttl);
	if(1) printf("[<info>] ttl we put in cache [%d] [<info>]\n", header.ttl);
	printf("\n----\n");
	if(debug_flag)
	{
		printf("\n******\n");
		printf("[<info>] Sending Ping packet [<info>]\n");	
		
		printf("[<info>] Source address [<info>]\n");
		printf("\t");
		print_mac_addr(frame_hdr.src_addr, 6);
		printf("\n[<info>] Destination address [<info>]\n");
		printf("\t");
		print_mac_addr(frame_hdr.dst_addr, 6);

	

		printf("\n[<info>] Source MIP [<info>]\n");
		printf("\t%d\n", header.src_addr);
		printf("[<info>] Destination MIP [<info>]\n");
		printf("\t%d\n", header.dst_addr);

		printf("[<info>] Contents of MIP-ARP Cache [<info>]\n");
		print_cache(cache);
		printf("******\n\n");
	}
	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);
		
	/* MIP SDU */
	msgvec[2].iov_base	= buf;
	msgvec[2].iov_len	= buf_len;

	if(debug_flag) 
	{
		printf("\n");
		printf("[<info>] Buffer: [%s]\nStrlen buffer: [%lu] [<info>]\n", buf, strlen(buf));
		printf("\n");
	}

	msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
	msg->msg_name		= &(socka);
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
	
	return rc;
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
	ifs->routin_sock = -1;
	ifs->unix_sock = -1;
}

/* Helper function that loops through the given interfaces in ifs and tries to find a matching index. 
 * Returns the sll_ifindex of that interface. This is how keep track of where the messages come from
 * and how we reply on the same "line". */
int find_interface_from_cache(struct ifs_data *ifs, struct sockaddr_ll *so_name)
{
	int index;
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == (so_name)->sll_ifindex)
			index = (so_name)->sll_ifindex; 
	}
	return index;
}


/*
 * Takes care of incoming messages
 * 
 * cache: MIP cache that stores MIP-addresses and MACs
 * queue: queue of messages that is used when we have to broadcast
 * ifs_data: structure that holds data on 
 * my_mip_addr: my mip address
 * 
 * This method does the heavy lifting when it comes to processing and analyzing incoming messages.
 * it performs checks based on the information in the message header and then takes actions based
 * on that. 
 *
 */
int handle_arp_packet(struct Cache *cache, struct Queue *queue, struct ifs_data *ifs, uint8_t *my_mip_addr)
{
	struct sockaddr_ll so_name;
	struct ether_frame	frame_hdr;
	struct msghdr	msg = {0};
	struct iovec	msgvec[3];
	struct mip_header	header;
	uint8_t valz;
	int rc;
	uint8_t buf[257];
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
		
	if(debug_flag) printf("\n[<info>] Receving message with MIP header [<info>]\n");
	/* Receive ARP message */
	rc = recvmsg(ifs->rsock, &msg, 0);
	if(rc == -1)
	{
		perror("sendmsg");
		return -1;
	}

	memcpy(&valz, buf, 1);
	if(debug_flag)
	{
		printf("******\n");
		printf("[<info>] Source address [<info>]\n");
		printf("\t");
		print_mac_addr(frame_hdr.src_addr, 6);
		printf("\n[<info>] Destination address [<info>]\n");
		printf("\t");
		print_mac_addr(frame_hdr.dst_addr, 6);

		printf("\n[<info>] Source MIP [<info>]\n");
		printf("\t%d\n", header.src_addr);
		printf("[<info>] Destination MIP [<info>]\n");
		printf("\t%d\n", header.dst_addr);
		
		printf("TTL: [%d]\n", header.ttl);
		printf("[<info>] Contents of MIP-ARP Cache [<info>]\n");
		print_cache(cache);
		printf("******\n\n");

	}

	if(debug_flag)
	{
		//print_mip_header(&header);
		printf("[<info>] Checking if the the Message is meant for me? [<info>]\n");
	}

	if(header.sdu_type == MIP_ARP) //check if the Message is of Type ARP
	{
		if(debug_flag) printf("[<info>] Received message. Type: *MIP-ARP* [<info>]\n");

		if(header.dst_addr == *my_mip_addr){
			if(debug_flag) printf("[<info>] Received message is a MIP-ARP message [<info>]\n");
			if(valz == header.src_addr){
				/* The logic here follows the idea that if we get a MIP-ARP type packet with 
				* the destination address == to my MIP address and the SDU == src address 
				* then this has to be a response to a broadcast performed by me. So, a response
				* to a request made by this this (since the dst_addr == my_addr)
				*/
				if(debug_flag) printf("[<info>] This is a response to a broadcast message [<info>]\n");
				
				int interface_index = find_interface_from_cache(ifs, &so_name);
				addToCache(cache, header.src_addr, frame_hdr.src_addr, interface_index);
				//print_cache(cache);

				//I got a response for a broadcast i sent looking for this MIP
				//Now i can empty my queue and send it the PING
				struct QueueEntry *queue_entry = isInQueue(queue, header.src_addr);
				if(queue_entry != NULL){
					//printf("Queue contents:\n");
					//printf("\t-To MIP: [%d]\n\t-Message: [%s]\n\t-Len: [%lu]\n", queue_entry->mip_address, queue_entry->message, queue_entry->len);

					struct CacheEntry *cache_entry = isInCache(cache, header.src_addr);
					//TTL???

					send_ping_message(cache_entry, ifs, my_mip_addr, &queue_entry->mip_address, queue_entry->message, queue_entry->len, cache, queue_entry->ttl);

					deleteFromQueue(queue, header.src_addr);

				}else {
					printf("NOTHING IN QUEUE\n"); //Hopefully we dont end up here...
				}
			}
		}

		if(header.dst_addr == 0xFF){
			if(debug_flag) printf("[<info>] The received message is a broadcast message [<info>]\n");

			if(valz == *my_mip_addr){
				if(debug_flag) printf("[<info>] The broadcast message was meant for me. Someone is looking for me! [<info>]\n");

				int interface_index = find_interface_from_cache(ifs, &so_name);
				addToCache(cache, header.src_addr, frame_hdr.src_addr, interface_index);
				//print_cache(cache);
				send_arp_response(ifs,&so_name, frame_hdr, &header,*my_mip_addr, header.sdu_len);
			}else {
				if(debug_flag) printf("[<info>] The broadcast message was not looking for me, ignoring.... [<info>]\n");
			}
		}
	}else if(header.sdu_type == MIP_PING) //Its a ping message so we need to forward it to our client/server via UNIX
	{
		if(debug_flag) printf("[<info>] Received message. Type: *PING* [<info>]\n");
		if(debug_flag) printf("[<info>] Message: %s [<info>]\n", buf);

		struct information info;
		memcpy(info.message,buf,sizeof(info.message));
		info.destination_host = header.src_addr; //Who we will reply to later
		
		printf("PING: TTL: %d\n", header.ttl);
	        rc = write(ifs->unix_sock, &info, sizeof(struct information));
		if(rc <= 0)
		{
			close(ifs->unix_sock);
			printf("<%d> has lef thte chat...\n", ifs->unix_sock);
			return -1;
		}
		if(debug_flag) printf("[<info>] Forwarding message to Pong-Server Application. [<info>]\n");
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
 * Sends a response to a broadcast message. It first builds the header and fills the required information there.
 * Then the SDU is filled with the address of the current daemon. A way of saying that "Hey, thats me!" 
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
	//frame.eth_proto[0] = frame.eth_proto[1] = /*0xFF;*/ htons(ETH_P_MIP);

	frame.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
	frame.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;
	
	//Building the MIP PDU
	response_header = construct_mip_header(m_header->src_addr, my_mip_addr, DEFAULT_TTL, len, MIP_ARP);

	/* Point to frame header */
	msgvec[0].iov_base	= &frame;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	msgvec[1].iov_base	= &response_header;
	msgvec[1].iov_len	= sizeof(struct mip_header);

	msgvec[2].iov_base	= address;// &response_sdu;
	msgvec[2].iov_len	= sizeof(address); //sizeof(struct mip_sdu);

	if(debug_flag)
	{
		printf("[<info>] Sending response to broadcast [<info>]\n");
		print_mip_header(&response_header);
		printf("[<info>] SDU: [%d] [<info>]\n [<info>] Size: [%lu] [<info>]\n", *address, len);
	}
	/* Allocate a zeroed-out message info struct */
	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));
	printf("RESP Header [%d]\n", response_header.ttl);
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
 * cache: is our MIP/MAC cache
 *
 *  Here we construct a MIP package with a header and SDU, and broadcast it
 * on all interfaces in order to find the dst_mip
 *
 * This is the method that send the broadcast request trying to find a dst_mip. The boradcast message 
 * is sent on ALL interfaces. 
 */
int send_arp_request(struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, struct Cache *cache)
{
	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	int rc;
	uint8_t *address = &dst_mip;
		
	uint8_t dst_addr[] = ETH_BROADCAST;
	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	//frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = /*0xFF;*/ htons(ETH_P_MIP);
	frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
	frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;

	struct mip_header	header;
	//struct mip_sdu		sdu;
	
	header = construct_mip_header(0xFF, *src_mip, DEFAULT_TTL, sizeof(uint8_t), MIP_ARP);

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

	for(i = 0; i < ifs->ifn; i++)
	{
		if(debug_flag)
		{	printf("\n\n[<info>] BROADCAST [<info>]");
			printf("\n******");
			printf("\n[<info>] Sending ARP-REQUEST messge on interface: ");
			print_mac_addr(ifs->addr[i].sll_addr, 6);
			printf("[<info>]\n");
		}

		memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
		if(debug_flag)
		{
			printf("[<info>] Sending Broadcast: ARP-REQUEST to find %u [<info>]\n", dst_mip);	
			printf("[<info>] SDU Content: [%d] SDU SIZE: [%lu] [<info>]", dst_mip, sizeof(dst_mip));
		
			printf("\n[<info>] Source address [<info>]\n");
			printf("\t");
			print_mac_addr(frame_hdr.src_addr, 6);
			printf("\n[<info>] Destination address [<info>]\n");
			printf("\t");
			print_mac_addr(frame_hdr.dst_addr, 6);

			printf("\n[<info>] Source MIP [<info>]\n");
			printf("\t%d\n", header.src_addr);
			printf("[<info>] Destination MIP [<info>]\n");
			printf("\t%d\n", header.dst_addr);
				
			printf("\n[<info>] TTL: %d [<info>]\n", header.ttl);


			printf("[<info>] Contents of MIP-ARP Cache [<info>]\n");
			print_cache(cache);
			printf("******\n\n");
		}		

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
