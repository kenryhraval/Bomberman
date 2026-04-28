#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "server.h"
#include "handle_client.h"
#include "configs.h"
#include "broadcasts.h"

#include <time.h>
#include <arpa/inet.h>

void *game_loop(void *arg)
{
    server_state_t *state = arg;
    struct timespec last_tick, now;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    while (state->server_running)
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

    // update bombs
    size_t i = 0;
    while (i < state->bomb_count)
    {
        if (state->bombs[i].timer_ticks > 0)
            state->bombs[i].timer_ticks--;

        if (state->bombs[i].timer_ticks == 0)
        {
            explode(state, (int)i);

            // do not increment i here
            // explode() removes the bomb and may swap another bomb into this index
        }
        else
        {
            i++;
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
            bomb_t *source = &state->explosions[i].source;
            state->explosions[i].active = false;
            broadcast_explosion_end(state, source->row, source->col, source->radius);

            free(state->explosions[i].footprint);
            state->explosions[i].footprint = NULL;
            state->explosions[i].footprint_size = 0;
        }
    }

    // check if one player has no proprietae client
    bool all_players_have_proprietary_client = true;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        if (!is_proprietary_client_id(&state->clients[i]))
        {
            all_players_have_proprietary_client = false;
            break;
        }
    }

    if (!all_players_have_proprietary_client)
        return;

    // check if draw timer hit timeout
    if (state->current_tick >= GAME_DRAW_TICK_TIMEOUT)
    {
        state->current_tick = 0;
        int alive_count = check_win_condition(state);
        if (alive_count > 1)
        {
            broadcast_winner(state, SERVER); // draw

            broadcast_statistics(state);
            // send game end status
            broadcast_set_game_status(state, GAME_END);
            state->game_status = GAME_END;
        }

        return;
    }

    // each 100 ticks, send timer sync message to clients to keep their timers accurate
    if (state->current_tick % 100 == 0)
    {
        broadcast_timer_sync(state);
    }
}

void maybe_spawn_bonus(server_state_t *state, uint16_t row, uint16_t col)
{
    // check if there is room for another bonus
    if (state->bonus_count >= MAX_BONUSES)
        return;

    // check if bonus should spawn
    double r = (double)(rand() % 100) / 100.0;
    if (r >= BLOCK_DESTROY_BONUS_CHANCE)
        return;

    // create new bonus at the end of the packed bonus array
    bonus_t *bonus = &state->bonuses[state->bonus_count];

    bonus->row = row;
    bonus->col = col;

    int bonus_type = rand() % 4; // 4 bonus types
    switch (bonus_type)
    {
    case 0:
        bonus->type = BONUS_BOMB_COUNT;
        break;
    case 1:
        bonus->type = BONUS_RADIUS;
        break;
    case 2:
        bonus->type = BONUS_SPEED;
        break;
    case 3:
        bonus->type = BONUS_TIMER;
        break;
    }

    state->bonus_count++;

    uint16_t bonus_cell = make_cell_index(row, col, state->map.cols);
    state->map.cells[bonus_cell] = bonus->type;

    broadcast_bonus_available(state, bonus->type, bonus_cell);
}

void calculate_explosion_footprint(const server_state_t *state, explosion_t *expl)
{
    uint16_t center_cell = make_cell_index((uint16_t)expl->source.row, (uint16_t)expl->source.col, state->map.cols);

    // explosion footprint always includes the center cell
    int idx = 0;
    expl->footprint[idx++] = center_cell;

    // bomb propagation directions
    int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    // propagate explosion in 4 directions to calculate footprint
    for (int d = 0; d < 4; d++)
    {
        // propagate explosion in this direction until we reach max radius or hit a block
        for (int r = 1; r <= expl->source.radius; r++)
        {
            // calculate explosion cell coordinates
            int row = (int)expl->source.row + dirs[d][0] * r;
            int col = (int)expl->source.col + dirs[d][1] * r;

            // check map bounds
            if (row < 0 || row >= state->map.rows ||
                col < 0 || col >= state->map.cols)
                break;

            uint16_t cell_idx = make_cell_index((uint16_t)row, (uint16_t)col, state->map.cols);

            // get cell type
            uint8_t cell = state->map.cells[cell_idx];

            if (cell == HARD_BLOCK)
                break;

            expl->footprint[idx++] = cell_idx;

            // if we hit a soft block or bomb, explosion stops but it still affects that cell
            if (cell == SOFT_BLOCK || cell == BOMB)
                break;
        }
    }

    expl->footprint_size = idx;
}

void explode(server_state_t *state, int bomb_idx)
{
    if (bomb_idx < 0 || bomb_idx >= (int)state->bomb_count)
        return;

    // make a copy of the bomb before we remove it from the state
    bomb_t bomb = state->bombs[bomb_idx];

    if (bomb_idx < (int)state->bomb_count - 1)
        state->bombs[bomb_idx] = state->bombs[state->bomb_count - 1];

    state->bomb_count--;

    // mark bomb cell as empty
    state->map.cells[make_cell_index(bomb.row, bomb.col, state->map.cols)] = EMPTY;
    state->clients[bomb.owner_id].player.bomb_count++;

    // broadcast EXPLOSION_START to all clients
    broadcast_explosion_start(state, bomb.row, bomb.col, bomb.radius);

    // add explosion to the explosion state array
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->explosions[i].active)
        {
            state->explosions[i].active = true;
            state->explosions[i].source = bomb; // copy bomb info to explosion source

            int max_cells_in_radius = bomb.radius * 4 + 1; // max cells in explosion footprint (cross-shaped)
            state->explosions[i].footprint = malloc(max_cells_in_radius * sizeof(uint16_t));
            if (state->explosions[i].footprint == NULL)
            {
                perror("Failed to allocate memory for explosion footprint");
                exit(EXIT_FAILURE);
            }

            calculate_explosion_footprint(state, &state->explosions[i]);

            state->explosions[i].duration_ticks =
                state->clients[bomb.owner_id].player.bomb_explosion_duration_ticks;

            break;
        }
    }

    // check inside the bomb
    check_player_deaths(state, bomb.row, bomb.col, bomb.owner_id);

    // bomb propagation directions
    int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    // propagate bomb in 4 directions
    for (int d = 0; d < 4; d++)
    {
        for (int r = 1; r <= bomb.radius; r++)
        {
            // calculate explosion cell coordinates
            int row = (int)bomb.row + dirs[d][0] * r;
            int col = (int)bomb.col + dirs[d][1] * r;

            // check map bounds
            if (row < 0 || row >= state->map.rows ||
                col < 0 || col >= state->map.cols)
                break;

            // convert to cell index and check cell type
            uint16_t idx = make_cell_index(row, col, state->map.cols);
            uint8_t cell = state->map.cells[idx];

            // check cell type
            if (cell == HARD_BLOCK)
            {
                break;
            }
            else if (cell == SOFT_BLOCK)
            {
                // destroys soft block
                state->map.cells[idx] = EMPTY;

                // update statistics and broadcast
                state->stats[bomb.owner_id].blocks_destroyed++;
                broadcast_block_destroyed(state, idx);

                // check if bonus should spawn
                maybe_spawn_bonus(state, row, col);

                break; // bomb stops at soft block
            }
            else if (cell == BOMB)
            {
                for (size_t i = 0; i < state->bomb_count; i++)
                {
                    if (state->bombs[i].row == row && state->bombs[i].col == col)
                    {
                        explode(state, (int)i); // use recursion
                        break;
                    }
                }
                break; // bomb stops at another bomb
            }
            else
            {
                // if empty cell, check if player is there and kill them
                check_player_deaths(state, row, col, bomb.owner_id);
            }
        }
    }
    check_win_condition(state);
}

void check_player_deaths(server_state_t *state, uint16_t row, uint16_t col, uint8_t killer_id)
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

            // update statistics
            if (p->id != killer_id)
                state->stats[killer_id].kills++;

            // broadcast death
            broadcast_death(state, p->id);
        }
    }
}

int check_win_condition(server_state_t *state)
{
    // already have a winner or game not running
    if (state->game_status != GAME_RUNNING)
        return -1;

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
        broadcast_statistics(state);
        broadcast_set_game_status(state, GAME_END);
        state->game_status = GAME_END;
    }
    else if (alive_count == 0)
    {
        broadcast_winner(state, SERVER); // draw
        broadcast_statistics(state);
        broadcast_set_game_status(state, GAME_END);
        state->game_status = GAME_END;
    }

    return alive_count;
}

void handle_bomb(server_state_t *state, const event_t *ev)
{
    player_t *p = &state->clients[ev->player_id].player;

    if (!p->alive)
        return;
    if (p->bomb_count == 0)
        return;

    uint16_t row = ev->data.cell / state->map.cols;
    uint16_t col = ev->data.cell % state->map.cols;

    // player must be standing on the cell where they want to place the bomb
    if (p->row != row || p->col != col)
        return;

    uint16_t bomb_cell = make_cell_index(row, col, state->map.cols);

    // check if cell is already occupied by a bomb
    if (state->map.cells[bomb_cell] == BOMB)
        return;

    // check if there is room for another bomb
    if (state->bomb_count >= MAX_BOMBS)
        return;

    // init bomb at the end of the packed bomb array
    bomb_t *bomb = &state->bombs[state->bomb_count];

    bomb->owner_id = ev->player_id;
    bomb->row = row;
    bomb->col = col;
    bomb->radius = p->bomb_radius;
    bomb->timer_ticks = p->bomb_timer_ticks;

    state->bomb_count++;

    // mark cell on map
    state->map.cells[bomb_cell] = BOMB;

    // reduce player's bomb count
    p->bomb_count--;

    // broadcast BOMB to all clients
    broadcast_bomb(state, ev->player_id, ev->data.cell);
}

void handle_move(server_state_t *state, const event_t *ev)
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
    if (new_row >= state->map.rows || new_col >= state->map.cols)
        return;

    // check cell type
    uint8_t cell = state->map.cells[make_cell_index(new_row, new_col, state->map.cols)];
    if (cell == HARD_BLOCK || cell == SOFT_BLOCK || cell == BOMB)
        return;

    // check if another player is on the target cell
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        if (i == ev->player_id)
            continue;
        // skip dead players
        if (!state->clients[i].player.alive)
            continue;
        if (state->clients[i].player.row == new_row &&
            state->clients[i].player.col == new_col)
            return;
    }

    // check if player enters an active explosion area
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->explosions[i].active)
            continue;

        // check explosion footprint for target cell
        for (size_t j = 0; j < state->explosions[i].footprint_size; j++)
        {
            if (state->explosions[i].footprint[j] == make_cell_index(new_row, new_col, state->map.cols))
            {
                // player moves into explosion and dies
                p->alive = false;

                uint8_t killer_id = state->explosions[i].source.owner_id;
                if (p->id != killer_id)
                    state->stats[killer_id].kills++;

                broadcast_death(state, p->id);
                check_win_condition(state);
                return;
            }
        }
    }

    // update player position
    p->row = new_row;
    p->col = new_col;
    p->last_move_tick = state->current_tick;

    // broadcast MOVED
    broadcast_moved(state, ev->player_id, make_cell_index(new_row, new_col, state->map.cols));

    // check if player moved into a bonus
    for (size_t i = 0; i < state->bonus_count; i++)
    {
        if (state->bonuses[i].row == new_row &&
            state->bonuses[i].col == new_col)
        {
            // apply bonus effect
            switch (state->bonuses[i].type)
            {
            case BONUS_SPEED:
                if (p->speed < MAX_PLAYER_SPEED)
                    p->speed++;
                break;
            case BONUS_RADIUS:
                if (p->bomb_radius < MAX_BOMB_RADIUS)
                    p->bomb_radius++;
                break;
            case BONUS_TIMER:
                if (p->bomb_explosion_duration_ticks < MAX_BOMB_EXPLOSION_DURATION_TICKS)
                    p->bomb_explosion_duration_ticks += BOMB_EXPLOSION_BONUS_INCREASE_TICKS;
                break;
            case BONUS_BOMB_COUNT:
                if (p->bomb_count < MAX_BOMBS_PER_PLAYER)
                    p->bomb_count++;
                break;
            default:
                break;
            }

            uint16_t bonus_cell = make_cell_index(new_row, new_col, state->map.cols);

            // remove bonus from map
            state->map.cells[bonus_cell] = EMPTY;

            // update statistics
            state->stats[p->id].bonuses_collected++;

            // broadcast bonus collected
            broadcast_bonus_collected(state, p->id, bonus_cell);

            // remove bonus from packed array by swapping with the last bonus
            if (i < state->bonus_count - 1)
                state->bonuses[i] = state->bonuses[state->bonus_count - 1];

            state->bonus_count--;

            break;
        }
    }
}
