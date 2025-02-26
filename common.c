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

#include "ether.h"
#include "utilities.h"
#include "cache.h"
#include "common.h"
#include "mip.h"
#include "ping_utilities.h"
#include "queue.h"
#include "debug.h"
#include "routing_utils.h"

/*
 * Method that is used by the Daemon to distinguish who the connecting party is. 
 * Checks the application_type to determine if its the PING_CLIENT or the ROUTING_DAEMON,
 * and then assigns the correct pointer to the "socket connection" for later use. 
 *
 * fd: file descriptor that we read from
 * local_if: holds infomration on the different sockets. 
 *	Will be used to set which socket we reply to (routing_daemon or ping_daemon)
 *	routing_sock = routing_daemon_socket
 *	unix_sock = ping_client_socket
 * Returns error if something goes wrong.
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
 * broadcast_queue: the queue used for storing the addresses we are waiting on a response
 * fd: file descriptor
 * local_ifs: structure that holds data on sockets
 * MIP_address: MIP of the local Daemon. "MY MIP" 
 *
 * Function reads message from unix socket and passes the message to the send_msg() function which handles the checking of 
 * the cache in order to determine if we need to broadcast (to find the MIP address) or if we can send the message
 * directly (because the host is already known to us).
 *
 */
void handle_client(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, int fd, struct ifs_data *local_ifs, uint8_t MIP_address)
{
        char buf[256];
        int rc;
        struct information received_info;
	struct packet_ux pu;

        memset(buf,0,sizeof(buf));
        //rc = read(fd, &received_info, sizeof(struct information));
	if(fd == local_ifs->unix_sock)
	{
		printf("[<info>] We got a message on the [PING/PONG] socket. [<info>]\n\n");
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
		printf("[<info>] We got a message on the [ROUTING DEAMON] socket. [<info>]\n\n");
		handle_message_from_routing_daemon(local_ifs, &MIP_address, broadcast_queue, cache);
	}
}

/*
* Method that checks whether a given MIP is in the cache or if we have to perfrom a router lookup
* Takes the job of storing the message from the PING/PONG client before it issues a request to the mip daemon. 
* It will request the dst_mip and will also provide the source address, just in case.
*
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
		if(debug_flag) printf("[<info>] Cache miss, need to send a broadcast to find: %d [<info>]\n", dst_mip);
		if(debug_flag) printf("[<info>] Storing message [%s] in buffer until further notice.[<info>]\n", buf);
		addToQueue(queue, *src_mip, dst_mip, buf, buf_len, ttl);
		if(debug_flag) printf("[<info>] Making Request to Routing Daemon to find [%d].[<info>]\n", dst_mip);

		lookup_request(ifs->routin_sock, *src_mip, dst_mip);	
	}

	return rc;
}

/*
 * Packages the message and sends it to the directely correct host. This is what is used when we have the dest_mip in our cache.
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
	frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
	frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;
	
	header = construct_mip_header(cache_entry->mip_address, *src_mip, ttl, 32, MIP_PING);
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
 * This is the method that determines the future of an incoming MIP packet
 * It takes care of incoming messages on the raw socket "line". 
 *
 * It perfroms checks on whether the incoming packet is addressed to the current MIP host
 * Or of it has to be forwarded to another host but in that case a TTL check in order. 
 * If the packet is destined for this host, then it is sent to a method that handles it. 
 * Otherwise, if the TTL check passes then the message will be forwarded.
 *
 * If the TTL check fails then the message should simply be discarded. 
*  cache: mip cache
*  queue: this is the message queue 
*  broadcast_queue: the queue that we hold for brodcasted messages (that are waiting for a reply)
*  ifs: datastrcuture to take care of the different socket connections
*  my_mip_addr: the address of the current MIP host - important to make checks and to address/asign src/dst
*/
int forwarding_engine(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, struct ifs_data *ifs, uint8_t *my_mip_addr)
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
	/*
	 * Main forwarding logic
	 * 1) Check if MIP_DST is meant for ME or if its a Broadcast
	 * 2) If (1) failed then 
	 *	Check if TTL is valid and then try to figure out next hop
	 * 3) If both of the above fail then 
	 *	- packet not meant for me
	 *	- TTL expired
	 *	We need to discard the packet
	 */
	if(header.dst_addr == *my_mip_addr || header.dst_addr == 0xFF){
		if(debug_flag)
		{
			printf("[<info>] MIP Destination == My MIP || 0xFF [<info>]\n");
			printf("[<info>] Need to handle arp packet... [<info>]\n");
		}
		handle_arp_packet(cache, queue, broadcast_queue, ifs , my_mip_addr, so_name, frame_hdr, header, buf);
	}else if(--header.ttl > 0) {
		if(debug_flag)
		{
			printf("[<info>] MIP Destionation != My MIP && != 0xFF [<info>]\n");
			printf("[<info>] Passed TTL check as well. So, we have to forward this message somewhere [<info>]\n");
			printf("[<info>] Need to get information from Routing Daemon. [<info>]\n");
			printf("[<info>] [SRC: %d] [DST: %d] [<info>]\n", header.src_addr, header.dst_addr);
		}
		addToQueue(queue, header.src_addr, header.dst_addr, buf, 255, header.ttl);
		lookup_request(ifs->routin_sock, header.src_addr, header.dst_addr);	
	}else {
		if(debug_flag)
		{
			printf("[<info>] MIP Destination != My MIP && TTL <= 0 [<info>]\n");
			printf("[<info>] Need to discard message due to failing TTL check. [<info>]\n");
			printf("[<info>] TTL: [%d] [<info>]\n", header.ttl);
		}
	}
	
	return 1;
}

/*
 * Takes care of incoming messages that are passed to it. This method is mostly used for handling
 * messages that are addressed to the current host or if the incoming message is a broadcast message. 
 * In the body of this method we check the types of the messages and to whom they are addressed to.
 *
 * The bulk of the work goes towards identifying on whether an incoming message is a MIP message and 
 * the performing the procedures necessary to for handling incoming ARP packets. 
 * If the incoming packet is a ROUTING packet and forwarding that to the Routing daemon. if necessary.
 * 
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
int handle_arp_packet(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, struct ifs_data *ifs, uint8_t *my_mip_addr, struct sockaddr_ll so_name, struct ether_frame frame_hdr, struct mip_header header, uint8_t *buf)
{
	uint8_t valz;
	int rc;

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
				
				//need to check if this response is for something we have in the queue.
				printf("[<info>] Checking if the broadcast response is one we have stored in our broadcast cache? [<info>]\n");
				struct QueueEntry *broadcast_entry = isInQueue(broadcast_queue, header.src_addr);
				if(broadcast_entry != NULL){
					if(debug_flag) printf("[<info>] Broadcast queue hit! We, made a broadcat request to this src [<info>]\n");
					int interface_index = find_interface_from_cache(ifs, &so_name);
					addToCache(cache, header.src_addr, frame_hdr.src_addr, interface_index);
					print_cache(cache);


					//We delete the entry from the broadcast queue
					deleteFromQueue(broadcast_queue, header.src_addr);
					//Now we forward the message because we know the address of the next_hop/destination

					struct QueueEntry *message_entry = popFromQueue(queue);
					if(message_entry != NULL){
						printf("[<info>] retrieved an entry from queue! [<info>]\n");
						printf("[<SRC: %d> <DST: %d> <TTL: %d> <Message: %s>]\n",message_entry->src_mip, message_entry->dst_mip, message_entry->ttl,  message_entry->message);

						printf("[<info>] Forwarding message to next hopp, which is [%d] [<info>]\n", header.src_addr);
						forward_packet_to_next_hop(ifs, &message_entry->src_mip, message_entry->dst_mip, message_entry->ttl, message_entry->message, 255, cache);
					}else{
						printf("[<info>] There was nothing in the message queue to forward...[<info>]");
					}

				}else{
					printf("[<info>] Never made a broadcast request for this MIP. Just ignoring it then [<info>]\n");
				}
			}else{
				//We send the message up to the PING/PONG client if we end up in this else..
				if(debug_flag) printf("[<info>] Received message. Type: *PING* [<info>]\n");
				if(debug_flag) printf("[<info>] Message: %s [<info>]\n", buf);

				struct information info;
				memcpy(info.message,buf,sizeof(info.message));
				info.destination_host = header.src_addr; //Who we will reply to later
		
				rc = write(ifs->unix_sock, &info, sizeof(struct information));
				if(rc <= 0)
				{
					close(ifs->unix_sock);
					printf("<%d> has lef thte chat...\n", ifs->unix_sock);
					return -1;
				}
				if(debug_flag) printf("[<info>] Forwarding message to Pong-Server Application. [<info>]\n");
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
	}else if(header.sdu_type == MIP_ROUTING){
		if(debug_flag) printf("[<info>] Received message. Type: *MIP-ROUTING* [<info>]\n");
		if(debug_flag) printf("------Daemon received ROUTING MSG from other Daemon. PROCESSING\n");
		if(debug_flag) printf("[<info>] Sending [HELLO] From MIP [%d] to Routing Daemon [<info>]\n\n", header.src_addr);
		
		send_message_to_routing_daemon(ifs, header.src_addr, *my_mip_addr, (char*)buf, sizeof(buf));
	}else{
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
 * Sets the appropriate src and dst fields. 
 * Main job here is to send a request looking for dst_mip.
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
	//create MIP header with TTL=0x01	
	header = construct_mip_header(0xFF, *src_mip, 0x01, sizeof(uint8_t), MIP_ARP);

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
int forward_packet_to_next_hop(struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, uint8_t ttl, char *buf, size_t buf_len, struct Cache *cache)
{
	struct ether_frame	frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[3];
	int rc;
	uint8_t *address = &dst_mip;
	struct sockaddr_ll socka;


	struct CacheEntry *cache_entry = isInCache(cache, dst_mip);
	if(cache_entry == NULL){
		if(debug_flag) printf("[<info>] Forward Packet: Something went wrong when getting the dst_mip form cache [<info>]\n");
		return -1;
	}
	 /* from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == cache_entry->index){
			memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
			//print_mac_addr(ifs->addr[i].sll_addr,6);
			socka = ifs->addr[i];
		}
	}
	memcpy(frame_hdr.dst_addr, cache_entry->mac_address, 6); //USE the cache interface
	frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
	frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;

	struct mip_header	header;
	//create MIP header with TTL=0x01	
	header = construct_mip_header(dst_mip, *src_mip, ttl, sizeof(uint8_t), MIP_ARP);

	/* frame header */
	msgvec[0].iov_base	= &frame_hdr;
	msgvec[0].iov_len	= sizeof(struct ether_frame);

	/* MIP header */
	msgvec[1].iov_base	= &header;
	msgvec[1].iov_len	= sizeof(struct mip_header);
	
	/* MIP SDU */
	msgvec[2].iov_base	= buf;
	msgvec[2].iov_len	= 255;

		if(debug_flag)
		{
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
