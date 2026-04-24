#include "client.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

int client_connect(client_state_t *game_state, const char *ip, int port)
{
    struct sockaddr_in addr;

    game_state->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (game_state->fd < 0) {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(game_state->fd);
        return -1;
    }

    if (connect(game_state->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(game_state->fd);
        return -1;
    }

    return 0;
}

int client_handshake(client_state_t *game_state, const char *player_name)
{
    msg_hello_t hello = {0};
    msg_generic_t header;
    msg_welcome_t welcome;

    snprintf(hello.client_id, sizeof(hello.client_id), "bomb-client-0.1");
    snprintf(hello.player_name, sizeof(hello.player_name), "%s", player_name);

    if (send_hello(game_state->fd, 255, 255, &hello) < 0)
        return -1;

    if (recv_welcome(game_state->fd, &header, &welcome) < 0)
        return -1;

    game_state->my_id = header.target_id;
    game_state->game_status = welcome.game_status;

    strncpy(game_state->server_id, welcome.server_id, MAX_CLIENT_ID_LEN);
    game_state->server_id[MAX_CLIENT_ID_LEN] = '\0';

    game_state->player_count = 1 + welcome.other_count;

    game_state->players[game_state->my_id].id = game_state->my_id;
    strncpy(game_state->players[game_state->my_id].name, player_name, MAX_NAME_LEN);
    game_state->players[game_state->my_id].name[MAX_NAME_LEN] = '\0';
    game_state->players[game_state->my_id].ready = false;

    for (int i = 0; i < welcome.other_count; i++) {
        uint8_t id = welcome.others[i].player_id;

        game_state->players[id].id = id;
        game_state->players[id].ready = welcome.others[i].ready;
        strncpy(game_state->players[id].name, welcome.others[i].name, MAX_NAME_LEN);
        game_state->players[id].name[MAX_NAME_LEN] = '\0';
    }

    return 0;
}

int client_send_leave(client_state_t *game_state)
{
    return send_leave(game_state->fd, game_state->my_id, 255);
}

void client_close(client_state_t *game_state)
{
    if (game_state->fd >= 0) {
        close(game_state->fd);
        game_state->fd = -1;
    }
}

int client_poll_network(client_state_t *game_state)
{
    struct pollfd pfd;
    msg_generic_t header;

    pfd.fd = game_state->fd;
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
        if (read_exact(game_state->fd, &header, sizeof(header)) < 0) {
            return -1;
        } 

        if (header.target_id != game_state->my_id && header.target_id != BROADCAST) {
            return -1; // message not intended for us
        }

        if (header.msg_type == MSG_WELCOME) {
            msg_welcome_t welcome;

            if (read_exact(game_state->fd, &welcome, sizeof(welcome)) < 0)
                return -1;

            game_state->my_id = header.target_id;
            game_state->game_status = welcome.game_status;

            strncpy(game_state->server_id, welcome.server_id, MAX_CLIENT_ID_LEN);
            game_state->server_id[MAX_CLIENT_ID_LEN] = '\0';

            for (int i = 0; i < welcome.other_count; i++) {
                uint8_t id = welcome.others[i].player_id;

                game_state->players[id].id = id;
                game_state->players[id].ready = welcome.others[i].ready;
                strncpy(game_state->players[id].name, welcome.others[i].name, MAX_NAME_LEN);
                game_state->players[id].name[MAX_NAME_LEN] = '\0';
            }

        } else if (header.msg_type == MSG_HELLO) {
            msg_hello_t payload;

            if (read_exact(game_state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            uint8_t id = header.sender_id;

            if (game_state->players[id].name[0] == '\0')
                game_state->player_count++;

            game_state->players[id].id = id;
            game_state->players[id].ready = false;
            strncpy(game_state->players[id].name, payload.player_name, MAX_NAME_LEN);
            game_state->players[id].name[MAX_NAME_LEN] = '\0';

        } else if (header.msg_type == MSG_SET_READY) {
            uint8_t id = header.sender_id;
            game_state->players[id].ready = true;
        } else if (header.msg_type == MSG_LEAVE) {
            uint8_t id = header.sender_id;
            game_state->players[id].id = 0;
            game_state->players[id].name[0] = '\0';
            game_state->players[id].ready = false;
            game_state->player_count--;

        } else if (header.msg_type == MSG_SET_STATUS) {
            msg_set_status_t payload;
            if (read_exact(game_state->fd, &payload, sizeof(payload)) < 0)
                return -1;
            
            game_state->game_status = payload.game_status;

        } else if (header.msg_type == MSG_MAP) {
            msg_map_t payload;
            if (read_exact(game_state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            if (payload.width > MAX_MAP_WIDTH || payload.height > MAX_MAP_HEIGHT)
                return -1;
            
            uint16_t cell_count = payload.height * payload.width;

            game_state->map.rows = payload.height;
            game_state->map.cols = payload.width;

            if (read_exact(game_state->fd, game_state->map.cells, cell_count) < 0)
                return -1;

        } else if (header.msg_type == MSG_WINNER) {
            msg_winner_t payload;
            
            if (read_exact(game_state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            game_state->winner_id = payload.winner_id;

        } else {
            return -1; // unknown message type
        }
    }

    return 0;
}

