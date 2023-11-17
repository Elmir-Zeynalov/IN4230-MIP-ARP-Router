#ifndef _DAEMON_ROUTING_H
#define _DAEMON_ROUTING_H

#include "ether.h"
#include "cache.h"
#include "mip.h"
#include "queue.h"
#include "debug.h"
#include "common.h"
#include <bits/types/struct_iovec.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>            /* getifaddrs */
#include <arpa/inet.h>          /* htons */
#include <sys/socket.h>



int disseminate_hello_message(struct ifs_data *ifs, uint8_t *src_mip, char *message, size_t len, struct Cache *cache);

#endif
