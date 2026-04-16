#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../shared/protocol.h"

#define PORT 6969

typedef struct {
    int fd;
    uint8_t id;
    int connected;
} client_t;

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in remote_address;

    client_t clients[MAX_PLAYERS] = {0};
    player_t players[MAX_PLAYERS] = {0};
    uint8_t next_id = 0;

    // 1. create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // 2. bind
    remote_address.sin_family = AF_INET;
    remote_address.sin_addr.s_addr = INADDR_ANY;
    remote_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&remote_address, sizeof(remote_address)) < 0) {
        perror("bind");
        return 1;
    }

    // 3. listen
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        msg_generic_t header;
        msg_hello_t hello;

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        if (next_id >= MAX_PLAYERS) {
            printf("Server full, rejecting client\n");
            close(client_fd);
            continue;
        }

        printf("Client connected\n");

        if (recv_hello(client_fd, &header, &hello) < 0) {
            printf("Failed to receive HELLO\n");
            close(client_fd);
            continue;
        }

        printf("Received HELLO:\n");
        printf("  player_id: %s\n", hello.player_id);
        printf("  player_name: %s\n", hello.player_name);

        clients[next_id].fd = client_fd;
        clients[next_id].id = next_id;
        clients[next_id].connected = 1;

        strncpy(players[next_id].id, hello.player_id, MAX_CLIENT_ID_LEN);
        players[next_id].id[MAX_CLIENT_ID_LEN] = '\0';
        strncpy(players[next_id].name, hello.player_name, MAX_NAME_LEN);
        players[next_id].name[MAX_NAME_LEN] = '\0';

        msg_welcome_t welcome = {0};
        snprintf(welcome.server_id, sizeof(welcome.server_id), "bomb-server-0.1");
        welcome.game_status = GAME_LOBBY;
        welcome.other_count = next_id;

        if (send_welcome(client_fd, 255, next_id, &welcome) < 0) {
            printf("Failed to send WELCOME\n");
            close(client_fd);
            clients[next_id].connected = 0;
            continue;
        }

        printf("Sent WELCOME to player %u\n", next_id);

        next_id++;
    }


    close(server_fd);

    return 0;
}

