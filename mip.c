#include <stdio.h>
#include "mip.h"

void print_mip_frame(struct mip_frame *m)
{
	int i;
	printf("**** PRINTING MIP_FRAME ****\n");
	printf("dst_addr: ");
	
	for(i = 0; i < 6 - 1; i++)
		printf("%d:",m->dst_addr[i]);
	printf("%d\n", m->dst_addr[6]);

	printf("src_addr: ");
	for(i = 0; i < 6 - 1; i++)
		printf("%d:",m->src_addr[i]);
	printf("%d\n", m->src_addr[6]);

	printf("ttl: %d\n", m->ttl);
	printf("sdu_len: %d\n", m->sdu_len);
	printf("sdu_type: %d\n", m->sdu_type);
}
