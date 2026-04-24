#include "client.h"
#include "event_queue.h"

void *client_loop(void *args)
{
    client_thread_args_t *client_args = (client_thread_args_t *)args;
    server_state_t *state = client_args->state;
    int idx = client_args->client_idx;
    free(client_args); // dont need this anymore

    int fd = state->clients[idx].fd;
    msg_generic_t header;

    while (read_exact(fd, &header, sizeof(header)) > 0)
    {
        pthread_mutex_lock(&state->mutex);

        switch (header.msg_type)
        {
        case MSG_LEAVE:
        {
            remove_client(state, idx);
            pthread_mutex_unlock(&state->mutex);
            return NULL; // exit thread
        }

        case MSG_SET_READY:
        {
            state->clients[idx].player.ready = true;
            broadcast_set_ready(state, idx);

            // check if all player ready to start game
            if (all_players_ready(state))
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
                pthread_mutex_unlock(&state->mutex);
                return NULL; // exit thread
            }

            break;
        }

        case MSG_PONG:
        {
            state->clients[idx].last_pong = time(NULL);
            break;
        }

        case MSG_MOVE_ATTEMPT:
        {
            msg_move_attempt_t payload;
            if (read_exact(fd, &payload, sizeof(payload)) < 0)
            {
                remove_client(state, idx);
                pthread_mutex_unlock(&state->mutex);
                return NULL; // exit thread
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
            // likt event_queue
            break;

        default:
            printf("Unhandled msg %u from client %d\n", header.msg_type, idx);
        }

        pthread_mutex_unlock(&state->mutex);
    }

    // recv return 0 meaning client discontected
    pthread_mutex_lock(&state->mutex);
    remove_client(state, idx);
    pthread_mutex_unlock(&state->mutex);

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

void broadcast_set_status(server_state_t *state, game_status_t status)
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
    msg_generic_t header = {
        .msg_type = MSG_MAP,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    // payload: H, W, then cells
    uint8_t h = state->map->rows;
    uint8_t w = state->map->cols;
    uint16_t cell_count = h * w;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        int fd = state->clients[i].fd;
        write_exact(fd, &header, sizeof(header));
        write_exact(fd, &h, sizeof(h));
        write_exact(fd, &w, sizeof(w));
        write_exact(fd, state->map->cells, cell_count);
    }
}

void broadcast_sync_board(server_state_t *state)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        player_t *p = &state->clients[i].player;

        msg_generic_t header = {
            .msg_type = MSG_SYNC_BOARD,
            .sender_id = p->id,
            .target_id = BROADCAST};

        // sūta katram klientam info par šo spēlētāju
        for (int j = 0; j < MAX_PLAYERS; j++)
        {
            if (!state->clients[j].connected)
                continue;

            int fd = state->clients[j].fd;
            write_exact(fd, &header, sizeof(header));
            write_exact(fd, p, sizeof(player_t));
        }
    }
}

bool all_players_ready(server_state_t *state)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected && !state->clients[i].player.ready)
        {
            return false;
        }
    }
    return true;
}

void start_game(server_state_t *state)
{
    state->game_status = GAME_RUNNING;
    
    // set all players to alive and put them on start positions
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        player_t *p = &state->clients[i].player;
        p->alive = true;
        p->row = state->map->configs.start_row[i];
        p->col = state->map->configs.start_col[i];
    }

    // 1. broadcast SET_STATUS
    broadcast_set_status(state, GAME_RUNNING);

    // 2. broadcast MAP
    broadcast_map(state);

    // 3. broadcast SYNC_BOARD for each player
    broadcast_sync_board(state);
}