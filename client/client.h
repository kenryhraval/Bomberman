#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "shared/protocol.h"
#include "client_protocol.h"

typedef struct {
    int fd;
    uint8_t my_id;
    game_status_t game_status;
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t player_count;
    player_t players[MAX_PLAYERS];
    map_t map;
    map_t overlay_map;
    uint8_t winner_id;
    statistics_t stats;
} client_state_t;

#define EXPLOSION_CELL 'X'

int client_connect(client_state_t *state, const char *ip, int port);
int client_handshake(client_state_t *state, const char *player_name);
int client_send_leave(client_state_t *state);
void client_close(client_state_t *state);
int client_poll_network(client_state_t *state);

#endif
