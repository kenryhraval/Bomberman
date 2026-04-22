#pragma once
#include <stdint.h>
#include <pthread.h>

#define EVENT_QUEUE_SIZE 64

typedef enum
{
    EVENT_MOVE,
    EVENT_BOMB,
} event_type_t;

typedef struct
{
    event_type_t type;
    uint8_t player_id;
    // union cos bomb and move attempt have different data
    union
    {
        uint8_t direction; // EVENT_MOVE — DIR_UP/DOWN/LEFT/RIGHT
        uint16_t cell;     // EVENT_BOMB
    } data;
} event_t;

typedef struct
{
    event_t events[EVENT_QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t mutex;
} event_queue_t;

int enqueue_event(event_queue_t *queue, event_t *event);
int dequeue_event(event_queue_t *queue, event_t *event);
void init_event_queue(event_queue_t *queue);