CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all clean

all: client1 client2

client1: src/client1.c
	$(CC) $(CFLAGS) -o $@ $<

client2: src/client2.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f client1 client2