#include "routing_interface_to_mip_daemon.h"
#include "routing_table.h"
#include "routing_utils.h"
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/*
 * Method that handles the interpretation of an incoming message from a MIP daemon.
 * the message is stored in a structure packet_ux where most of the valuable information
 * is stored in the msg field. the structure is different depending on the message. thats why
 * we perform different checks to determine if its a HELLO, Routing Update, or Request.
 *
 * unick_sock: socket connection with mip daemon
 * packet_ux: the structure that is used to pass messages between MIP daemon and Routing daemon. The most useful information
 *  is stored in the msg filed. The message filed has a specific structure depending on the type of message. 
 *  F.ex 
 *      In the HELLO message - we check the first 3 bytes
 *      In the REQUEST message- we start at msg+2 and then check the 3 bytes there
 *      In UPDATE, we check the first 3 bytes to determine if its an update and then we take the contents and update our own routing table.
 */
int handle_routing_message(int unix_sock, struct packet_ux *pu, struct Table *routing_table)
{
    int rc;
    printf("[<info>] Starting to handle routing message [<info>]\n"); 
    if(strncmp(pu->msg, ROUTING_HELLO,3) == 0){
        printf("Received a [HELLO] message\n");
        addToTable(routing_table, pu->mip, pu->mip, 1);       
    }
    //CHECK if the MIP damon has sent us a routing table of a neighbor.
    if(strncmp(pu->msg, ROUTING_UPDATE,3) == 0){
        int number_of_entries;
        memcpy(&number_of_entries, pu->msg + 3, sizeof(int));
        int i;
        //go through the entires and update our routing table
        for(i = 0 ; i < number_of_entries; i++)
        {
            struct TableEntry table_entry;
            memcpy(&table_entry, pu->msg + (3 + sizeof(int) + (i * sizeof(struct TableEntry))), sizeof(struct TableEntry));

            update_routing_table(routing_table, pu->my_mip, pu->mip, &table_entry);

            printf("----[%d\t%d\t%d]------\n",table_entry.mip_address, table_entry.next_hop, table_entry.number_of_hops);
        }
    }
    //Check if the the message recevied is a REQUEST from the MIP DAEMON
    if(strncmp(pu->msg + 2, ROUTING_REQUEST,3) == 0){
        struct packet_ux response;
        uint8_t ttl = 0; 
        uint8_t looking_for_mip;
        memcpy(&looking_for_mip, pu->msg+5, sizeof(uint8_t));

        printf("Checking if i have [%d] in my routing table\n", looking_for_mip);

        memcpy(response.msg, pu->msg, sizeof(uint8_t));
        memcpy(response.msg + 1, &ttl, sizeof(uint8_t));
        memcpy(response.msg + 2, ROUTING_RESPONSE, 3);
        struct TableEntry *entry = isInTable(routing_table, looking_for_mip);
        if(entry == NULL){
            printf("NO entries in table for [%d]\n", looking_for_mip);
            uint8_t no_hits = 255;
            memcpy(response.msg + 5, &no_hits, sizeof(uint8_t));
            printf("NEXT HOP [NO HOP]: %d \n", no_hits);
        }else{
            memcpy(response.msg + 5, &entry->next_hop, sizeof(uint8_t));
        }
        
        rc = write(unix_sock, &response, sizeof(struct packet_ux));
    }

    if(strncmp(pu->msg, ROUTING_RESPONSE,3) == 0){
    
    }
    printf("-\n");
    print_routing_table(routing_table);
    return rc;
}

/*
 * prints a table entry that it is given
 */
void print_table_entry(struct TableEntry *entry) {
    printf("[%u \t %u \t %u]\n", entry->mip_address, entry->number_of_hops, entry->next_hop);
}

/*
 * Method used to send the routing table from the routing daemon to the MIP daemon.
 * The entire routing table is placed in the msg filed of the packet_ux structure.
 * The structure is basic:
 * first 3 bytes are "UPD"
 * then an integer to specify the number of entires
 * finally, the actualy entires one by bye
 *
 * Structure is:
 * [U][P][D][number_of_entries][Entry][Entry][...]
 *
 * unix_sock: connection with mip daemon
 * routing_table: the routing table that will be passed down
 *
 */
int advertise_my_routing_table(int unix_sock, struct Table *routing_table)
{
    int rc;
    struct packet_ux pu;
        
    memcpy(&pu.msg, ROUTING_UPDATE, 3);
    memcpy(&pu.msg[3], &routing_table->num_entries, sizeof(int));
    memcpy(pu.msg + 3 + sizeof(int), routing_table->entries, routing_table->num_entries * sizeof(struct TableEntry));

    rc = write(unix_sock, &pu, sizeof(struct packet_ux));

    // Check for write errors
    if (rc == -1) {
        // Handle error
        perror("write");
        return -1;
    }

    return 1;
}
