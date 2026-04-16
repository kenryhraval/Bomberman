#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../shared/protocol.h"

#define PORT 6969

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in remote_address;

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

    // 4. accept
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        return 1;
    }

    printf("Client connected\n");

    // 5. receive HELLO
    msg_generic_t header;
    msg_hello_t hello;

    if (recv_hello(client_fd, &header, &hello) < 0) {
        printf("Failed to receive HELLO\n");
        return 1;
    }

    printf("Received HELLO:\n");
    printf("  client_id: %s\n", hello.client_id);
    printf("  player_name: %s\n", hello.player_name);

    // 6. send WELCOME (minimal version)
    msg_welcome_t welcome = {0};
    snprintf(welcome.server_id, sizeof(welcome.server_id), "bomb-server-0.1");
    welcome.game_status = GAME_LOBBY;
    welcome.other_count = 0;

    if (send_welcome(client_fd, server_fd, client_fd, &welcome) < 0) {
        printf("Failed to send WELCOME\n");
        return 1;
    }

    printf("Sent WELCOME\n");

    // 7. cleanup
    close(client_fd);
    close(server_fd);

    return 0;
}

