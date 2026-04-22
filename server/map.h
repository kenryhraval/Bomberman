#pragma once
#include <stdint.h>
#include "../shared/common.h"

#define MAX_MAP_ROWS 255
#define MAX_MAP_COLS 255

typedef enum
{
    HARD_BLOCK = 'H',
    SOFT_BLOCK = 'S',
    EMPTY = '.',
    BOMB = 'B',
    SPEED_BONUS = 'A',
    BOMB_RADIUS_BONUS = 'R',
    BOMB_TIMER_BONUS = 'T',
    PLAYER_1 = '1',
    PLAYER_LAST = '1' + MAX_PLAYERS - 1,
} cell_type_t;

typedef struct
{
    uint16_t player_speed;
    uint16_t explosion_duration_ticks;
    uint8_t explosion_radius;
    uint16_t bomb_timer_ticks;
    uint16_t start_row[MAX_PLAYERS];
    uint16_t start_col[MAX_PLAYERS];
} game_config_t;

typedef struct
{
    uint8_t rows;
    uint8_t cols;
    uint8_t cells[MAX_MAP_ROWS * MAX_MAP_COLS];
    game_config_t configs;
} map_t;

int load_map(const char *filename, map_t *map);