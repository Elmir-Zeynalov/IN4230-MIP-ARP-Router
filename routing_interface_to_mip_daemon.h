#ifndef _ROUTING_INTERFACE_TO_MIP_H
#define _ROUTING_INTERFACE_TO_MIP_H

#include "routing_utils.h"
#include "routing_table.h"
int handle_routing_message(struct packet_ux *pu, struct Table *routing_table);
int advertise_my_routing_table(int unix_sock, struct Table *routing_table);


#endif
