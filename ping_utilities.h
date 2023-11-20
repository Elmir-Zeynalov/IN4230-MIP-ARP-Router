#ifndef _PING_H
#define _PING_H

#include <stdint.h>
#include <sys/types.h>

struct information {
    uint8_t destination_host;
    char message[256];
    uint8_t ttl;
};

#endif
