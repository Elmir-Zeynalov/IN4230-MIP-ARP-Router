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
