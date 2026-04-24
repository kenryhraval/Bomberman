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

int client_handshake(client_state_t *client, const char *player_name)
{
    msg_hello_t hello = {0};
    msg_generic_t header;
    msg_welcome_t welcome;

    snprintf(hello.client_id, sizeof(hello.client_id), "bomb-client-0.1");
    snprintf(hello.player_name, sizeof(hello.player_name), "%s", player_name);

    if (send_hello(client->fd, 255, 255, &hello) < 0)
        return -1;

    if (recv_welcome(client->fd, &header, &welcome) < 0)
        return -1;

    client->my_id = header.target_id;
    client->game_status = welcome.game_status;

    strncpy(client->server_id, welcome.server_id, MAX_CLIENT_ID_LEN);
    client->server_id[MAX_CLIENT_ID_LEN] = '\0';

    client->player_count = 1 + welcome.other_count;

    client->players[client->my_id].id = client->my_id;
    strncpy(client->players[client->my_id].name, player_name, MAX_NAME_LEN);
    client->players[client->my_id].name[MAX_NAME_LEN] = '\0';
    client->players[client->my_id].ready = false;

    for (int i = 0; i < welcome.other_count; i++) {
        uint8_t id = welcome.others[i].player_id;

        client->players[id].id = id;
        client->players[id].ready = welcome.others[i].ready;
        strncpy(client->players[id].name, welcome.others[i].name, MAX_NAME_LEN);
        client->players[id].name[MAX_NAME_LEN] = '\0';
    }

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
            msg_welcome_t welcome;

            if (read_exact(client->fd, &welcome, sizeof(welcome)) < 0)
                return -1;

            client->my_id = header.target_id;
            client->game_status = welcome.game_status;

            strncpy(client->server_id, welcome.server_id, MAX_CLIENT_ID_LEN);
            client->server_id[MAX_CLIENT_ID_LEN] = '\0';

            for (int i = 0; i < welcome.other_count; i++) {
                uint8_t id = welcome.others[i].player_id;

                client->players[id].id = id;
                client->players[id].ready = welcome.others[i].ready;
                strncpy(client->players[id].name, welcome.others[i].name, MAX_NAME_LEN);
                client->players[id].name[MAX_NAME_LEN] = '\0';
            }

        } else if (header.msg_type == MSG_HELLO) {
            msg_hello_t hello;

            if (read_exact(client->fd, &hello, sizeof(hello)) < 0)
                return -1;

            uint8_t id = header.sender_id;

            if (client->players[id].name[0] == '\0')
                client->player_count++;

            client->players[id].id = id;
            client->players[id].ready = false;
            strncpy(client->players[id].name, hello.player_name, MAX_NAME_LEN);
            client->players[id].name[MAX_NAME_LEN] = '\0';
        } else if (header.msg_type == MSG_SET_READY) {
            uint8_t id = header.sender_id;
            client->players[id].ready = true;
        } else {
            // later: handle more message types
            return 0;
        }
    }

    return 0;
}

