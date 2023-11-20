#include "queue.h"
#include <stdint.h>

// Initialize the queue
void initializeQueue(struct Queue* queue) {
    queue->num_entries = 0;
}

// Add an entry to the queue
void addToQueue(struct Queue* queue, uint8_t src_mip,  uint8_t dst_mip, char *buf, size_t len, uint8_t ttl) {
    if (queue->num_entries < QUEUE_SIZE) {
        struct QueueEntry* newEntry = &(queue->entries[queue->num_entries]);
        newEntry->src_mip = src_mip;
        newEntry->dst_mip = dst_mip;

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
struct QueueEntry* isInQueue(struct Queue* queue, uint8_t dst_mip) {
    for (int i = 0; i < queue->num_entries; i++) {
        if (queue->entries[i].dst_mip == dst_mip) {
            return &(queue->entries[i]);
        }
    }
    return NULL; // Entry not found
}

// Delete an entry from the queue
void deleteFromQueue(struct Queue* queue, uint8_t dst_mip) {
    for (int i = 0; i < queue->num_entries; i++) {
        if (queue->entries[i].dst_mip == dst_mip) {
            // Replace the entry with the last entry in the queue
            queue->entries[i] = queue->entries[queue->num_entries - 1];
            queue->num_entries--;
            return;
        }
    }
}


struct QueueEntry* popFromQueue(struct Queue* queue) {
    if (queue->num_entries > 0) {
        // Retrieve the pointer to the first entry in the queue
        struct QueueEntry* firstEntry = &(queue->entries[0]);

        // Shift the remaining entries to fill the gap
        for (int i = 0; i < queue->num_entries - 1; i++) {
            queue->entries[i] = queue->entries[i + 1];
        }

        // Decrement the number of entries
        queue->num_entries--;

        return firstEntry;
    } else {
        printf("Queue is empty. Cannot pop an entry.\n");
        return NULL; // Return NULL or handle as appropriate
    }
}
