#ifndef _ROUTING_TABLE_H
#define _ROUTING_TABLE_H

#include "imports.h"

#define TABLE_SIZE 10 

// Define the structure for a cache entry
struct TableEntry {
    uint8_t mip_address;
    uint8_t number_of_hops;
    uint8_t next_hop;
};

struct Table {
    struct TableEntry entries[TABLE_SIZE];
    int num_entries;
};

void initializeTable(struct Table* table);
void addToTable(struct Table* table, uint8_t mip_address, uint8_t next_hop, uint8_t number_of_hops);
struct TableEntry* isInTable(struct Table* table, uint8_t mip);
void deleteFromTable(struct Table* table, uint8_t mip);
void print_routing_table(struct Table *table);
#endif
