#pragma once
#include "server.h"

typedef struct
{
    server_state_t *state;
    int client_idx;
} client_thread_args_t;

void *client_loop(void *args);

void broadcast_leave(server_state_t *state, int sender_idx);
void broadcast_set_ready(server_state_t *state, int sender_idx);
void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_game_status(server_state_t *state, game_status_t status);
void broadcast_map(server_state_t *state);
void send_moved(int fd, uint8_t sender_id, uint8_t target_id, const msg_moved_t *msg);
// void broadcast_sync_board(server_state_t *state);

bool all_players_ready(server_state_t *state);
void start_game(server_state_t *state);