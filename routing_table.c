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
