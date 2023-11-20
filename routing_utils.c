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
    
    printf("//////////////////////\n");
    printf("%d\n", pu.msg[0]);
    printf("%d\n", pu.msg[1]);
    
    printf("%c\n", pu.msg[2]);
    printf("%c\n", pu.msg[3]);
    printf("%c\n", pu.msg[4]);
    
    printf("%d\n", pu.msg[5]);
    printf("//////////////////////\n\n");
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
    printf("%s\n", pu.msg);
    printf("TYPE: %s\n", type);

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
        printf("XXXXXXXXX\n");

        disseminate_update_message(ifs, src_mip, pu.msg, 255);
    }
    if(strncmp(pu.msg + 2, ROUTING_RESPONSE,3) == 0)
    {

        uint8_t next_hop = (unsigned char)pu.msg[5];
        printf("[<info>] Handling [RESPONSE] from routing daemon [<info>]\n");
        printf("[info] Received RSP from Routing Daemon [info]\n");
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
            printf("QUICKCHEK: [%d]\n", check->src_mip);

            send_arp_request(ifs, src_mip, next_hop, cache);
            
        }else{
            //We have the next_hop in our cache so we can directly send to it
           printf("[<info>] We have the next-hop in our cache already!! [<info>]\n"); 

        }
    }
        
        //need to package and pass

    



}

int send_message_to_routing_daemon(struct ifs_data *ifs, uint8_t from_mip, uint8_t my_mip_addr, char *buff, size_t len)
{
    int rc;

    struct packet_ux pu;
    pu.mip = from_mip;
    pu.my_mip = my_mip_addr;
    //memcpy(&pu.msg, buff, sizeof(pu.msg));
    memcpy(pu.msg, buff, sizeof(pu.msg));
    printf("{\n");
    //printf("[<info>] Forwarding message [%s] FROM MIP [%d] to Routing daemon [<info>]\n", buff, pu.mip);

    printf("%c\n", buff[0]);
    printf("%c\n", buff[1]);
    printf("%c\n", buff[2]);
    
    int entries;
    memcpy(&entries, pu.msg +3, sizeof(int));

    printf("Entries: %d\n", entries);
    struct TableEntry t;

    memcpy(&t,  (pu.msg + 3) + sizeof(int), sizeof(struct TableEntry));

    printf("T-Entry: %d|%d|%d\n", t.mip_address, t.next_hop, t.number_of_hops);

    printf("}\n\n");
    rc = write(ifs->routin_sock, &pu, sizeof(struct packet_ux));
    if(rc <= 0) 
    {
        perror("write to routing daemon");
        return rc;
    }
    return rc;
}


