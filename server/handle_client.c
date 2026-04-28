#include "handle_client.h"
#include "event_queue.h"

void watchdog_handler(union sigval sv)
{
    watchdog_param_t *param = sv.sival_ptr;

    pthread_mutex_lock(&param->state->mutex);

    // if client is still waiting for pong
    if (param->client->waiting_for_pong)
    {
        printf("Client %d timed out, removing...\n", param->client->player.id);
        // ping was sent 30s ago, no pong received -> remove
        if (param->client->connected) // guard against double-remove
            remove_client(param->state, param->client->player.id);
        pthread_mutex_unlock(&param->state->mutex);
        return;
    }
    else
    {
        printf("Client %d is alive, feeding watchdog\n", param->client->player.id);
    }

    param->client->waiting_for_pong = true;
    pthread_mutex_unlock(&param->state->mutex); // unlock before send

    // send ping to check if client is alive
    send_ping(param->client->fd, SERVER, param->client->player.id);

    // feed for another 30 seconds
    feed_player_watchdog(param->timer_id);
}

void start_player_watchdog(timer_t *timer_id, client_t *client, server_state_t *state, watchdog_param_t **out_param)
{
    watchdog_param_t *param = malloc(sizeof(watchdog_param_t));
    param->timer_id = timer_id;
    param->client = client;
    param->state = state;

    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = watchdog_handler,
        .sigev_value.sival_ptr = param,
    };
    if (timer_create(CLOCK_MONOTONIC, &sev, timer_id) < 0)
    {
        perror("timer_create");
        free(param);
        return;
    }

    *out_param = param;

    struct itimerspec ts = {
        .it_value = {.tv_sec = CLIENT_TIMEOUT_SECONDS},
        .it_interval = {.tv_sec = 0},
    };

    if (timer_settime(*timer_id, 0, &ts, NULL) < 0)
    {
        perror("timer_settime");
    }

    printf("Watchdog started for client %d, timeout=%d seconds\n",
           client->player.id, CLIENT_TIMEOUT_SECONDS);
}

void feed_player_watchdog(timer_t *timer_id)
{
    struct itimerspec ts = {
        .it_value = {.tv_sec = CLIENT_TIMEOUT_SECONDS},
        .it_interval = {.tv_sec = 0}};
    timer_settime(*timer_id, 0, &ts, NULL);
}

void *client_loop(void *args)
{
    client_thread_args_t *client_args = (client_thread_args_t *)args;
    server_state_t *state = client_args->state;
    int idx = client_args->client_idx;
    free(client_args); // dont need this anymore

    int fd = state->clients[idx].fd;
    msg_generic_t header;
    timer_t watchdog_timer;
    watchdog_param_t *watchdog_param = NULL;
    start_player_watchdog(&watchdog_timer, &state->clients[idx], state, &watchdog_param);

    printf("Started client thread for client %d\n", idx);

    bool exit_thread = false;
    while (read_exact(fd, &header, sizeof(header)) == 0)
    {
        pthread_mutex_lock(&state->mutex);

        if (state->clients[idx].connected)
        {
            state->clients[idx].waiting_for_pong = false;
            feed_player_watchdog(&watchdog_timer);
        }

        printf("Received msg type %u from client %d\n", header.msg_type, idx);

        switch (header.msg_type)
        {
        case MSG_LEAVE:
        {
            remove_client(state, idx);
            break;
        }

        case MSG_SET_READY:
        {
            state->clients[idx].player.ready = true;
            broadcast_set_ready(state, idx);

            // check if all player ready to start game
            if (state->game_status == GAME_LOBBY && all_players_ready(state))
            {
                start_game(state);
            }

            break;
        }

        case MSG_PING:
        {
            int res = send_pong(fd, SERVER, header.sender_id);
            // error while sending PONG, probably client disconnected
            if (res < 0)
            {
                remove_client(state, idx);
                exit_thread = true;
                break;
            }

            break;
        }

        case MSG_PONG:
        {
            break;
        }

        case MSG_MOVE_ATTEMPT:
        {
            msg_move_attempt_t payload;
            if (read_exact(fd, &payload, sizeof(payload)) < 0)
            {
                remove_client(state, idx);
                exit_thread = true;
                break; // exit thread
            }

            event_t ev = {
                .type = EVENT_MOVE,
                .player_id = idx,
                .data.direction = payload.direction,
            };

            int res = enqueue_event(&state->queue, &ev);
            if (res < 0)
                printf("Failed to push move event for client %d\n", idx);

            break;
        }

        case MSG_BOMB_ATTEMPT:
        {
            msg_bomb_attempt_t payload;
            if (read_exact(fd, &payload, sizeof(payload)) < 0)
            {
                remove_client(state, idx);
                exit_thread = true;
                break; // exit thread
            }

            event_t ev = {
                .type = EVENT_BOMB,
                .player_id = idx,
                .data.cell = ntohs(payload.cell),
            };

            int res = enqueue_event(&state->queue, &ev);
            if (res < 0)
                printf("Failed to push bomb event for client %d\n", idx);
            break;
        }

        default:
            printf("Unhandled msg %u from client %d\n", header.msg_type, idx);
        }

        pthread_mutex_unlock(&state->mutex);

        if (exit_thread)
            break;
    }

    printf("Client %d disconnected\n", idx);

    // recv return 0 meaning client disconnected
    // maybe already removed by watchdog
    pthread_mutex_lock(&state->mutex);
    if (state->clients[idx].connected)
        remove_client(state, idx);
    pthread_mutex_unlock(&state->mutex);

    // remove timer
    timer_delete(watchdog_timer);
    if (watchdog_param)
        free(watchdog_param);

    return NULL;
}

void broadcast_hello(server_state_t *state, int sender_idx, const msg_hello_t *hello)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected && i != sender_idx)
        {
            // vienīgais veids, kā nodot jaunā sender_idx,
            // ir iestatot sender_id kā apraides avotu
            send_hello(state->clients[i].fd, sender_idx, BROADCAST, hello);
        }
    }
}

void broadcast_set_ready(server_state_t *state, int sender_idx)
{
    msg_generic_t header = {
        .msg_type = MSG_SET_READY,
        .sender_id = sender_idx,
        .target_id = -1,
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected)
        {
            header.target_id = state->clients[i].player.id;
            write_exact(state->clients[i].fd, &header, sizeof(header));
        }
    }
}

void broadcast_leave(server_state_t *state, int sender_idx)
{
    msg_generic_t header = {
        .msg_type = MSG_LEAVE,
        .sender_id = sender_idx,
        .target_id = BROADCAST};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected && i != sender_idx)
        {
            write_exact(state->clients[i].fd, &header, sizeof(header));
        }
    }
}

void broadcast_set_game_status(server_state_t *state, game_status_t status)
{
    msg_set_status_t payload = {
        .game_status = status,
    };

    msg_generic_t header = {
        .msg_type = MSG_SET_STATUS,
        .sender_id = SERVER,
        .target_id = BROADCAST,
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected)
        {
            write_exact(state->clients[i].fd, &header, sizeof(header));
            write_exact(state->clients[i].fd, &payload, sizeof(payload));
        }
    }
}

void broadcast_map(server_state_t *state)
{
    msg_map_t map_msg = {
        .height = state->map.rows,
        .width = state->map.cols};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        // abstrakcijai visur vajadzētu send_x izmantot
        send_map(state->clients[i].fd, SERVER, BROADCAST, &map_msg, state->map.cells);
    }
}

void broadcast_moved(server_state_t *state, uint8_t player_id, uint16_t cell)
{
    msg_moved_t moved_msg = {
        .player_id = player_id,
        .cell = htons(cell)};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        send_moved(state->clients[i].fd, SERVER, state->clients[i].player.id, &moved_msg);
    }
}

// Send the full running-game state to a single client, using only
// existing protocol messages. Used after a mid-game reconnect so the
// returning client rebuilds map, players, bombs, explosions, and bonuses.
void sync_board_to_client(server_state_t *state, int idx)
{
    int fd = state->clients[idx].fd;

    // 1. current map (soft blocks destroyed, bonuses/bombs imprinted)
    msg_map_t map_msg = {
        .height = state->map.rows,
        .width = state->map.cols,
    };
    if (send_map(fd, SERVER, idx, &map_msg, state->map.cells) < 0)
        return;

    // 2. position of every connected player (alive or dead body)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        player_t *p = &state->clients[i].player;
        msg_moved_t moved = {
            .player_id = p->id,
            .cell = htons(make_cell_index(p->row, p->col, state->map.cols)),
        };
        if (send_moved(fd, SERVER, idx, &moved) < 0)
            return;
    }

    // 3. death state for already-dead players
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        if (state->clients[i].player.alive)
            continue;

        msg_generic_t header = {
            .msg_type = MSG_DEATH,
            .sender_id = state->clients[i].player.id,
            .target_id = idx,
        };
        msg_death_t payload = {
            .player_id = state->clients[i].player.id,
        };
        if (write_exact(fd, &header, sizeof(header)) < 0)
            return;
        if (write_exact(fd, &payload, sizeof(payload)) < 0)
            return;
    }

    // 4. active bombs
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->bombs[i].active)
            continue;

        msg_generic_t header = {
            .msg_type = MSG_BOMB,
            .sender_id = state->bombs[i].owner_id,
            .target_id = idx,
        };
        msg_bomb_t payload = {
            .player_id = state->bombs[i].owner_id,
            .cell = htons(make_cell_index(state->bombs[i].row,
                                          state->bombs[i].col,
                                          state->map.cols)),
        };
        if (write_exact(fd, &header, sizeof(header)) < 0)
            return;
        if (write_exact(fd, &payload, sizeof(payload)) < 0)
            return;
    }

    // 5. active explosions (still burning)
    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (!state->explosions[i].source.active)
            continue;

        bomb_t *src = &state->explosions[i].source;
        msg_generic_t header = {
            .msg_type = MSG_EXPLOSION_START,
            .sender_id = SERVER,
            .target_id = idx,
        };
        msg_explosion_start_t payload = {
            .radius = src->radius,
            .cell = htons(make_cell_index(src->row, src->col, state->map.cols)),
        };
        if (write_exact(fd, &header, sizeof(header)) < 0)
            return;
        if (write_exact(fd, &payload, sizeof(payload)) < 0)
            return;
    }

    // 6. bonuses lying on the map
    for (size_t i = 0; i < state->bonus_count; i++)
    {
        if (!state->bonuses[i].active)
            continue;

        msg_generic_t header = {
            .msg_type = MSG_BONUS_AVAILABLE,
            .sender_id = SERVER,
            .target_id = idx,
        };
        msg_bonus_available_t payload = {
            .bonus_type = state->bonuses[i].type,
            .cell = htons(make_cell_index(state->bonuses[i].row,
                                          state->bonuses[i].col,
                                          state->map.cols)),
        };
        if (write_exact(fd, &header, sizeof(header)) < 0)
            return;
        if (write_exact(fd, &payload, sizeof(payload)) < 0)
            return;
    }
}

bool all_players_ready(server_state_t *state)
{
    if (state->player_count < 2)
        return false;

    if (state->player_count < 2)
        return false;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected && !state->clients[i].player.ready)
            return false;
    }

    return true;
}

void start_game(server_state_t *state)
{
    memset(state->bombs, 0, sizeof(state->bombs));
    memset(state->explosions, 0, sizeof(state->explosions));
    state->current_tick = 0;

    state->game_status = GAME_RUNNING;

    // set all players to alive and put them on start positions
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        player_t *p = &state->clients[i].player;
        p->alive = true;
        p->row = state->config.start_row[i];
        p->col = state->config.start_col[i];
        p->bomb_count = START_BOMB_COUNT;
        p->bomb_radius = state->config.explosion_radius;
        p->bomb_timer_ticks = state->config.bomb_timer_ticks;

        // set last_move_tick so that first player move is allowed immediately at game start
        if (p->speed > 0)
        {
            uint64_t ticks_per_move = (TICKS_PER_SECOND + p->speed - 1) / p->speed;
            p->last_move_tick = (state->current_tick >= ticks_per_move)
                                    ? (state->current_tick - ticks_per_move)
                                    : 0;
        }
        else
        {
            p->last_move_tick = state->current_tick;
        }
    }

    // 1. broadcast SET_STATUS
    broadcast_set_game_status(state, GAME_RUNNING);

    // 2. broadcast MAP
    broadcast_map(state);

    // 3. broadcast positions
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        player_t *p = &state->clients[i].player;
        uint16_t cell = make_cell_index(p->row, p->col, state->map.cols);

        broadcast_moved(state, p->id, cell);
    }
}
