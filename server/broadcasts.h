#pragma once
#include "server.h"
#include "shared/protocol.h"


void broadcast_bomb(const server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_death(const server_state_t *state, uint8_t player_id);
void broadcast_explosion_start(const server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_explosion_end(const server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_block_destroyed(const server_state_t *state, uint16_t cell);
void broadcast_winner(const server_state_t *state, uint8_t winner_id);
void broadcast_bonus_available(const server_state_t *state, bonus_type_t bonus_type, uint16_t cell);
void broadcast_bonus_collected(const server_state_t *state, uint8_t player_id, uint16_t cell);

void broadcast_leave(const server_state_t *state, int sender_idx);
void broadcast_set_ready(const server_state_t *state, int sender_idx);
void broadcast_hello(const server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_game_status(const server_state_t *state, game_status_t status);
void broadcast_moved(const server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_map(const server_state_t *state);
