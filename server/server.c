#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#include "server.h"
#include "handle_client.h"
#include "game.h"
#include "map.h"

#include "shared/protocol.h"

int player_name_in_use(const server_state_t *state, const char *name)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!state->clients[i].connected)
            continue;
        if (strncmp(state->clients[i].player.name, name, MAX_NAME_LEN) == 0)
            return 1;
    }
    return 0;
}

int serve_main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in remote_address;
    
    // global server structs
    server_state_t server_state;
    server_state.bonuses = NULL;
    server_state.bonus_count = 0;
    server_state.game_status = GAME_LOBBY;
    server_state.player_count = 0;
    server_state.current_tick = 0;
    memset(server_state.clients, 0, sizeof(server_state.clients));
    memset(server_state.bombs, 0, sizeof(server_state.bombs));
    memset(server_state.explosions, 0, sizeof(server_state.explosions));
    init_event_queue(&server_state.queue);
    pthread_mutex_init(&server_state.mutex, NULL);


    // get --map argument
    const char *map_filename = MAP_FILENAME_DEFAULT;
    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], MAP_ARGUMENT) == 0)
        {
            map_filename = argv[i + 1];
            break;
        }
    }

    // load map and config
    if (load_map(map_filename, &server_state) < 0)
    {
        perror("Failed to load map");
        return 1;
    }


    // set bomb_explosion_duration_ticks for each player based on config
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        server_state.clients[i].player.bomb_explosion_duration_ticks = server_state.config.explosion_duration_ticks;
    }

    // 1. create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    // avoid "Address already in use" error
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. bind
    remote_address.sin_family = AF_INET;
    remote_address.sin_addr.s_addr = INADDR_ANY;
    remote_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&remote_address, sizeof(remote_address)) < 0)
    {
        perror("bind");
        return 1;
    }

    // 3. listen
    if (listen(server_fd, MAX_PLAYERS) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // 4. start game loop thread
    pthread_t game_thread;
    pthread_create(&game_thread, NULL, game_loop, &server_state);

    while (true)
    {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&server_state.mutex);
        int free_idx = add_client(&server_state, client_fd);
        pthread_mutex_unlock(&server_state.mutex);

        if (free_idx < 0)
            continue; // rejected

        // spawn client thread
        client_thread_args_t *args = malloc(sizeof(client_thread_args_t));
        if (args == NULL)
        {
            perror("Failed to allocate memory for client thread args");
            exit(EXIT_FAILURE);
        }
        args->state = &server_state;
        args->client_idx = free_idx;

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_loop, args);
        pthread_detach(client_thread); // nav jāgaida
    }

    close(server_fd);
    return 0;
}

int find_free_slot(client_t clients[])
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!clients[i].connected)
        {
            return i;
        }
    }
    return -1;
}

int add_client(server_state_t *state, int fd)
{
    msg_generic_t header;
    msg_hello_t hello;
    char hello_client_id[MAX_CLIENT_ID_LEN + 1];
    char hello_player_name[MAX_NAME_LEN + 1];

    if (fd < 0)
    {
        perror("accept");
        return -1;
    }

    if (state->player_count >= MAX_PLAYERS)
    {
        printf("Server full, rejecting client\n");
        send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
        close(fd);
        return -1;
    }

    int free_idx = find_free_slot(state->clients);
    if (free_idx < 0)
    {
        printf("Server full, rejecting client\n");
        send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
        close(fd);
        return -1;
    }

    printf("Client connected\n");

    // 1. recieve HELLO message
    if (recv_hello(fd, &header, &hello) < 0)
    {
        printf("Failed to receive HELLO\n");
        send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
        close(fd);
        return -1;
    }

    if (/*header.sender_id != SERVER ||*/ header.target_id != SERVER)
    {
        printf("Invalid HELLO routing fields: sender=%u target=%u\n", header.sender_id, header.target_id);
        send_disconnect(fd, SERVER, 255);
        close(fd);
        return -1;
    }

    // save client_id and player_name from HELLO payload for later use
    memcpy(hello_client_id, hello.client_id, MAX_CLIENT_ID_LEN);
    hello_client_id[MAX_CLIENT_ID_LEN] = '\0';
    memcpy(hello_player_name, hello.player_name, MAX_NAME_LEN);
    hello_player_name[MAX_NAME_LEN] = '\0';

    // check client version compatibility
    if (strncmp(hello_client_id, CLIENT_ID, MAX_CLIENT_ID_LEN) != 0)
    {
        printf("Unsupported client version: %s\n", hello_client_id);
        send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
        close(fd);
        return -1;
    }

    // check for empty player name
    if (hello_player_name[0] == '\0')
    {
        printf("Rejecting client with empty player name\n");
        send_disconnect(fd, SERVER, 255);
        close(fd);
        return -1;
    }

    // check for duplicate player name
    if (player_name_in_use(state, hello_player_name))
    {
        printf("Duplicate player name rejected: %s\n", hello_player_name);
        send_disconnect(fd, SERVER, 255);
        close(fd);
        return -1;
    }

    client_t *c = &state->clients[free_idx];
    player_t *p = &state->clients[free_idx].player;

    // 2. register client
    c->fd = fd;
    c->connected = true;

    p->id = free_idx;
    p->alive = true;
    p->ready = false;
    p->last_move_tick = 0;
    p->speed = state->config.player_speed;
    p->bomb_count = 10; // TODO: idk what bomb count to start with
    p->bomb_radius = state->config.explosion_radius;
    p->bomb_timer_ticks = state->config.bomb_timer_ticks;
    p->row = state->config.start_row[free_idx];
    p->col = state->config.start_col[free_idx];

    strncpy(p->name, hello_player_name, MAX_NAME_LEN);
    p->name[MAX_NAME_LEN] = '\0';

    state->player_count++;

    printf("Player joined: %s (id=%d)\n", p->name, p->id);

    // 3. send WELCOME message to the new client
    msg_welcome_t welcome = {0};
    snprintf(welcome.server_id, sizeof(welcome.server_id), SERVER_ID);
    welcome.game_status = state->game_status;
    welcome.other_count = state->player_count - 1;
    for (int j = 0, k = 0; j < MAX_PLAYERS && k < welcome.other_count; j++)
    {
        if (state->clients[j].connected && j != free_idx)
        {
            welcome.others[k].player_id = state->clients[j].player.id;
            welcome.others[k].ready = state->clients[j].player.ready;
            strncpy(welcome.others[k].name, state->clients[j].player.name, MAX_NAME_LEN);
            welcome.others[k].name[MAX_NAME_LEN] = '\0';
            k++;
        }
    }
    int res = send_welcome(fd, free_idx, free_idx, &welcome);
    if (res < 0)
    {
        printf("Failed to send WELCOME\n");
        remove_client_quietly(state, free_idx);
        return -1;
    }

    printf("Sent WELCOME to player %s (id=%d)\n", p->name, p->id);

    // 4. inform all other clients about the new player
    // just retranslate HELLo message to all clients
    broadcast_hello(state, free_idx, &hello);

    printf("Broadcasted HELLO of player %s (id=%d) to other clients\n", p->name, p->id);

    return free_idx;
}


void remove_client_quietly(server_state_t *state, int id)
{
    close(state->clients[id].fd);
    state->clients[id].fd = -1;
    state->clients[id].connected = 0;

    state->clients[id].player.id = 0;
    state->clients[id].player.name[0] = '\0';
    state->clients[id].player.alive = 0;
    state->clients[id].player.ready = 0;

    state->player_count--;
}


void remove_client(server_state_t *state, int id)
{
    // broadcast LEAVE message to all other clients before removing
    broadcast_leave(state, id);

    close(state->clients[id].fd);
    state->clients[id].fd = -1;
    state->clients[id].connected = 0;

    state->clients[id].player.id = 0;
    state->clients[id].player.name[0] = '\0';
    state->clients[id].player.alive = 0;
    state->clients[id].player.ready = 0;

    state->player_count--;
}
