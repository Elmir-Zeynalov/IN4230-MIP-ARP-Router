#include "routing_interface_to_mip_daemon.h"
#include "routing_table.h"
#include "routing_utils.h"
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int handle_routing_message(int unix_sock, struct packet_ux *pu, struct Table *routing_table)
{
    int rc;
    printf("[<info>] Starting to handle routing message [<info>]\n"); 
    if(strncmp(pu->msg, ROUTING_HELLO,3) == 0){
        printf("Received a [HELLO] message\n");
        addToTable(routing_table, pu->mip, pu->mip, 1);       
    }
    
    if(strncmp(pu->msg, ROUTING_UPDATE,3) == 0){
        printf("\tNEW ROUTING UPDATTE LFG!\n");
        printf("\tNEW ROUTING UPDATTE LFG!\n");
        printf("\tNEW ROUTING UPDATTE LFG!\n");
        printf("\tFROM: %d\n",pu->mip);
        
        //process_incoming_table_update(pu,routing_table);
        int number_of_entries;
        memcpy(&number_of_entries, pu->msg + 3, sizeof(int));
        printf("\tNumber of entries: %d\n", number_of_entries);
        
        int i;
        for(i = 0 ; i < number_of_entries; i++)
        {
            struct TableEntry table_entry;
            memcpy(&table_entry, pu->msg + (3 + sizeof(int) + (i * sizeof(struct TableEntry))), sizeof(struct TableEntry));

            update_routing_table(routing_table, pu->my_mip, pu->mip, &table_entry);

            printf("----[%d\t%d\t%d]------\n",table_entry.mip_address, table_entry.next_hop, table_entry.number_of_hops);
        }
    }

    if(strncmp(pu->msg + 2, ROUTING_REQUEST,3) == 0){
        struct packet_ux response;
        printf("IT WORKED LMAOOOOOO\n");

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
        
        printf("////////ROUTTT//////////////\n");
        printf("%d\n", response.msg[0]);
        printf("%d\n", response.msg[1]);
    
        printf("%c\n", response.msg[2]);
        printf("%c\n", response.msg[3]);
        printf("%c\n", response.msg[4]);
    
        printf("%d\n", response.msg[5]);
        printf("//////////////////////\n\n");

        rc = write(unix_sock, &response, sizeof(struct packet_ux));
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
