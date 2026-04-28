#pragma once
#include "server.h"
#include "shared/protocol.h"


void broadcast_bomb(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_death(server_state_t *state, uint8_t player_id);
void broadcast_explosion_start(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_explosion_end(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_block_destroyed(server_state_t *state, uint16_t cell);
void broadcast_winner(server_state_t *state, uint8_t winner_id);
void broadcast_bonus_available(server_state_t *state, bonus_type_t bonus_type, uint16_t cell);
void broadcast_bonus_collected(server_state_t *state, uint8_t player_id, uint16_t cell);

void broadcast_leave(server_state_t *state, int sender_idx);
void broadcast_set_ready(server_state_t *state, int sender_idx);
void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_game_status(server_state_t *state, game_status_t status);
void broadcast_moved(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_map(server_state_t *state);
