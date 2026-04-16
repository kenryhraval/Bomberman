#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../shared/protocol.h"

int main(int argc, char *argv[]) {
    char *ip = "127.0.0.1";
    int port = 6969;

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int fd;
    struct sockaddr_in addr;

    // 1. create socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    // 2. connect
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server\n");

    // 3. build HELLO
    msg_hello_t hello = {0};
    snprintf(hello.player_id, sizeof(hello.player_id), "bomb-client-0.1");
    snprintf(hello.player_name, sizeof(hello.player_name), "henrijs");

    // 4. send HELLO
    if (send_hello(fd, 255, 255, &hello) < 0) {
        printf("Failed to send HELLO\n");
        return 1;
    }

    printf("Sent HELLO\n");

    // 5. receive WELCOME
    msg_generic_t header;
    msg_welcome_t welcome;

    if (recv_welcome(fd, &header, &welcome) < 0) {
        printf("Failed to receive WELCOME\n");
        return 1;
    }

    printf("Received WELCOME:\n");
    printf("  server_id: %s\n", welcome.server_id);
    printf("  status: %u\n", welcome.game_status);

    printf("Press Enter to quit...\n");
    getchar();

    close(fd);

    return 0;
}

