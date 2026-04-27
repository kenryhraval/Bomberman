CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE -lrt
# -I. is needed to find the shared/protocol.h header from both server and client directories
CPPFLAGS=-I.

all: server client

server: build
	$(CC) $(CPPFLAGS) $(CFLAGS) server/main.c server/map.c server/game.c server/server.c server/handle_client.c server/event_queue.c server/server_protocol.c server/broadcasts.c shared/protocol.c -o build/server_app

client: build
	$(CC) $(CPPFLAGS) $(CFLAGS) client/client.c client/main.c client/draw.c client/helpers.c client/client_protocol.c shared/protocol.c -o build/client_app -lncursesw

build:
	mkdir -p build
 
clean:
	rm -f build/server_app build/client_app

.PHONY: all server client build clean
