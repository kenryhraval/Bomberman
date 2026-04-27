#pragma once
#include "server.h"
#include "shared/protocol.h"


void broadcast_move(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_bomb(server_state_t *state, uint8_t player_id, uint16_t cell);
void broadcast_death(server_state_t *state, uint8_t player_id);
void broadcast_explosion_start(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_explosion_end(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius);
void broadcast_block_destroyed(server_state_t *state, uint16_t cell);
void broadcast_winner(server_state_t *state, uint8_t winner_id);
void broadcast_bonus_available(server_state_t *state, bonus_type_t bonus_type, uint16_t cell);
void broadcast_bonus_collected(server_state_t *state, uint8_t player_id, uint16_t cell);
