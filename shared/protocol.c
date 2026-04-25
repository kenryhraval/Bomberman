#include "protocol.h"

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>


int read_exact(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *p = (char *)buf;

    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);

        if (n == 0) {
            return -1;  // connection closed
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue; // interrupted by signal, retry
            }
            return -1; // real error
        }

        total += (size_t)n;
    }

    return 0;
}


int write_exact(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *p = (const char *)buf;

    while (total < count) {
        ssize_t n = write(fd, p + total, count - total);

        if (n < 0) {
            if (errno == EINTR) {
                continue; // interrupted by signal, retry
            }
            return -1; // real error
        }

        total += (size_t)n;
    }

    return 0;
}


static int send_header(int fd, uint8_t type, uint8_t sender_id, uint8_t target_id)
{
    msg_generic_t header = {
        .msg_type = type,
        .sender_id = sender_id,
        .target_id = target_id
    };

    return write_exact(fd, &header, sizeof(header));
}


int send_hello(int fd, uint8_t sender_id, uint8_t target_id, const msg_hello_t *msg)
{
    if (send_header(fd, MSG_HELLO, sender_id, target_id) < 0) {
        return -1;
    }

    if (write_exact(fd, msg, sizeof(*msg)) < 0) {
        return -1;
    }
    return 0;
}


int recv_hello(int fd, msg_generic_t *header, msg_hello_t *msg)
{
    if (read_exact(fd, header, sizeof(*header)) < 0) {
        return -1;
    }
    if (header->msg_type != MSG_HELLO) {
        return -1;
    }
    if (read_exact(fd, msg, sizeof(*msg)) < 0) {
        return -1;
    }
    return 0;
}


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


int send_leave(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_LEAVE, sender_id, target_id) < 0) {
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

int send_disconnect(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_DISCONNECT, sender_id, target_id) < 0) {
        return -1;
    }
    return 0;
}


int send_pong(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_PONG, sender_id, target_id) < 0) {
        return -1;
    }
    return 0;
}
   

int send_ping(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_PING, sender_id, target_id) < 0) {
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


int send_moved(int fd, uint8_t sender_id, uint8_t target_id, const msg_moved_t *msg)
{
    if (send_header(fd, MSG_MOVED, sender_id, target_id) < 0)
        return -1;

    if (write_exact(fd, msg, sizeof(*msg)) < 0)
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

