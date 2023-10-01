#ifndef _COMMON_H
#define _COMMON_H

#include <cstdint>
#include <stdint.h>		/* uint8_t */
#include <unistd.h>		/* size_t */
#include <linux/if_packet.h>	/* struct sockaddr_ll */

#define MAX_EVENTS	10
#define MAX_IF		3
#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}


struct ether_frame {
	uint8_t	dst_addr[6];
	uint8_t	src_addr[6];
	uint8_t eth_proto[2];
} __attribute__((packed));

struct ifs_data {
	struct sockaddr_ll addr[MAX_IF];
	int rsock;
	int ifn;
};

#endif // !DEBUG
