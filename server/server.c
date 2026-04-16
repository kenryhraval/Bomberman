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

typedef struct {
    uint8_t game_status;
    uint8_t player_count;
    client_t clients[MAX_PLAYERS];
    player_t players[MAX_PLAYERS];
} server_state_t;


int find_free_slot(client_t clients[]);
void add_client(server_state_t *state, int fd);
void remove_client(server_state_t *state, int id);
void send_welcome_to_all(server_state_t *state);


int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in remote_address;

    server_state_t state;
    state.game_status = GAME_LOBBY;
    state.player_count = 0;
    memset(state.clients, 0, sizeof(state.clients));
    memset(state.players, 0, sizeof(state.players));

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
            if (state.clients[i].connected) {
                pfds[nfds].fd = state.clients[i].fd;
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
            add_client(&state, client_fd);
        }

        // existing client messages
        int idx = 1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!state.clients[i].connected) continue;

            if (pfds[idx].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                printf("Client %d disconnected\n", i);
                remove_client(&state, i);

            } else if (pfds[idx].revents & POLLIN) {
                msg_generic_t header;

                if (read_exact(state.clients[i].fd, &header, sizeof(header)) < 0) {
                    printf("Client %d disconnected unexpectedly\n", i);
                    remove_client(&state, i);

                } else if (header.msg_type == MSG_LEAVE) {
                    printf("Client %d sent LEAVE\n", i);
                    remove_client(&state, i);

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

void add_client(server_state_t *state, int fd) {
    msg_generic_t header;
    msg_hello_t hello;

    if (fd < 0) {
        perror("accept");
        return;
    }

    if (state->player_count >= MAX_PLAYERS) {
        printf("Server full, rejecting client\n");
        close(fd);
        return;
    }

    int free_id = find_free_slot(state->clients);
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

    // pārbauda vai atbalsta klienta versiju
    if (strncmp(hello.client_id, "bomb-client-0.1", MAX_CLIENT_ID_LEN) != 0) {
        printf("Unsupported client version: %s\n", hello.client_id);
        close(fd);
        return;
    }

    state->clients[free_id].fd = fd;
    state->clients[free_id].connected = 1;
    state->players[free_id].id = free_id; // should generate non-guessable id
    strncpy(state->players[free_id].name, hello.player_name, MAX_NAME_LEN);
    state->players[free_id].name[MAX_NAME_LEN] = '\0';                 

    state->player_count++;

    // informē visus klientus par jauno spēlētāju sastāvu
    send_welcome_to_all(state);
}


void remove_client(server_state_t *state, int id) {
    close(state->clients[id].fd);
    state->clients[id].fd = -1;
    state->clients[id].connected = 0;

    state->players[id].id = 0;
    state->players[id].name[0] = '\0';
    state->players[id].alive = 0;
    state->players[id].ready = 0;

    state->player_count--; 

    // informē visus klientus par jauno spēlētāju sastāvu
    send_welcome_to_all(state);
}


void send_welcome_to_all(server_state_t *state) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->clients[i].connected) {
            msg_welcome_t welcome = {0};
            snprintf(welcome.server_id, sizeof(welcome.server_id), "bomb-server-0.1");
            welcome.game_status = GAME_LOBBY;
            welcome.other_count = state->player_count - 1;
            for (int j = 0, k = 0; j < MAX_PLAYERS && k < welcome.other_count; j++) {
                if (state->clients[j].connected && j != i) {
                    welcome.others[k].player_id = state->players[j].id;
                    welcome.others[k].ready = state->players[j].ready;
                    strncpy(welcome.others[k].name, state->players[j].name, MAX_NAME_LEN);
                    welcome.others[k].name[MAX_NAME_LEN] = '\0';
                    k++;
                }
            }
            send_welcome(state->clients[i].fd, SERVER, state->players[i].id, &welcome);
        }
    }
}

