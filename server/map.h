#pragma once
#include <stdint.h>
#include <stddef.h>
#include "shared/common.h"

#define MAPS_DIR "maps"
#define MAX_MAP_PATH_LEN 256

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

typedef struct {
    char name[MAX_MAP_NAME_LEN];
    char path[MAX_MAP_PATH_LEN];

    uint8_t rows;
    uint8_t cols;
    uint16_t player_speed;
    uint16_t explosion_duration_ticks;
    uint8_t explosion_radius;
    uint16_t bomb_timer_ticks;

    uint8_t supported_players;
} map_choice_t;

int load_map(const char *filename, server_state_t *server_state);
size_t scan_map_choices(const char *maps_dir, map_choice_t choices[], size_t max_choices);
int find_available_map_choices_and_send(server_state_t *state, int fd, uint8_t target_id);