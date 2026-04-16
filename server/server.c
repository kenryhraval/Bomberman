#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#include "../shared/protocol.h"

#define PORT 6969

typedef struct {
    int fd;
    int connected;
} client_t;


int find_free_slot(client_t clients[]);
void add_client(client_t clients[], player_t players[], int fd, uint8_t *player_count);
void remove_client(client_t clients[], player_t players[], int id, uint8_t *player_count);


int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in remote_address;

    client_t clients[MAX_PLAYERS] = {0};
    player_t players[MAX_PLAYERS] = {0};
    uint8_t player_count = 0;

    // 1. create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // avoid "Address already in use" error 
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. bind
    remote_address.sin_family = AF_INET;
    remote_address.sin_addr.s_addr = INADDR_ANY;
    remote_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&remote_address, sizeof(remote_address)) < 0) {
        perror("bind");
        return 1;
    }

    // 3. listen
    if (listen(server_fd, MAX_PLAYERS) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        struct pollfd pfds[MAX_PLAYERS + 1];
        int nfds = 0;

        pfds[nfds].fd = server_fd;
        pfds[nfds].events = POLLIN;
        nfds++;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected) {
                pfds[nfds].fd = clients[i].fd;
                pfds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int ready = poll(pfds, nfds, -1);
        if (ready < 0) {
            perror("poll");
            continue;
        }

        // new incoming connection
        if (pfds[0].revents & POLLIN) {
            client_fd = accept(server_fd, NULL, NULL);
            add_client(clients, players, client_fd, &player_count);
        }

        // existing client messages
        int idx = 1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].connected) continue;

            if (pfds[idx].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                printf("Client %d disconnected\n", i);
                remove_client(clients, players, i, &player_count);
                if (player_count > 0) player_count--;

            } else if (pfds[idx].revents & POLLIN) {
                msg_generic_t header;

                if (read_exact(clients[i].fd, &header, sizeof(header)) < 0) {
                    printf("Client %d disconnected unexpectedly\n", i);
                    remove_client(clients, players, i, &player_count);
                } else if (header.msg_type == MSG_LEAVE) {
                    printf("Client %d sent LEAVE\n", i);
                    remove_client(clients, players, i, &player_count);
                } else {
                    printf("Unhandled message type %u from client %d\n", header.msg_type, i);
                }
            }

            idx++;
        }
    }

    close(server_fd);
    return 0;
}


int find_free_slot(client_t clients[]) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].connected) {
            return i;
        }
    }
    return -1;
}

void add_client(client_t clients[], player_t players[], int fd, uint8_t *player_count) {
    msg_generic_t header;
    msg_hello_t hello;
    if (fd < 0) {
        perror("accept");
        return;
    }

    if (*player_count >= MAX_PLAYERS) {
        printf("Server full, rejecting client\n");
        close(fd);
        return;
    }

    int free_id = find_free_slot(clients);
    if (free_id < 0) {
        printf("Server full, rejecting client\n");
        close(fd);
        return;
    }

    printf("Client connected\n");

    if (recv_hello(fd, &header, &hello) < 0) {
        printf("Failed to receive HELLO\n");
        close(fd);
        return;
    }

    printf("Received HELLO:\n");
    printf("  player_name: %s\n", hello.player_name);

    // pārbauda vai atbalsta klienta versiju
    if (strncmp(hello.client_id, "bomb-client-0.1", MAX_CLIENT_ID_LEN) != 0) {
        printf("Unsupported client version: %s\n", hello.client_id);
        close(fd);
        return;
    }

    clients[free_id].fd = fd;
    clients[free_id].connected = 1;

    players[free_id].id = free_id; // should generate non-guessable id
    strncpy(players[free_id].name, hello.player_name, MAX_NAME_LEN);
    players[free_id].name[MAX_NAME_LEN] = '\0';                 

    msg_welcome_t welcome = {0};
    snprintf(welcome.server_id, sizeof(welcome.server_id), "bomb-server-0.1");
    welcome.game_status = GAME_LOBBY;
    welcome.other_count = *player_count;
    for (int i = 0, j = 0; i < MAX_PLAYERS && j < welcome.other_count; i++) {
        if (clients[i].connected && i != free_id) {
            welcome.others[j].player_id = players[i].id;
            welcome.others[j].ready = players[i].ready;
            strncpy(welcome.others[j].name, players[i].name, MAX_NAME_LEN);
            welcome.others[j].name[MAX_NAME_LEN] = '\0';
            j++;
        }
    }

    if (send_welcome(fd, SERVER, free_id, &welcome) < 0) {
        printf("Failed to send WELCOME\n");
        close(fd);
        clients[free_id].connected = 0;
        return;
    }

    printf("Sent WELCOME to player %u\n", free_id);

    (*player_count)++;
}


void remove_client(client_t clients[], player_t players[], int id, uint8_t *player_count) {
    close(clients[id].fd);
    clients[id].fd = -1;
    clients[id].connected = 0;

    players[id].id = 0;
    players[id].name[0] = '\0';
    players[id].alive = 0;
    players[id].ready = 0;

    (*player_count)--; 
}

