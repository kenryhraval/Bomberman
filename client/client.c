#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include "configs.h"

#include "client.h"
#include "helpers.h"

int client_connect(client_state_t *state, const char *ip, int port)
{
    struct sockaddr_in addr;

    state->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (state->fd < 0) {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(state->fd);
        return -1;
    }

    if (connect(state->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(state->fd);
        return -1;
    }

    return 0;
}

int client_handshake(client_state_t *state, const char *player_name)
{
    msg_hello_t hello = {0};
    msg_generic_t header;
    msg_welcome_t welcome;

    snprintf(hello.client_id, sizeof(hello.client_id), CLIENT_ID);
    snprintf(hello.player_name, sizeof(hello.player_name), "%s", player_name);

    if (send_hello(state->fd, 255, 255, &hello) < 0)
        return -1;

    if (recv_welcome(state->fd, &header, &welcome) < 0)
        return -1;

    uint8_t my_id = header.target_id;

    state->my_id = my_id;
    state->game_status = welcome.game_status;
    strncpy(state->server_id, welcome.server_id, MAX_CLIENT_ID_LEN);
    state->server_id[MAX_CLIENT_ID_LEN] = '\0';
    state->player_count = 1 + welcome.other_count;

    state->players[my_id].id = my_id;
    strncpy(state->players[my_id].name, player_name, MAX_NAME_LEN);
    state->players[my_id].name[MAX_NAME_LEN] = '\0';
    state->players[my_id].ready = false;
    state->players[my_id].alive = true;

    for (int i = 0; i < welcome.other_count; i++) {
        uint8_t id = welcome.others[i].player_id;

        state->players[id].id = id;
        state->players[id].ready = welcome.others[i].ready;
        strncpy(state->players[id].name, welcome.others[i].name, MAX_NAME_LEN);
        state->players[id].name[MAX_NAME_LEN] = '\0';
        state->players[id].alive = true;
    }

    return 0;
}


int client_send_leave(const client_state_t *state)
{
    return send_leave(state->fd, state->my_id, 255);
}


void client_close(client_state_t *state)
{
    if (state->fd >= 0) {
        close(state->fd);
        state->fd = -1;
    }
}


int client_poll_network(client_state_t *state)
{
    struct pollfd pfd;
    msg_generic_t header;

    pfd.fd = state->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, 0);
    if (ret < 0) {
        return -1; // kļūda poll izsaukumā
    }

    if (ret == 0) {
        return 0; // nav datu lasīšanai
    }

    if (pfd.revents & POLLIN) {
        if (read_exact(state->fd, &header, sizeof(header)) < 0) {
            return -1;
        } 

        if (header.target_id != state->my_id && header.target_id != BROADCAST) {
            return -1; // ziņa nav domāta šim klientam
        }

        if (header.msg_type == MSG_HELLO) {
            msg_hello_t payload;

            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            uint8_t id = header.sender_id;

            if (state->players[id].name[0] == '\0')
                state->player_count++;

            state->players[id].id = id;
            state->players[id].ready = false;
            state->players[id].alive = true;
            strncpy(state->players[id].name, payload.player_name, MAX_NAME_LEN);
            state->players[id].name[MAX_NAME_LEN] = '\0';

        } else if (header.msg_type == MSG_SET_READY) {
            uint8_t id = header.sender_id;
            state->players[id].ready = true;

        } else if (header.msg_type == MSG_LEAVE) {
            uint8_t id = header.sender_id;
            state->players[id].id = 0;
            state->players[id].name[0] = '\0';
            state->players[id].ready = false;
            state->player_count--;

        } else if (header.msg_type == MSG_SET_STATUS) {
            msg_set_status_t payload;
            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;
            
            state->game_status = payload.game_status;

        } else if (header.msg_type == MSG_MAP) {
            msg_map_t payload;
            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;
            
            uint16_t cell_count = payload.height * payload.width;

            if (cell_count > MAX_MAP_ROWS * MAX_MAP_COLS)
                return -1;

            // sagatavo pamatkarti
            state->map.rows = payload.height;
            state->map.cols = payload.width;

            if (read_exact(state->fd, state->map.cells, cell_count) < 0)
                return -1;

            // sagatavo pārklājuma karti
            state->overlay_map.rows = payload.height;
            state->overlay_map.cols = payload.width;
            memset(state->overlay_map.cells, EMPTY, cell_count);

        } else if (header.msg_type == MSG_WINNER) {
            msg_winner_t payload;
            
            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            state->winner_id = payload.winner_id;

        } else if (header.msg_type == MSG_MOVED) {
            msg_moved_t moved;

            if (read_exact(state->fd, &moved, sizeof(moved)) < 0)
                return -1;

            uint8_t id = moved.player_id;
            uint16_t cell = ntohs(moved.cell);

            state->players[id].row = cell / state->map.cols;
            state->players[id].col = cell % state->map.cols;
            
        } else if (header.msg_type == MSG_BOMB) {
            msg_bomb_t bomb;

            if (read_exact(state->fd, &bomb, sizeof(bomb)) < 0)
                return -1;

            uint16_t cell = ntohs(bomb.cell);
            // atzīmē bumbu uz pamatkartes
            state->map.cells[cell] = BOMB;
           
        } else if (header.msg_type == MSG_EXPLOSION_START) {
            msg_explosion_start_t explosion;

            if (read_exact(state->fd, &explosion, sizeof(explosion)) < 0)
                return -1;

            uint16_t cell = ntohs(explosion.cell);
            uint8_t radius = explosion.radius;
            // atzīmē sprādziena efektu pārklājuma kartē
            mark_explosion(state, cell, radius, 'X');

        } else if (header.msg_type == MSG_EXPLOSION_END) {
            msg_explosion_end_t explosion;

            if (read_exact(state->fd, &explosion, sizeof(explosion)) < 0)
                return -1;

            uint16_t cell = ntohs(explosion.cell);
            uint8_t radius = explosion.radius;
            // noņem sprādziena efektu pārklājuma kartē
            mark_explosion(state, cell, radius, EMPTY);
            // noņem bumbu no pamatkartes
            state->map.cells[cell] = EMPTY;

        } else if (header.msg_type == MSG_DEATH) {
            msg_death_t death;

            if (read_exact(state->fd, &death, sizeof(death)) < 0)
                return -1;

            uint8_t id = death.player_id;
            state->players[id].alive = false;

        } else if (header.msg_type == MSG_BONUS_AVAILABLE) {
            msg_bonus_available_t bonus;

            if (read_exact(state->fd, &bonus, sizeof(bonus)) < 0)
                return -1;

            // atzīmē pieejamo bonusu uz pamatkartes
            uint16_t cell = ntohs(bonus.cell);
            state->map.cells[cell] = bonus.bonus_type;

        } else if (header.msg_type == MSG_BONUS_RETRIEVED) {
            msg_bonus_retrieved_t bonus;

            if (read_exact(state->fd, &bonus, sizeof(bonus)) < 0)
                return -1;

            uint16_t cell = ntohs(bonus.cell);
            // atzīmē, ka paņemto bonusu uz pamatkartes
            state->map.cells[cell] = EMPTY;

        } else if (header.msg_type == MSG_BLOCK_DESTROYED) {
            msg_block_destroyed_t block;

            if (read_exact(state->fd, &block, sizeof(block)) < 0)
                return -1;

            uint16_t cell = ntohs(block.cell);
            // atzīmē iznīcināto bloku uz pamatkartes
            state->map.cells[cell] = EMPTY;

        } else if (header.msg_type == MSG_STATISTICS) {
            msg_statistics_t payload;

            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            // pārliecināties par endiness sakritību, saņemot statistiku
            state->stats.kills = payload.stats.kills;
            state->stats.blocks_destroyed = ntohs(payload.stats.blocks_destroyed);
            state->stats.bonuses_collected = ntohs(payload.stats.bonuses_collected);
        
        } else if (header.msg_type == MSG_PING) {
            // server checks if client is still alive
            if (send_pong(state->fd, state->my_id, SERVER) < 0)
                return -1;
                
        } else if (header.msg_type == MSG_MAP_CHOICES) {
            msg_map_choices_t payload;

            if (read_exact(state->fd, &payload, sizeof(payload)) < 0)
                return -1;

            // TODO: store/display choices later
            
        } else {
            return -1; // nezināms ziņas tips
        }
    }

    return 0;
}

