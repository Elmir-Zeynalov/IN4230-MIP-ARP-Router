#include <stdio.h>
#include "mip.h"

void print_mip_header(struct mip_header *h)
{
	printf("**********\t\tMIP\t\t**********\n");
	printf("[----------\t\tHEADER\t\t----------]\n");
	printf("dst_MIP_addr: %d\n", h->dst_addr );
	printf("src_MIP_addr: %d\n",h->src_addr);
	printf("ttl: %d\n", h->ttl);
	printf("sdu_len: %d\n", h->sdu_len);
	printf("sdu_type: %d\n", h->sdu_type);
	printf("*****************************************\n");
}

void print_mip_sdu(struct mip_sdu *s)
{
	printf("**********\t\tMIP\t\t**********\n");
	printf("[----------\t\tSDU\t\t----------]\n");
	printf("MIP_TYPE: %d\n", s->type );
	printf("MIP_ADDR: %d\n",s->mip_addr);
	printf("*****************************************\n");
}

struct mip_header construct_mip_header(uint8_t dst_addr, uint8_t src_addr, uint8_t ttl, uint16_t sdu_len, uint8_t sdu_type)
{
	struct mip_header header;
	header.src_addr = src_addr;
	header.dst_addr = dst_addr;
	header.ttl	= ttl;
	header.sdu_len	= sdu_len;
	header.sdu_type	= sdu_type;

	return header;
}

struct mip_sdu construct_mip_sdu(uint8_t type, uint8_t mip_addr)
{
	struct mip_sdu sdu;
	sdu.type = type;
	sdu.mip_addr = mip_addr;

	return sdu;
}
