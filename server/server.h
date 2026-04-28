#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>

#include "shared/protocol.h"
#include "server_protocol.h"
#include "event_queue.h"
#include "map.h"
#include "configs.h"

typedef struct {
    bomb_t source;
    uint16_t* footprint;
    size_t footprint_size;
    uint16_t duration_ticks;
} explosion_t;

typedef struct {
    bool active;
    uint16_t row, col;
    bonus_type_t type;
} bonus_t;

typedef struct {
    int fd;
    int connected;
    bool waiting_for_pong;
    struct sockaddr_in addr;
    player_t player;
} client_t;

typedef struct server_state {
    game_status_t game_status;
    uint8_t player_count;
    client_t clients[MAX_PLAYERS];
    bomb_t bombs[MAX_BOMBS];
    explosion_t explosions[MAX_BOMBS];
    bonus_t *bonuses;
    size_t bonus_count;
    map_t map;
    config_t config;
    event_queue_t queue;
    pthread_mutex_t mutex;
    uint64_t current_tick;
    statistics_t stats[MAX_PLAYERS];

    bool server_running; // used to signal threads to exit when server is shutting down
} server_state_t;

int serve_main(int argc, char *argv[]);
void main_cleanup(server_state_t *state);    
void close_all_client_fds(server_state_t *state);
void signal_handler(int signum);

void broadcast_leave(server_state_t *state, int sender_idx);
void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello);
void broadcast_set_ready(server_state_t *state, int sender_idx);
void remove_client_quietly(server_state_t *state, int id);
void broadcast_statistics(server_state_t *state);

int find_free_slot(client_t clients[]);
int add_client(server_state_t *state, int fd, struct sockaddr_in *client_addr);
void remove_client(server_state_t *state, int id);
int player_name_in_use(const server_state_t *state, const char *name);
