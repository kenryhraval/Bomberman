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


int send_header(int fd, uint8_t type, uint8_t sender_id, uint8_t target_id)
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


int send_leave(int fd, uint8_t sender_id, uint8_t target_id) 
{
    if (send_header(fd, MSG_LEAVE, sender_id, target_id) < 0) {
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

