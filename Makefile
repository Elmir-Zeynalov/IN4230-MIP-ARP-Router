CC = gcc
CFLAGS = -g -std=gnu11 -Wall -Wextra

mipd:daemon.c common.c cache.c
	$(CC) $(CFLAGS) daemon.c  common.c mip.c cache.c -o mipd

clean:
	rm -f mipd
