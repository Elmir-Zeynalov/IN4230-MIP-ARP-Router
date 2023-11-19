#include "routing_table.h"
#include <stdint.h>

// Initialize the Table
void initializeTable(struct Table* routingTable) {
    routingTable->num_entries = 0;
}

// Add an entry to the Table
void addToTable(struct Table* routingTable, uint8_t mip, uint8_t next_hop, uint8_t number_of_hops) {
    if (routingTable->num_entries < TABLE_SIZE) {

        struct TableEntry *previousEntry = isInTable(routingTable, mip);

        if(previousEntry != NULL)
        {
            //So, an entry already exists. We only update the entry if the incoming hop count is less
            if(previousEntry->number_of_hops > number_of_hops)
            {
                printf("[<info>] Updating previous entry in RT with a more efficient route [<info>]\n");
                previousEntry->number_of_hops = number_of_hops;
                previousEntry->next_hop = next_hop;
            }else{
                printf("[<info>] Updating previous entry in RT with a more efficient route [<info>]\n");
            }

        }else{

            struct TableEntry* newEntry = &(routingTable->entries[routingTable->num_entries]);
            newEntry->mip_address = mip;
            newEntry->next_hop = next_hop;
            newEntry->number_of_hops = number_of_hops;
            routingTable->num_entries++;
        }
    } else {
        printf("Table is full. Cannot add more entries.\n");
    }
}

// Check if an entry with the given MIP address is in the Table
struct TableEntry* isInTable(struct Table* Table, uint8_t mip) {
    for (int i = 0; i < Table->num_entries; i++) {
        if (Table->entries[i].mip_address == mip) {
            return &(Table->entries[i]);
        }
    }
    return NULL; // Entry not found
}

// Delete an entry from the Table
void deleteFromTable(struct Table* Table, uint8_t mip) {
    for (int i = 0; i < Table->num_entries; i++) {
        if (Table->entries[i].mip_address == mip) {
            // Replace the entry with the last entry in the Table
            Table->entries[i] = Table->entries[Table->num_entries - 1];
            Table->num_entries--;
            return;
        }
    }
}

void update_routing_table(struct Table *routing_table, uint8_t my_mip, uint8_t mip_address, struct TableEntry *table_entry){

    if(table_entry->mip_address == my_mip){
        printf("Incoming entry is ME. So, we ignore it...\n");
        return;
    }
    //check is it me?
    //check if i have it in my table already?
    //check if the hop_count i have is more efficient or not
    //update if needed. Else ignore
    struct TableEntry *local_entry = isInTable(routing_table, table_entry->mip_address);
    if(local_entry != NULL){
        if(local_entry->number_of_hops > table_entry->number_of_hops + 1){
            local_entry->next_hop = mip_address;
            local_entry->number_of_hops = table_entry->number_of_hops+1;
        }else{
            printf("We tried to update, BUT the entry we already is more efficient. IGNORE.\n");
            printf("\tLocal:\t%d|%d|%d\n", local_entry->mip_address,local_entry->next_hop, local_entry->number_of_hops);
            printf("\tIncoming:\t %d|%d|%d\n", table_entry->mip_address, table_entry->next_hop, table_entry->number_of_hops);
        }
    }
    else {
        printf("We tried to update, BUT the entry we already is more efficient. IGNORE.\n");
        printf("\tIncoming:\t %d|%d|%d\n", table_entry->mip_address, table_entry->next_hop, table_entry->number_of_hops);
        addToTable(routing_table, table_entry->mip_address,mip_address, table_entry->number_of_hops + 1);
    }


    printf("TABLE UPDATED\n");

}


void print_routing_table(struct Table *routing_table)
{
        printf("[*Printing Routing Table*]\n");
        int i;

        printf("**************************\n");
        if(routing_table->num_entries > 0)
        {
            printf("*MIP\tNext-hop\tHop Count\n");
            for(i = 0; i < routing_table->num_entries; i++)
            {
                printf("* %d\t|", routing_table->entries[i].mip_address);
                printf("%d\t|", routing_table->entries[i].next_hop);
                printf("%d *\n", routing_table->entries[i].number_of_hops);
            }
        }else{
            printf("*Routing Table Empty*\n");
        }
        printf("**************************\n");
}
