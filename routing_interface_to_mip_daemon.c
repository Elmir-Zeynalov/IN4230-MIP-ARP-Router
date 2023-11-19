#include "routing_interface_to_mip_daemon.h"
#include "routing_table.h"
#include "routing_utils.h"
#include <string.h>
#include <unistd.h>

int handle_routing_message(struct packet_ux *pu, struct Table *routing_table)
{
    int rc;
    printf("[<info>] Starting to handle routing message [<info>]\n"); 
    if(strncmp(pu->msg, ROUTING_HELLO,3) == 0){
        printf("Received a [HELLO] message\n");
        addToTable(routing_table, pu->mip, pu->mip, 1);       
    }
    
    if(strncmp(pu->msg, ROUTING_UPDATE,3) == 0){
        printf("NEW ROUTING UPDATTE LFG!\n");
        printf("NEW ROUTING UPDATTE LFG!\n");
        printf("NEW ROUTING UPDATTE LFG!\n");

    }

    if(strncmp(pu->msg, ROUTING_REQUEST,3) == 0){
    
    }

    if(strncmp(pu->msg, ROUTING_RESPONSE,3) == 0){
    
    }
    printf("-\n");
    print_routing_table(routing_table);
    return rc;
}

void print_table_entry(struct TableEntry *entry) {
    printf("[%u \t %u \t %u]\n", entry->mip_address, entry->number_of_hops, entry->next_hop);
}


int advertise_my_routing_table(int unix_sock, struct Table *routing_table)
{
    int rc;
    struct packet_ux pu;
        
    memcpy(&pu.msg, ROUTING_UPDATE, 3);
    memcpy(&pu.msg[3], &routing_table->num_entries, sizeof(int));
    memcpy(pu.msg + 3 + sizeof(int), routing_table->entries, routing_table->num_entries * sizeof(struct TableEntry));

    /*
    // Copy Table entries into the msg field after the number of entries
    memcpy(pu.msg + 3 + sizeof(int), routing_table->entries, routing_table->num_entries * sizeof(struct TableEntry));

    // Copy Table entries into the msg field
    memcpy(pu.msg + 1, routing_table->entries, routing_table->num_entries * sizeof(struct TableEntry));
    
    
        // Print the contents of the msg field for debugging
    printf("Message Contents:\n");

    // Print ROUTING_UPDATE
    printf("First 3 bytes: ");
    for (size_t i = 0; i < 3; ++i) {
        printf("%c ", pu.msg[i]);
    }
    printf("\n");

    // Print num_entries
    printf("Num Entries: %u\n", pu.msg[3]);

    // Print Table entries
    printf("Table Entries:\n");
    for (int i = 0; i < routing_table->num_entries; ++i) {
        printf("Entry %d: ", i);
        print_table_entry(&routing_table->entries[i]);
    }
    
    */
    rc = write(unix_sock, &pu, sizeof(struct packet_ux));

    // Check for write errors
    if (rc == -1) {
        // Handle error
        perror("write");
        return -1;
    }

    return 1;
}
