#include <stdint.h>
#include <stdlib.h>		/* free */
#include <stdio.h>		/* printf */
#include <unistd.h>		/* fgets */
#include <string.h>		/* memset */
#include <sys/socket.h>	/* socket */
#include <fcntl.h>
#include <sys/epoll.h>			/* epoll */
#include <linux/if_packet.h>	/* AF_PACKET */
#include <net/ethernet.h>		/* ETH_* */
#include <arpa/inet.h>			/* htons */
#include <ifaddrs.h>			/* getifaddrs */



int main(int argc, char *argv[])
{
	int opt, hflag = 0, dflag = 0, socket_upper = -1;
	char *MIP_address = NULL;

	while ((opt = getopt(argc, argv, "hds:m:")) != -1) {
		switch (opt) {
			case 'h':
				hflag = 1;
				break;
			case 'd':
				dflag = 1;
				break;
			case 's':
				socket_upper = atoi(optarg);
				break;
			case 'm':
				MIP_address = optarg;
				break;
			default:
				printf("Usage: %s [-h] [-d] -s <socket_upper> -m <MIP_address>\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (socket_upper == -1 || MIP_address == NULL) {
		printf("Both socket_upper and MIP_address are required.\n");
		printf("Usage: %s [-h] [-d] -s <socket_upper> -m <MIP_address>\n", argv[0]);
		exit(EXIT_FAILURE);
	}	

	return 0;
}
