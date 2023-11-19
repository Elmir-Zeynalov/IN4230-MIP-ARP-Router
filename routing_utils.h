#ifndef _ROUTING_H
#define _ROUTING_H
#include "imports.h"
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "daemon_routing_utils.h"


#define HELLO_INTERVAL 6000//3000
#define HELLO_TIMEOUT  5000
#define MAX_EVENTS 5
#define MAX_CONNS 5
#define TABLE_UPDATE_TIMEOUT 4000

enum state {
    INIT = 0,
    WAIT,
    ADVERTISE_ROUTING_TABLE,
    EXIT
};

#define ROUTING_HELLO	    "HEI"
#define ROUTING_UPDATE	    "UPD"
#define ROUTING_REQUEST	    "REQ"
#define ROUTING_RESPONSE    "RSP"

struct packet_ux{
    uint8_t mip : 8;
    uint8_t ttl ;
    uint8_t my_mip;
    char msg[255];
}__attribute__((packed));

void send_routing_hello(int unix_sock, uint8_t mip_sender);
void lookup_request(int unix_sock, uint8_t host_mip, uint8_t requested_mip);
void lookup_response(int unix_sock, uint8_t host_mip, uint8_t requested_mip);
void handle_message_from_routing_daemon(struct ifs_data *ifs, uint8_t *src_mip , struct Cache *cache);
int send_message_to_routing_daemon(struct ifs_data *ifs, uint8_t from_mip, char *buff, size_t len);

#endif
