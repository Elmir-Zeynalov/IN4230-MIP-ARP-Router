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


void usage(char *arg)
{
	printf("Usage: %s [-h] [-d] <socket_upper> <MIP_address>\nOptions:\n\t-h prints help and exits program\n\t-d enables debugging mode\nRequired Arguments:\n\tsocket_upper: pathname of the UNIX socket used to interface with upper layers\n\tMIP address: the MIP address to assign to this host\n", arg);
}

int main(int argc, char *argv[])
{
	int opt;
	int hflag = 0;
	int dflag = 0;
	int socket_upper = -1;
	char *MIP_address = NULL;

	while((opt = getopt(argc, argv, "dh")) != -1)
	{
		switch (opt) {
			case 'h':
				hflag = 1;
				break;
			case 'd':
				dflag = 1;
				break;
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if(hflag)
	{
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}


	return 0;
}
