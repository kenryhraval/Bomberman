#pragma once
#include "server.h"

#define TICK_RATE 20 // 20 ticks per second => 50ms per tick

void *game_loop(void *arg);

void game_tick(void *arg);

void handle_move(server_state_t *state, event_t *ev);
void handle_bomb(server_state_t *state, event_t *ev);
void explode(server_state_t *state, int bomb_idx);

void broadcast_move(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_bomb(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_death(server_state_t *state, uint8_t player_id);
void broadcast_explosion_start(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_explosion_end(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_block_destroyed(server_state_t *state, uint16_t cell);
void broadcast_winner(server_state_t *state, uint8_t winner_id);
// void broadcast_set_game_status(server_state_t *state, game_status_t status);

void check_player_deaths(server_state_t *state, uint16_t row, uint16_t col);
void check_win_condition(server_state_t *state);

void maybe_spawn_bonus(server_state_t *state, uint16_t row, uint16_t col);