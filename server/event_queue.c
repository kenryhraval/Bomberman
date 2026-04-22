#include "event_queue.h"

#include <string.h>

void init_event_queue(event_queue_t *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    memset(queue->events, 0, sizeof(queue->events));
}

int enqueue_event(event_queue_t *queue, event_t *event)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->count < EVENT_QUEUE_SIZE)
    {
        queue->events[queue->tail] = *event;
        queue->tail = (queue->tail + 1) % EVENT_QUEUE_SIZE;
        queue->count++;

        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }
    else
    {
        // queue full, drop event
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
}

int dequeue_event(event_queue_t *queue, event_t *event)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->count > 0)
    {
        *event = queue->events[queue->head];
        queue->head = (queue->head + 1) % EVENT_QUEUE_SIZE;
        queue->count--;
        
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }
    else
    {
        // queue empty
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
}