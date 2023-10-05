#ifndef MIP
#define MIP

#include <stdint.h>
#include <sys/types.h>

//MIP SDU TYPES
#define MIP_ARP 0x01
#define MIP_PING 0x02

struct mip_frame {
	uint8_t dst_addr;
	uint8_t src_addr;
	uint8_t	ttl : 4;
	uint16_t sdu_len : 9;
	uint8_t sdu_type : 3;
}__attribute__((packed));


void print_mip_frame(struct mip_frame *m);

#endif
