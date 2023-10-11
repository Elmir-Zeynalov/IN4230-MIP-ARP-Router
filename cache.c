#include "cache.h"

// Initialize the cache
void initializeCache(struct Cache *cache) {
    cache->num_entries = 0;
}

// Add an entry to the cache
void addToCache(struct Cache *cache, uint8_t mip, uint8_t mac[6], int index) { // struct sockaddr_ll intf) {
    if (cache->num_entries < CACHE_SIZE) {
        struct CacheEntry* newEntry = &(cache->entries[cache->num_entries]);
        newEntry->mip_address = mip;
        memcpy(newEntry->mac_address, mac, 6);
        //newEntry->interface = intf;
        newEntry->index = index;
        cache->num_entries++;
    } else {
        printf("Cache is full. Cannot add more entries.\n");
    }
}

// Check if an entry with the given MIP address is in the cache
struct CacheEntry* isInCache(struct Cache *cache, uint8_t mip) {
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].mip_address == mip) {
            return &(cache->entries[i]);
        }
    }
    return NULL; // Entry not found
}

// Delete an entry from the cache
void deleteFromCache(struct Cache *cache, uint8_t mip) {
    for (int i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].mip_address == mip) {
            // Replace the entry with the last entry in the cache
            cache->entries[i] = cache->entries[cache->num_entries - 1];
            cache->num_entries--;
            return;
        }
    }
}
