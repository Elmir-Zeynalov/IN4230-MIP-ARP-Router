#ifndef _QUEUE_H
#define _QUEUE_H

#include "imports.h"

#define QUEUE_SIZE 4

// Define the structure for a cache entry
struct QueueEntry {
    uint8_t src_mip;
    uint8_t dst_mip;
    char message[255];
    size_t len;
    uint8_t ttl;
};

struct Queue {
    struct QueueEntry entries[QUEUE_SIZE];
    int num_entries;
};

void initializeQueue(struct Queue* queue);
void addToQueue(struct Queue* queue, uint8_t src_mip, uint8_t dst_mip, char *buf, size_t len, uint8_t ttl);
struct QueueEntry* isInQueue(struct Queue* queue, uint8_t mip);
void deleteFromQueue(struct Queue* queue, uint8_t mip);

struct QueueEntry* popFromQueue(struct Queue* queue);
#endif
