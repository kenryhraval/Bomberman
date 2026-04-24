#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "../shared/protocol.h"

typedef struct {
    uint8_t rows;
    uint8_t cols;
    uint8_t cells[MAX_MAP_HEIGHT * MAX_MAP_WIDTH];
} map_t;

typedef struct {
    int fd;
    uint8_t my_id;
    game_status_t game_status;
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t player_count;
    player_t players[MAX_PLAYERS];
    map_t map;
    uint8_t winner_id;
} client_state_t;

int client_connect(client_state_t *client, const char *ip, int port);
int client_handshake(client_state_t *client, const char *player_name);
int client_send_leave(client_state_t *client);
void client_close(client_state_t *client);
int client_poll_network(client_state_t *client);

#endif

