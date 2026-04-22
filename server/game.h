#pragma once
#include "server.h"

#define TICK_RATE 20 // 20 ticks per second => 50ms per tick

void *game_loop(void *arg);

void game_tick(void *arg);

void handle_move(server_state_t *state, event_t *ev);
void broadcast_move(server_state_t *state, uint8_t player_id, uint16_t cell);