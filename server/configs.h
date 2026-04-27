#pragma once
#include "shared/common.h"

/* Game Configurations */
#define BLOCK_DESTROY_BONUS_CHANCE 0.2 // 20% chance to spawn a bonus when a soft block is destroyed
#define BOMB_EXPLOSION_BONUS_INCREASE_TICKS 10

/* Server Configurations */
#define MAP_ARGUMENT "--map"
#define MAP_FILENAME_DEFAULT "map.txt"
#define CLIENT_TIMEOUT_SECONDS 30

#define PORT 6969
#define CLIENT_ID "bomb-client-0.1"
#define SERVER_ID "bomb-server-0.1"
#define MAX_BOMBS (START_BOMB_COUNT * MAX_PLAYERS)