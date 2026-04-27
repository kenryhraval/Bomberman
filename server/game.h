#pragma once
#include "server.h"

void *game_loop(void *arg);
void game_tick(void *arg);

void handle_move(server_state_t *state, event_t *ev);
void handle_bomb(server_state_t *state, event_t *ev);
void explode(server_state_t *state, int bomb_idx);
void calculate_explosion_footprint(server_state_t *state, explosion_t *expl);

void check_player_deaths(server_state_t *state, uint16_t row, uint16_t col, uint8_t killer_id);
void check_win_condition(server_state_t *state);

void maybe_spawn_bonus(server_state_t *state, uint16_t row, uint16_t col);
