#include "server_protocol.h"
#include "shared/protocol.h"

#include <arpa/inet.h>


int send_welcome(int fd, uint8_t sender_id, uint8_t target_id, const msg_welcome_t *msg) 
{
    if (send_header(fd, MSG_WELCOME, sender_id, target_id) < 0) {
        return -1;
    }
    if (write_exact(fd, msg, sizeof(*msg)) < 0) {
        return -1;
    }
    return 0;
}


int send_disconnect(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_DISCONNECT, sender_id, target_id) < 0) {
        return -1;
    }
    return 0;
}


int send_map(int fd, uint8_t sender_id, uint8_t target_id,
             const msg_map_t *msg, const uint8_t *cells)
{
    uint16_t cell_count = msg->height * msg->width;

    if (send_header(fd, MSG_MAP, sender_id, target_id) < 0) return -1;
    if (write_exact(fd, msg, sizeof(*msg)) < 0) return -1;
    if (write_exact(fd, cells, cell_count) < 0) return -1;

    return 0;
}


int send_moved(int fd, uint8_t sender_id, uint8_t target_id, const msg_moved_t *msg)
{
    if (send_header(fd, MSG_MOVED, sender_id, target_id) < 0)
        return -1;

    if (write_exact(fd, msg, sizeof(*msg)) < 0)
        return -1;

    return 0;
}


int send_map_choices(int fd, uint8_t sender_id, uint8_t target_id, const msg_map_choices_t *msg)
{
    if (send_header(fd, MSG_MAP_CHOICES, sender_id, target_id) < 0)
        return -1;

    if (write_exact(fd, msg, sizeof(*msg)) < 0)
        return -1;

    return 0;
}
