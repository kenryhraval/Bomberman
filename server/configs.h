#pragma once
#include "shared/common.h"

/* Game Configurations */
#define BLOCK_DESTROY_BONUS_CHANCE 0.2
#define BOMB_EXPLOSION_BONUS_INCREASE_TICKS 10
#define START_BOMB_COUNT 1
#define MAX_BOMBS_PER_PLAYER 10
#define MAX_BOMBS (MAX_BOMBS_PER_PLAYER * MAX_PLAYERS)
#define MAX_BONUSES 255
#define MAX_PLAYER_SPEED 10
#define MAX_BOMB_RADIUS 10
#define MAX_BOMB_EXPLOSION_DURATION_TICKS 100

#define GAME_DRAW_TICK_TIMEOUT (5 * 60 * TICK_RATE)

/* Server Configurations */
#define CLIENT_TIMEOUT_SECONDS 30

#define PORT 6969
#define CLIENT_ID "bomb-client-0.1"
#define SERVER_ID "bomb-server-0.1"

