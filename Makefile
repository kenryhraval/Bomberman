CC=gcc
CFLAGS=-Wall -Wextra -std=c11

all: server client

server: build
	$(CC) $(CFLAGS) server/main.c server/map.c server/game.c server/server.c server/handle_client.c server/event_queue.c shared/protocol.c -o build/server_app

client: build
	$(CC) $(CFLAGS) client/client.c client/main.c client/helpers.c shared/protocol.c -o build/client_app -lncursesw

build:
	mkdir -p build

clean:
	rm -f build/server_app build/client_app

.PHONY: all server client build clean
