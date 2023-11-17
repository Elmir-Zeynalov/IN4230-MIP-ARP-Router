CC = gcc
CFLAGS = -g -std=gnu11 -Wall -Wextra

all: mipd ping_server ping_client router

mipd:daemon.c common.c cache.c unix_utils.c ping_client.c queue.c utilities.c debug.c routing_utils.c daemon_routing_utils.c
	$(CC) $(CFLAGS) daemon.c  common.c mip.c cache.c unix_utils.c ping_client.c queue.c utilities.c debug.c routing_utils.c daemon_routing_utils.c -o mipd

ping_client: ping_client.c client.c
	$(CC) $(CFLAGS) ping_client.c client.c -o ping_client

ping_server: server.c
	$(CC) $(CFLAGS) server.c -o ping_server

router: routing_daemon.c debug.c mip.c utilities.c routing_utils.c daemon_routing_utils.c
	$(CC) $(CFLAGS) routing_daemon.c debug.c mip.c utilities.c routing_utils.c daemon_routing_utils.c -o router
clean:
	rm -f mipd
	rm -f ping_client
	rm -f ping_server
	rm -f router
