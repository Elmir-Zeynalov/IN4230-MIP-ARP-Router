#include "routing_utils.h"

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
    memcpy(&pu.msg, "REQ",3); 
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
    memcpy(&pu.msg, "RSP",3); 
    memcpy(&pu.msg[3], &requested_mip, 1);
    
    write(unix_sock, &pu, 8);
}

