#pragma once

#include "client.h"

#define C_READY      1
#define C_NOT_READY  2
#define C_WALL       3
#define C_SOFT       4
#define C_EMPTY      5
#define C_BOMB       6
#define C_EXPLOSION  7
#define C_BONUS      8
#define C_PLAYER     9
#define C_ME         10
#define C_BORDER     11

// colors 0-7 are standard terminal colors, 
// can use 8+ for custom colors if terminal supports it
#define COLOR_GRAY 8

void draw_init_colors(void);
void draw_lobby(const client_state_t *state);
void draw_running(const client_state_t *state);
void draw_end(const client_state_t *state);
void draw_map_select(const client_state_t *state);
