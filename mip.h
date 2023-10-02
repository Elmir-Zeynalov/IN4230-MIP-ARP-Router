#ifndef MIP
#define MIP

#include <stdint.h>
#include <sys/types.h>

struct mip_frame {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint8_t	ttl : 4;
	uint16_t sdu_len : 9;
	uint8_t sdu_type : 3;
}__attribute__((packed));


void print_mip_frame(struct mip_frame *m);

#endif
