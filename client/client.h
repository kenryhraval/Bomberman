#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "shared/protocol.h"
#include "client_protocol.h"

// skatu pārslēgšanai
typedef enum {
    VIEW_LOBBY = 0,
    VIEW_MAP_SELECT = 1
} client_view_t;


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
    client_map_choice_t map_choices[MAX_MAP_CHOICES];
    uint8_t map_choice_count;
    uint8_t selected_map_id;
    client_view_t view;
} client_state_t;

int client_connect(client_state_t *state, const char *ip, int port);
int client_handshake(client_state_t *state, const char *player_name);
int client_send_leave(const client_state_t *state);
void client_close(client_state_t *state);
int client_poll_network(client_state_t *state);

#endif
