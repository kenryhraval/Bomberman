#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "../shared/protocol.h"

typedef struct {
    int fd;
    uint8_t my_id;
    char name[MAX_NAME_LEN + 1];
    bool ready;
    msg_welcome_t welcome;
} client_state_t;

int client_connect(client_state_t *client, const char *ip, int port);
int client_handshake(client_state_t *client);
int client_send_leave(client_state_t *client);
void client_close(client_state_t *client);
int client_poll_network(client_state_t *client);


#endif

