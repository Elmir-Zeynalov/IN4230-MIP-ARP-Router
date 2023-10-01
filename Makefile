CC = gcc
CFLAGS = -g -std=gnu11 -Wall -Wextra

mipd:daemon.c
	$(CC) $(CFLAGS) daemon.c -o mipd

clean:
	rm -f mipd
