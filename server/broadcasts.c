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

