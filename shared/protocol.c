#include "protocol.h"

#include <unistd.h>
#include <errno.h>


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


int send_hello(int fd, uint8_t sender_id, uint8_t target_id, const msg_hello_t *msg)
{
    msg_generic_t header = {
        .msg_type = MSG_HELLO,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
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
    msg_generic_t header = {
        .msg_type = MSG_WELCOME,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
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
    msg_generic_t header = {
        .msg_type = MSG_LEAVE,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    return 0;
}


int send_set_ready(int fd, uint8_t sender_id, uint8_t target_id) {
    msg_generic_t header = {
        .msg_type = MSG_SET_READY,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    return 0;
}

int send_disconnect(int fd, uint8_t sender_id, uint8_t target_id) {
    msg_generic_t header = {
        .msg_type = MSG_DISCONNECT,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    return 0;
}

int send_pong(int fd, uint8_t sender_id, uint8_t target_id) {
    msg_generic_t header = {
        .msg_type = MSG_PONG,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    return 0;
}

int send_ping(int fd, uint8_t sender_id, uint8_t target_id) {
    msg_generic_t header = {
        .msg_type = MSG_PING,
        .sender_id = sender_id,
        .target_id = target_id
    };
    if (write_exact(fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    return 0;
}


int send_map(int fd, uint8_t sender_id, uint8_t target_id,
             const msg_map_t *msg, const uint8_t *cells)
{
    msg_generic_t header = {
        .msg_type = MSG_MAP,
        .sender_id = sender_id,
        .target_id = target_id
    };

    uint16_t cell_count = msg->height * msg->width;

    if (write_exact(fd, &header, sizeof(header)) < 0) return -1;
    if (write_exact(fd, msg, sizeof(*msg)) < 0) return -1;
    if (write_exact(fd, cells, cell_count) < 0) return -1;

    return 0;
}


int send_move_attempt(int fd, uint8_t sender_id, uint8_t target_id,
                      const msg_move_attempt_t *msg)
{
    msg_generic_t header = {
        .msg_type = MSG_MOVE_ATTEMPT,
        .sender_id = sender_id,
        .target_id = target_id
    };

    if (write_exact(fd, &header, sizeof(header)) < 0)
        return -1;

    if (write_exact(fd, msg, sizeof(*msg)) < 0)
        return -1;

    return 0;
}


int send_moved(int fd, uint8_t sender_id, uint8_t target_id, const msg_moved_t *msg)
{
    msg_generic_t header = {
        .msg_type = MSG_MOVED,
        .sender_id = sender_id,
        .target_id = target_id
    };

    if (write_exact(fd, &header, sizeof(header)) < 0)
        return -1;

    if (write_exact(fd, msg, sizeof(*msg)) < 0)
        return -1;

    return 0;
}
