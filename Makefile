CC     = gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean

all: server client

server: server.c
	$(CC) $(CFLAGS) -o $@ $<

client: client.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	bash test.sh

clean:
	rm -f server client
