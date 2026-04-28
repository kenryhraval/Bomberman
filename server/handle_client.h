#pragma once
#include "server.h"

#include <time.h>

typedef struct
{
    server_state_t *state;
    int client_idx;
} client_thread_args_t;

typedef struct
{
    timer_t *timer_id;
    client_t *client;
    server_state_t *state;
} watchdog_param_t;

void *client_loop(void *args);

void broadcast_leave(server_state_t *state, int sender_idx);
void broadcast_set_ready(server_state_t *state, int sender_idx);
void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_game_status(server_state_t *state, game_status_t status);
void broadcast_map(server_state_t *state);
void sync_board_to_client(server_state_t *state, int idx);

bool all_players_ready(server_state_t *state);
void start_game(server_state_t *state);

void watchdog_handler(union sigval sv);
void start_player_watchdog(timer_t *timer_id, client_t *client, server_state_t *state, watchdog_param_t **out_param);
void feed_player_watchdog(timer_t *timer_id);