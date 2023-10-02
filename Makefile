CC = gcc
CFLAGS = -g -std=gnu11 -Wall -Wextra

mipd:daemon.c common.c
	$(CC) $(CFLAGS) daemon.c  common.c mip.c -o mipd

clean:
	rm -f mipd
