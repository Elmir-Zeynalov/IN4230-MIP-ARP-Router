#include "routing_interface_to_mip_daemon.h"
#include "routing_table.h"
#include "routing_utils.h"
#include <string.h>

int handle_routing_message(struct packet_ux *pu, struct Table *routing_table)
{
    int rc;
    printf("[<info>] Starting to handle routing message [<info>]\n"); 
    if(strncmp(pu->msg, ROUTING_HELLO,3) == 0){
        printf("Received a [HELLO] message\n");
        addToTable(routing_table, pu->mip, pu->mip, 1);       
    }
    
    if(strncmp(pu->msg, ROUTING_UPDATE,3) == 0){
    
    }

    if(strncmp(pu->msg, ROUTING_REQUEST,3) == 0){
    
    }

    if(strncmp(pu->msg, ROUTING_RESPONSE,3) == 0){
    
    }
    printf("-\n");
    print_routing_table(routing_table);
    return rc;
}
