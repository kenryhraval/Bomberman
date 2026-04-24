#pragma once
#include <stdint.h>
#include "../shared/common.h"

typedef struct {
    uint16_t player_speed;
    uint16_t explosion_duration_ticks;
    uint8_t explosion_radius;
    uint16_t bomb_timer_ticks;
    uint16_t start_row[MAX_PLAYERS];
    uint16_t start_col[MAX_PLAYERS];
} config_t;

int load_map(const char *filename, map_t *map, config_t *config);
