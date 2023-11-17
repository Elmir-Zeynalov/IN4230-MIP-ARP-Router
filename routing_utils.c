#include "routing_utils.h"
#include "daemon_routing_utils.h"
#include <string.h>



void send_routing_hello(int unix_sock)
{
    struct packet_ux pu;
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
    struct packet_ux pu;
    pu.mip = host_mip;
    pu.ttl = 0;
    memcpy(&pu.msg,ROUTING_REQUEST,3); 
    memcpy(&pu.msg[3], &requested_mip, 1);
    
    write(unix_sock, &pu, 8);
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
    struct packet_ux pu;
    pu.mip = host_mip;
    pu.ttl = 0;
    memcpy(&pu.msg, ROUTING_RESPONSE,3); 
    memcpy(&pu.msg[3], &requested_mip, 1);
    
    write(unix_sock, &pu, 8);
}



void handle_message_from_routing_daemon(struct ifs_data *ifs, uint8_t *src_mip, struct Cache *cache)
{
    int rc;
    struct packet_ux pu;

    rc = read(ifs->routin_sock,&pu, sizeof(struct packet_ux));
    
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
    }
    if(strcmp(type, ROUTING_REQUEST) == 0)
    {
        printf("[<info>] Handling [REQUEST] from routing daemon [<info>]\n");
    }
    if(strcmp(type, ROUTING_RESPONSE) == 0)
    {
        printf("[<info>] Handling [RESPONSE] from routing daemon [<info>]\n");
    }



}



