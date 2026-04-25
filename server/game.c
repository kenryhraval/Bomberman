#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "server.h"
#include "handle_client.h"

#include <time.h>
#include <arpa/inet.h>

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
        else if (ev.type == EVENT_BOMB)
            handle_bomb(state, &ev);
    }

    // 2. update game state (move bombs, check win condition, etc)

    // update bombs explosions
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->bombs[i].active)
            continue;
        state->bombs[i].timer_ticks--;

        if (state->bombs[i].timer_ticks == 0)
        {
            explode(state, i);
        }
    }

    // check for explosion end
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->explosions[i].active)
            continue;
        state->explosions[i].duration_ticks--;

        if (state->explosions[i].duration_ticks == 0)
        {
            state->explosions[i].active = false;
            broadcast_explosion_end(state,
                                    state->explosions[i].row,
                                    state->explosions[i].col,
                                    state->explosions[i].radius);
        }
    }
}

void broadcast_block_destroyed(server_state_t *state, uint16_t cell)
{
    msg_generic_t header = {
        .msg_type = MSG_BLOCK_DESTROYED,
        .sender_id = SERVER,
        .target_id = BROADCAST,
    };

    msg_block_destroyed_t payload = {
        .cell = htons(cell),
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void broadcast_explosion_end(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius)
{
    msg_generic_t header = {
        .msg_type = MSG_EXPLOSION_END,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    msg_explosion_end_t payload = {
        .radius = radius,
        .cell = htons(make_cell_index(row, col, state->map->cols))};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void maybe_spawn_bonus(server_state_t *state, uint16_t row, uint16_t col)
{
}

void explode(server_state_t *state, int bomb_idx)
{
    bomb_t *bomb = &state->bombs[bomb_idx];
    bomb->active = false;

    // mark bomb cell as empty
    state->map->cells[make_cell_index(bomb->row, bomb->col, state->map->cols)] = EMPTY;
    state->clients[bomb->owner_id].player.bomb_count++;

    // broadcast EXPLOSION_START to all clients
    broadcast_explosion_start(state, bomb->row, bomb->col, bomb->radius);

    // add explosion to the explosion state array
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->explosions[i].active)
        {
            state->explosions[i].active = true;
            state->explosions[i].row = bomb->row;
            state->explosions[i].col = bomb->col;
            state->explosions[i].radius = bomb->radius;
            state->explosions[i].duration_ticks = state->config->explosion_duration_ticks;
            break;
        }
    }

    // check inside the bomb
    check_player_deaths(state, bomb->row, bomb->col);

    // bomb propagation directions
    int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    // propagate bomb in 4 directions
    for (int d = 0; d < 4; d++)
    {
        for (int r = 1; r <= bomb->radius; r++)
        {
            // calculate explosion cell coordinates
            int row = (int)bomb->row + dirs[d][0] * r;
            int col = (int)bomb->col + dirs[d][1] * r;

            // check map bounds
            if (row < 0 || row >= state->map->rows ||
                col < 0 || col >= state->map->cols)
                break;

            // convert to cell index and check cell type
            uint16_t idx = make_cell_index(row, col, state->map->cols);
            uint8_t cell = state->map->cells[idx];

            // check cell type
            if (cell == HARD_BLOCK)
            {
                break;
            }
            else if (cell == SOFT_BLOCK)
            {
                // destroyes soft block
                state->map->cells[idx] = EMPTY;
                broadcast_block_destroyed(state, make_cell_index(row, col, state->map->cols));

                // check if bonus should spawn
                maybe_spawn_bonus(state, row, col);

                break; // bomb stops at soft block
            }
            else if (cell == BOMB)
            {
                // chain reaction with another bomb
                for (int i = 0; i < MAX_BOMBS; i++)
                {
                    if (state->bombs[i].active &&
                        state->bombs[i].row == row &&
                        state->bombs[i].col == col)
                    {
                        explode(state, i); // use recursion
                        break;
                    }
                }
                break; // bomb stops at another bomb
            }
            else
            {
                // if empty cell, check if player is there and kill them
                check_player_deaths(state, row, col);
            }
        }
    }
    check_win_condition(state);
}

void broadcast_explosion_start(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius)
{
    msg_generic_t header = {
        .msg_type = MSG_EXPLOSION_START,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    msg_explosion_start_t payload = {
        .cell = htons(make_cell_index(row, col, state->map->cols)),
        .radius = radius};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void check_player_deaths(server_state_t *state, uint16_t row, uint16_t col)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        player_t *p = &state->clients[i].player;
        if (!p->alive)
            continue;

        if (p->row == row && p->col == col)
        {
            p->alive = false;
            broadcast_death(state, p->id);
        }
    }
}

void broadcast_winner(server_state_t *state, uint8_t winner_id)
{
    msg_generic_t header = {
        .msg_type = MSG_WINNER,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    msg_winner_t payload = {
        .winner_id = winner_id,
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void check_win_condition(server_state_t *state)
{
    // already have a winner or game not running
    if (state->game_status != GAME_RUNNING)
        return;

    int alive_count = 0;
    uint8_t last_alive_id = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        // count alive players and remember the last alive players id
        if (state->clients[i].player.alive)
        {
            alive_count++;
            last_alive_id = state->clients[i].player.id;
        }
    }

    // we have a winner if only 1 player is alive, if 0 players alive then its a draw
    if (alive_count == 1)
    {
        broadcast_winner(state, last_alive_id);
        broadcast_set_game_status(state, GAME_END);
        state->game_status = GAME_END;
    }
    else if (alive_count == 0)
    {
        // draw
        broadcast_winner(state, 255); // TODO: How to indicate draw?
        broadcast_set_game_status(state, GAME_END);
        state->game_status = GAME_END;
    }
}

void broadcast_death(server_state_t *state, uint8_t player_id)
{
    msg_generic_t header = {
        .msg_type = MSG_DEATH,
        .sender_id = player_id,
        .target_id = BROADCAST};

    msg_death_t payload = {
        .player_id = player_id,
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void handle_bomb(server_state_t *state, event_t *ev)
{
    player_t *p = &state->clients[ev->player_id].player;

    if (!p->alive)
        return;
    if (p->bomb_count == 0)
        return; // no bombs

    uint16_t row = ev->data.cell / state->map->cols;
    uint16_t col = ev->data.cell % state->map->cols;

    // player must be standing on the cell where they want to place the bomb
    if (p->row != row || p->col != col)
        return;

    // check if cell is already occupied by a bomb
    uint8_t cell = state->map->cells[make_cell_index(row, col, state->map->cols)];
    if (cell == BOMB)
        return;

    // find free bomb slot
    int slot = -1;
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->bombs[i].active)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return; // no free bomb slots

    // init bomb
    state->bombs[slot].active = true;
    state->bombs[slot].owner_id = ev->player_id;
    state->bombs[slot].row = row;
    state->bombs[slot].col = col;
    state->bombs[slot].radius = p->bomb_radius;
    state->bombs[slot].timer_ticks = p->bomb_timer_ticks;

    // mark cell on map
    state->map->cells[make_cell_index(row, col, state->map->cols)] = BOMB;

    // reduce players bomb count
    p->bomb_count--;

    // broadcast BOMB to all clients
    broadcast_bomb(state, ev->player_id, ev->data.cell);
}

void handle_move(server_state_t *state, event_t *ev)
{
    client_t *client = &state->clients[ev->player_id];
    player_t *p = &client->player;

    if (!p->alive)
        return;
    if (p->speed == 0)
        return;

    // check if enough time has passed since last move based on player's speed
    uint64_t ticks_per_move = (TICKS_PER_SECOND + p->speed - 1) / p->speed;
    if (p->last_move_tick != 0 &&
        state->current_tick - p->last_move_tick < ticks_per_move)
        return; // ignore move if player is trying to move too fast

    uint16_t new_row = p->row;
    uint16_t new_col = p->col;

    // calc new position based on direction
    switch (ev->data.direction)
    {
    case DIR_UP:
        if (new_row == 0)
            return;
        new_row--;
        break;
    case DIR_DOWN:
        new_row++;
        break;
    case DIR_LEFT:
        if (new_col == 0)
            return;
        new_col--;
        break;
    case DIR_RIGHT:
        new_col++;
        break;
    default:
        return;
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
    p->last_move_tick = state->current_tick;

    // broadcast MOVED
    broadcast_move(state, ev->player_id, make_cell_index(new_row, new_col, state->map->cols));
}

void broadcast_move(server_state_t *state, uint8_t player_id, uint16_t cell)
{
    msg_generic_t header = {MSG_MOVED, player_id, BROADCAST};
    msg_moved_t payload = {
        .player_id = player_id,
        .cell = htons(cell)};
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}

void broadcast_bomb(server_state_t *state, uint8_t player_id, uint16_t cell)
{
    msg_generic_t header = {MSG_BOMB, player_id, BROADCAST};
    msg_bomb_t payload = {
        .player_id = player_id,
        .cell = htons(cell)};
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
    }
}