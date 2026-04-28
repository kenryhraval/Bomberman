#include "client_protocol.h"
#include "shared/protocol.h"

#include <arpa/inet.h>

int recv_welcome(int fd, msg_generic_t *header, msg_welcome_t *msg)
{
    if (read_exact(fd, header, sizeof(*header)) < 0) {
        return -1;
    }
    if (header->msg_type != MSG_WELCOME) {
        return -1;
    }
    if (read_exact(fd, msg, sizeof(*msg)) < 0) {
        return -1;
    }
    return 0;
}


int send_set_ready(int fd, uint8_t sender_id, uint8_t target_id) 
{

    if (send_header(fd, MSG_SET_READY, sender_id, target_id) < 0) {
        return -1;
    }
    return 0;
}


int send_move_attempt(int fd, uint8_t sender_id, uint8_t direction)
{
    msg_move_attempt_t msg = {
        .direction = direction
    };

    if (send_header(fd, MSG_MOVE_ATTEMPT, sender_id, SERVER) < 0)
        return -1;

    if (write_exact(fd, &msg, sizeof(msg)) < 0)
        return -1;

    return 0;
}


int send_bomb_attempt(int fd, uint8_t sender_id, const uint16_t row, const uint16_t col, const uint16_t map_cols)
{
    msg_bomb_attempt_t msg = {
        .cell = htons(make_cell_index(row, col, map_cols))
    };

    if (send_header(fd, MSG_BOMB_ATTEMPT, sender_id, SERVER) < 0)
        return -1;

    if (write_exact(fd, &msg, sizeof(msg)) < 0)
        return -1;

    return 0;
}


int send_map_selected(int fd, uint8_t sender_id, uint8_t target_id, uint8_t map_id)
{
    msg_map_selected_t payload = {
        .map_id = map_id,
    };

    if (send_header(fd, MSG_MAP_SELECTED, sender_id, target_id) < 0)
        return -1;

    if (write_exact(fd, &payload, sizeof(payload)) < 0)
        return -1;

    return 0;
}

