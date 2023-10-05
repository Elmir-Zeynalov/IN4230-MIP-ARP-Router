#include <stdio.h>
#include "mip.h"

void print_mip_frame(struct mip_frame *m)
{
	printf("**** PRINTING MIP_FRAME ****\n");
	printf("dst_MIP_addr: %d\n", m->dst_addr );
	printf("src_MIP_addr: %d\n",m->src_addr);
	printf("ttl: %d\n", m->ttl);
	printf("sdu_len: %d\n", m->sdu_len);
	printf("sdu_type: %d\n", m->sdu_type);
	printf("***************************\n");

}
