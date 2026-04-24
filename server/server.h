#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include "event_queue.h"
#include "map.h"

#include "../shared/protocol.h"

#define PORT 6969
#define CLIENT_ID "bomb-client-0.1"
#define SERVER_ID "bomb-server-0.1"
#define MAX_BOMBS (START_BOMB_COUNT * MAX_PLAYERS)

typedef struct {
    bool active;
    uint16_t row, col; // center of explosion
    uint8_t radius;    // radius, client calculates explosion area based on this
    uint16_t duration_ticks;
} explosion_t;

typedef struct {
    int fd;
    int connected;
    time_t last_pong;
    player_t player;
} client_t;

typedef struct {
    game_status_t game_status;
    uint8_t player_count;
    client_t clients[MAX_PLAYERS];
    bomb_t bombs[MAX_BOMBS];
    explosion_t explosions[MAX_BOMBS];
    map_t* map;
    event_queue_t queue;
    pthread_mutex_t mutex;
    uint64_t current_tick;
} server_state_t;

int serve_main(map_t *map);

void broadcast_leave(server_state_t *state, int sender_idx);
void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_ready(server_state_t *state, int sender_idx);
void remove_client_quietly(server_state_t *state, int id);

int find_free_slot(client_t clients[]);
int add_client(server_state_t *state, int fd);
void remove_client(server_state_t *state, int id);
int player_name_in_use(const server_state_t *state, const char *name);