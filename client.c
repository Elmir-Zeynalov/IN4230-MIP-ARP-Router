#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ping_client.h"

int main (int argc, char *argv[])
{
	char *socket_lower = argv[1];
	uint8_t destination_host = atoi(argv[2]);
	char *message = argv[3];
	printf("This is the PING_CLIENT speaking\n");
	printf("Socket Lower: %s\nDestination Host: %d\nMessage: %s\n", socket_lower, destination_host, message);
	client(socket_lower, &destination_host,message,strlen(message));
	return 0;
}
