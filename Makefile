CC=gcc
CFLAGS=-Wall -Wextra -std=c11

all: server client

server:
	$(CC) $(CFLAGS) server/server.c shared/protocol.c -o build/server_app

client:
	$(CC) $(CFLAGS) client/client.c shared/protocol.c -o build/client_app

clean:
	rm -f build/server_app build/client_app

.PHONY: all server client clean
