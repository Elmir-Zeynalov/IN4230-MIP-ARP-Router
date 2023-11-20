#include "routing_utils.h"
#include "cache.h"
#include "common.h"
#include "daemon_routing_utils.h"
#include <stdint.h>
#include <string.h>
#include "queue.h"
#include "routing_table.h"

void send_routing_hello(int unix_sock, uint8_t mip_sender)
{
    struct packet_ux pu;
    pu.mip = mip_sender;
    memcpy(&pu.msg, ROUTING_HELLO, 3);
    write(unix_sock, &pu, sizeof(struct packet_ux));
}


/*
* Method that the MIP daemon calls when it wants to perform a request to the routing daemon
* We build the request according to the spec and include the host_mip (my mip address),
* ttl and requested_mip (the mip we are looking for). 
* We package it and send a message on the unix_sock line (passed to this fuction). 
*
* Unix_sock line should be the unix connected we have with the routing daemon. 
* We send 8 bytes of data with all the information we need. 
*
*/
void lookup_request(int unix_sock, uint8_t host_mip, uint8_t requested_mip)
{
    //<MIP address of the host itself (8 bits)> <zero (0) TTL (8 bits)> <R (0x52, 8 bits)> <E (0x45, 8 bits)> <Q (0x51, 8 bits)> <MIP address to look up (8 bits)>

    struct packet_ux pu;
    pu.my_mip = host_mip;
    pu.mip = requested_mip;
    pu.ttl = 0;

    char *req = "REQ";
    uint8_t ttl = 0; 

    memcpy(pu.msg, &host_mip, sizeof(uint8_t));
    memcpy(pu.msg + 1, &ttl, sizeof(uint8_t));
    memcpy(pu.msg + 2, req, 3);
    memcpy(pu.msg + 5, &requested_mip, sizeof(uint8_t));
    
    write(unix_sock, &pu, sizeof(struct packet_ux));
}

/*
* This is a response to a routing request. This message is built
* and sent from the routing daemon to the mip daemon of the requester.
* Similar to the lookup request, we build the message with
* the host_mip and requested_mip(dst_mip), and reply with
* the next hop and place that in the msg part.
*/
void lookup_response(int unix_sock, uint8_t host_mip, uint8_t requested_mip)
{
    struct packet_ux *pu;
    char buff[255];
    pu = (struct packet_ux*)buff;
    memset(pu->msg, 0, 255);

    pu->mip = host_mip;
    pu->ttl = 0;
    memcpy(&pu->msg,ROUTING_RESPONSE,3); 
    memcpy(&pu->msg[3], &requested_mip, 1);
    
    write(unix_sock, &pu, 8);
}


/*
 * Method used by the MIP daemon when receiving a message from the Routing daemon
 * It here where we decide, bsed on the msg type in packet_ux what we should do 
 * This is where we know if the routing daemon wants us to advertise HELLO, its ROUTING tabke or
 * when it replies to our routing requests. 
 * This is where we get the potential next-hop from the routing daemon. 
 *
 * F.ex 
 * - if the mip daemon should advertise a routing table
 * - broadcast a hello message to its neighbours (hello passed from the routing daemon)
 * - receving a response to a routing request! -
 *
 */
void handle_message_from_routing_daemon(struct ifs_data *ifs, uint8_t *src_mip, struct Queue *broadcast_queue, struct Cache *cache)
{
    int rc;
    struct packet_ux pu;

    rc = read(ifs->routin_sock,&pu, sizeof(struct packet_ux));
    if(rc <= 0)
    {   
			close(ifs->routin_sock);
			printf("<%d> routing daemone disconnected...\n", ifs->routin_sock);
			ifs->routin_sock = -1;
			return;
    } 
    char type[4];
    memcpy(type, pu.msg, 3);
    type[3] = '\0';


    printf("[<info>] Message from routing daemon [%s] [<info>]\n", type);
    if(strcmp(type, ROUTING_HELLO) == 0)
    {
        printf("[<info>] Handling [HELLO] from routing daemon [<info>]\n");
        disseminate_hello_message(ifs, src_mip, ROUTING_HELLO, strlen(ROUTING_HELLO), cache);
    }
    if(strcmp(type, ROUTING_UPDATE) == 0)
    {
        printf("[<info>] Handling [UPDATE] from routing daemon [<info>]\n");
        int number_of_routing_table_entries;
        memcpy(&number_of_routing_table_entries, &pu.msg[3] , sizeof(int));
        printf("Number of Entries in Incoming Routing Table [%d]\n", number_of_routing_table_entries);

        struct TableEntry table_entry;
        memcpy(&table_entry, pu.msg + 3 + sizeof(int), sizeof(struct TableEntry));
        printf("----[%d\t%d\t%d]------\n",table_entry.mip_address, table_entry.next_hop, table_entry.number_of_hops);

        disseminate_update_message(ifs, src_mip, pu.msg, 255);
    }
    if(strncmp(pu.msg + 2, ROUTING_RESPONSE,3) == 0)
    {

        uint8_t next_hop = (unsigned char)pu.msg[5];
        printf("[<info>] Handling [RESPONSE] from routing daemon [<info>]\n");
        printf("<MIP: %d> <TTL: %d> <%c> <%c> <%c> <NEXT HOP: %u>\n", pu.msg[0], pu.msg[1], pu.msg[2], pu.msg[3], pu.msg[4], next_hop);
        
        if(next_hop == 255){
            printf("[<info>] Router didnt have a next-hop address for the destination [<info>]\n");
        }
        //We check if the next hop is in our cache
        struct CacheEntry *cache_entry =  isInCache(cache, next_hop);
        if(cache_entry == NULL) {
            char *broadcast_message = "BROADCAST";
            //We need to broadcast a message
            printf("[<info> Adding [%d] to broadcast queue <info>]\n", next_hop);
            addToQueue(broadcast_queue, next_hop, next_hop, broadcast_message, strlen(broadcast_message), 0);
            printf("[<info>] Sending BROADCAST [<info>]\n");
            
            struct QueueEntry *check = isInQueue(broadcast_queue, next_hop);
            send_arp_request(ifs, src_mip, next_hop, cache);
        }else{
            //We have the next_hop in our cache so we can directly send to it
           printf("[<info>] We have the next-hop in our cache already!! [<info>]\n"); 

        }
    }
}

/*
* Used to pass a message to the routing daemon. 
* given the parameters, it builds a packet_ux message and sends it to the routing daemon
* 
* from_mip: src mip address
* my_mip_addr: the mip address of the requester
* buff: message
* len: length
*
*/
int send_message_to_routing_daemon(struct ifs_data *ifs, uint8_t from_mip, uint8_t my_mip_addr, char *buff, size_t len)
{
    int rc;

    struct packet_ux pu;
    pu.mip = from_mip;
    pu.my_mip = my_mip_addr;
    memcpy(pu.msg, buff, sizeof(pu.msg));
    
    int entries;
    memcpy(&entries, pu.msg +3, sizeof(int));

    struct TableEntry t;
    memcpy(&t,  (pu.msg + 3) + sizeof(int), sizeof(struct TableEntry));

    rc = write(ifs->routin_sock, &pu, sizeof(struct packet_ux));
    if(rc <= 0) 
    {
        perror("write to routing daemon");
        return rc;
    }
    return rc;
}


