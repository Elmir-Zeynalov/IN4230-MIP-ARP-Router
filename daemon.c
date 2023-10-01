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
	printf("Usage: %s [-h] [-d] <socket_upper> <MIP_address>\nOptions:\n\t-h: prints help and exits program\n\t-d: enables debugging mode\nRequired Arguments:\n\tsocket_upper: pathname of the UNIX socket used to interface with upper layers\n\tMIP address: the MIP address to assign to this host\n", arg);
}

int main(int argc, char *argv[])
{
	int opt;
	int hflag = 0;
	int dflag = 0;
	char *socket_upper = NULL;
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

	printf("argc: %d\n", argc);
	if(hflag)
	{
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if(dflag && argc < 4)
	{
		printf("Dflag and missing reqs\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if(!dflag && argc < 3)
	{
		printf("NO Dflag and missing args\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	socket_upper = argv[dflag ? 2 : 1];
	MIP_address = argv[dflag ? 3 : 2];
/*
	if(dflag)
	{
		socket_upper = argv[2];
		MIP_address = argv[3];
	}else
	{
		socket_upper = argv[1];
		MIP_address = argv[2];
	}
*/
	printf("Socket Upper: %s\n", socket_upper);
	printf("MIP Address: %s\n", MIP_address);

	return 0;
}
