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
	int opt, hflag = 0, dflag = 0;

	while ((opt = getopt(argc, argv, "sc")) != -1) {
		switch (opt) {
			case 'h':
				hflag = 1;
				break;
			case 'd':
				/* Running in client mode */
				dflag = 1;
				break;
			default: /* '?' */
				printf("Usage: %s "
						"[-h] prints help and exits the program "
						"[-d] enable debugging mode\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	printf("H: %d\n", hflag);
	printf("D: %d\n", dflag);
	return 0;
}
