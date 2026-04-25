#pragma once
#include <stdint.h>
#include "shared/common.h"

// forward declaration to avoid circular dependency with server.h
typedef struct server_state server_state_t;

typedef struct {
    uint16_t player_speed;
    uint16_t explosion_duration_ticks;
    uint8_t explosion_radius;
    uint16_t bomb_timer_ticks;
    uint16_t start_row[MAX_PLAYERS];
    uint16_t start_col[MAX_PLAYERS];
} config_t;

int load_map(const char *filename, server_state_t *server_state);
