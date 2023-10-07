#ifndef _ETHERNET_H
#define _ETHERNET_H

#include <stdint.h>             /* uint8_t */
#include <unistd.h>             /* size_t */

struct ether_frame {
        uint8_t dst_addr[6];
        uint8_t src_addr[6];
        uint8_t eth_proto[2];
} __attribute__((packed));


#endif
