#include "queue.h"
#include <stdint.h>

// Initialize the queue
void initializeQueue(struct Queue* queue) {
    queue->num_entries = 0;
}

// Add an entry to the queue
void addToQueue(struct Queue* queue, uint8_t mip, char *buf, size_t len, uint8_t ttl) {
    if (queue->num_entries < QUEUE_SIZE) {
        struct QueueEntry* newEntry = &(queue->entries[queue->num_entries]);
        newEntry->mip_address = mip;
        strncpy(newEntry->message, buf, len);
        newEntry->message[len] = '\0'; // Ensure null-termination
        newEntry->len = len;
        newEntry->ttl = ttl;
        queue->num_entries++;
    } else {
        printf("Queue is full. Cannot add more entries.\n");
    }
}

// Check if an entry with the given MIP address is in the queue
struct QueueEntry* isInQueue(struct Queue* queue, uint8_t mip) {
    for (int i = 0; i < queue->num_entries; i++) {
        if (queue->entries[i].mip_address == mip) {
            return &(queue->entries[i]);
        }
    }
    return NULL; // Entry not found
}

// Delete an entry from the queue
void deleteFromQueue(struct Queue* queue, uint8_t mip) {
    for (int i = 0; i < queue->num_entries; i++) {
        if (queue->entries[i].mip_address == mip) {
            // Replace the entry with the last entry in the queue
            queue->entries[i] = queue->entries[queue->num_entries - 1];
            queue->num_entries--;
            return;
        }
    }
}
