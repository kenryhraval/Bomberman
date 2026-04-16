#include "client.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

int client_connect(client_state_t *client, const char *ip, int port)
{
    struct sockaddr_in addr;

    client->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->fd < 0) {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(client->fd);
        return -1;
    }

    if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(client->fd);
        return -1;
    }

    return 0;
}

int client_handshake(client_state_t *client)
{
    msg_hello_t hello = {0};
    msg_generic_t header;

    snprintf(hello.client_id, sizeof(hello.client_id), "bomb-client-0.1");
    snprintf(hello.player_name, sizeof(hello.player_name), "%s", client->name);

    if (send_hello(client->fd, 255, 255, &hello) < 0) {
        return -1;
    }

    if (recv_welcome(client->fd, &header, &client->welcome) < 0) {
        return -1;
    }

    client->my_id = header.target_id;
    return 0;
}

int client_send_leave(client_state_t *client)
{
    return send_leave(client->fd, client->my_id, 255);
}

void client_close(client_state_t *client)
{
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
}

int client_poll_network(client_state_t *client)
{
    struct pollfd pfd;
    msg_generic_t header;

    pfd.fd = client->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, 0);
    if (ret < 0) {
        return -1;
    }

    if (ret == 0) {
        return 0; // no new data
    }

    if (pfd.revents & POLLIN) {
        if (read_exact(client->fd, &header, sizeof(header)) < 0) {
            return -1;
        }

        if (header.msg_type == MSG_WELCOME) {
            if (read_exact(client->fd, &client->welcome, sizeof(client->welcome)) < 0) {
                return -1;
            }

            client->my_id = header.target_id;
            
        } else {
            // later: handle more message types
            return 0;
        }
    }

    return 0;
}

