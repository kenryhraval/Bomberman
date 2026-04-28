#include "broadcasts.h"
#include "server.h"

#include <arpa/inet.h>

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


void broadcast_explosion_start(server_state_t *state, uint16_t row, uint16_t col, uint8_t radius)
{
    msg_generic_t header = {
        .msg_type = MSG_EXPLOSION_START,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    msg_explosion_start_t payload = {
        .cell = htons(make_cell_index(row, col, state->map.cols)),
        .radius = radius};

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
        .cell = htons(make_cell_index(row, col, state->map.cols))};

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        write_exact(state->clients[i].fd, &header, sizeof(header));
        write_exact(state->clients[i].fd, &payload, sizeof(payload));
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


void broadcast_bonus_collected(server_state_t *state, uint8_t player_id, uint16_t cell)
{
    msg_generic_t header = {
        .msg_type = MSG_BONUS_RETRIEVED,
        .sender_id = player_id,
        .target_id = BROADCAST};

    msg_bonus_retrieved_t payload = {
        .player_id = player_id,
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


void broadcast_bonus_available(server_state_t *state, bonus_type_t bonus_type, uint16_t cell)
{
    msg_generic_t header = {
        .msg_type = MSG_BONUS_AVAILABLE,
        .sender_id = SERVER,
        .target_id = BROADCAST};

    msg_bonus_available_t payload = {
        .bonus_type = bonus_type,
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


void broadcast_statistics(server_state_t *state)
{
    msg_generic_t header = {
        .msg_type = MSG_STATISTICS,
        .sender_id = SERVER,
        .target_id = BROADCAST};
        
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;

        if (write_exact(state->clients[i].fd, &header, sizeof(header)) < 0)
            continue;

        // make sure endianess is correct when sending statistics
        msg_statistics_t payload = {
            .stats = {
                .kills = state->stats[i].kills,
                .blocks_destroyed = htons(state->stats[i].blocks_destroyed),
                .bonuses_collected = htons(state->stats[i].bonuses_collected)
            }
        };

        if (write_exact(state->clients[i].fd, &payload, sizeof(payload)) < 0)
            continue;
    }
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
