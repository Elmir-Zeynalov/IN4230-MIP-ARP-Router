#include "utilities.h"

void print_mac_addr(uint8_t *addr, size_t len)
{
        size_t i;

        for (i = 0; i < len - 1; i++) {
                printf("%02x:", addr[i]);
        }
        printf("%02x ", addr[i]);
}


void usage(char *arg)
{
        printf("Usage: %s [-h] [-d] <socket_upper> <MIP_address>\n"
                "Options:\n"
                "\t-h: prints help and exits program\n"
                "\t-d: enables debugging mode\nRequired Arguments:\n"
                "\tsocket_upper: pathname of the UNIX socket used to interface with upper layers\n"
                "\tMIP address: the MIP address to assign to this host\n", arg);
}

