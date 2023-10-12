#ifndef _CACHE_H
#define _CACHE_H

#include "imports.h"
#define CACHE_SIZE 4

// Define the structure for a cache entry
struct CacheEntry {
    uint8_t mip_address;
    uint8_t mac_address[6];
    //struct sockaddr_ll interface;
    int index;
};

struct Cache {
    struct CacheEntry entries[CACHE_SIZE];
    int num_entries;
};

void initializeCache(struct Cache* cache);
void addToCache(struct Cache* cache, uint8_t mip, uint8_t mac[6], int index);
struct CacheEntry* isInCache(struct Cache* cache, uint8_t mip);
void deleteFromCache(struct Cache* cache, uint8_t mip);

#endif
