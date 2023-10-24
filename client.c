#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include "ping_client.h"


// Usage method to provide user with feedback on how to run the program
void usage(char *arg)
{
        printf("Usage: %s [-h] <socket_lower> <destination_host> <message>\n"
                "Options:\n"
                "\t-h: prints help and exits program\n"
                "\tsocket_lower: pathname of the UNIX socket used to interface with upper layers\n"
                "\t<destination_host>: the MIP address you are sending to\n"
                "\t<message>: message you want to send\n" , arg);
}


int main (int argc, char *argv[])
{
	int hflag = 0;
        int opt; 
	while((opt = getopt(argc, argv, "h")) != -1)
        {
                switch (opt) {
                        case 'h':
                                hflag = 1;
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
        
        if(argc < 4)
        {
                usage(argv[0]);
                exit(EXIT_SUCCESS);
        }

	char *socket_lower = argv[1];
	uint8_t destination_host = atoi(argv[2]);
	char *message = argv[3];
	client(socket_lower, &destination_host,message,strlen(message));
	return 0;
}
