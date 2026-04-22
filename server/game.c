#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "server.h"

#include <time.h>

void *game_loop(void *arg)
{
    server_state_t *state = arg;
    struct timespec last_tick, now;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    while (true)
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - last_tick.tv_sec) * 1000 +
                          (now.tv_nsec - last_tick.tv_nsec) / 1000000;

        long remaining = (1000 / TICK_RATE) - elapsed_ms;
        if (remaining > 0)
        {
            struct timespec sleep_time = {
                .tv_sec = remaining / 1000,
                .tv_nsec = (remaining % 1000) * 1000000L,
            };
            nanosleep(&sleep_time, NULL);
        }

        // update last tick before game_tick
        // to include time spent in game_tick in the tick rate calculation
        clock_gettime(CLOCK_MONOTONIC, &last_tick);

        pthread_mutex_lock(&state->mutex);
        if (state->game_status == GAME_RUNNING)
        {
            game_tick(state);
        }
        pthread_mutex_unlock(&state->mutex);
    }

    return NULL;
}

void game_tick(void *arg)
{
    server_state_t *state = arg;
    state->current_tick++;

    // 1. process all event from clients
    event_t ev;
    while (dequeue_event(&state->queue, &ev) == 0)
    {
        if (ev.type == EVENT_MOVE)
            handle_move(state, &ev);
        else if (ev.type == EVENT_BOMB){}
            // handle_bomb(state, &ev);
    }

    // 2. update game state (move bombs, check win condition, etc)
}

void handle_move(server_state_t *state, event_t *ev)
{
    client_t *client = &state->clients[ev->player_id];
    player_t *p = &client->player;

    if (!p->alive)
        return;

    // check if enough time has passed since last move based on player's speed
    uint64_t ticks_per_move = TICKS_PER_SECOND / p->speed;
    if (state->current_tick - p->last_move_tick < ticks_per_move)
        return; // ignore move if player is trying to move too fast

    uint16_t new_row = p->row;
    uint16_t new_col = p->col;

    // calc new position based on direction
    switch (ev->data.direction)
    {
    case DIR_UP:
        new_row--;
        break;
    case DIR_DOWN:
        new_row++;
        break;
    case DIR_LEFT:
        new_col--;
        break;
    case DIR_RIGHT:
        new_col++;
        break;
    }

    // check map bounds
    if (new_row >= state->map->rows || new_col >= state->map->cols)
        return;

    // check cell type
    uint8_t cell = state->map->cells[make_cell_index(new_row, new_col, state->map->cols)];
    if (cell == HARD_BLOCK || cell == SOFT_BLOCK || cell == BOMB)
        return;

    // check if another player is on the target cell
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        if (i == ev->player_id)
            continue;
        if (state->clients[i].player.row == new_row &&
            state->clients[i].player.col == new_col)
            return;
    }

    // update player position
    p->row = new_row;
    p->col = new_col;

    // broadcast MOVED
    broadcast_move(state, ev->player_id, make_cell_index(new_row, new_col, state->map->cols));
}

void broadcast_move(server_state_t *state, uint8_t player_id, uint16_t cell)
{
    msg_generic_t header = {MSG_MOVED, player_id, BROADCAST};
    msg_moved_t payload = {
        .player_id = player_id,
        .cell = cell};
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}