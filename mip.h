#ifndef MIP
#define MIP

#include <stdint.h>
#include <sys/types.h>

//MIP SDU TYPES
#define MIP_ARP 0x01
#define MIP_PING 0x02

#define MIP_ROUTING 0x04


#define MIP_TYPE_REQUEST	0x00
#define MIP_TYPE_RESPONSE	0x01

struct mip_header {
	uint8_t dst_addr;
	uint8_t src_addr;
	uint8_t	ttl : 4;
	uint16_t sdu_len : 9;
	uint8_t sdu_type : 3;
}__attribute__((packed));

//MIP SDU
struct mip_sdu {
	uint8_t type: 1;
	uint8_t mip_addr;
	uint8_t padding: 3;
}__attribute__((packed));

//PDU
struct mip_pdu {
	struct mip_header	*header;
	struct mip_sdu		*sdu; 
};


void print_mip_header(struct mip_header *h);
void print_mip_sdu(struct mip_sdu *s);
struct mip_header construct_mip_header(uint8_t dst_addr, uint8_t src_addr, uint8_t ttl, uint16_t sdu_len, uint8_t sdu_type);
struct mip_sdu construct_mip_sdu(uint8_t type, uint8_t mip_addr);
#endif
