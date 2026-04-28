#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "handle_client.h"
#include "game.h"
#include "map.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// for signal handler access
static server_state_t *global_server_state = NULL;

void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("Received SIGINT, shutting down server...\n");
        if (global_server_state != NULL)
        {
            global_server_state->server_running = false;
        }
    }
}

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
    memset(&server_state, 0, sizeof(server_state));
    server_state.game_status = GAME_LOBBY;
    server_state.server_running = true;
    server_state.initiator_id = 255;  // no initiator yet

    init_event_queue(&server_state.queue);
    pthread_mutex_init(&server_state.mutex, NULL);

    // add signal handler
    // do not set SA_RESTART flag for SIGINT handler, so that accept() will be interrupted
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction SIGINT");
        return 1;
    }

    if (sigaction(SIGTERM, &sa, NULL) < 0)
    {
        perror("sigaction SIGTERM");
        return 1;
    }
    // set global pointer for signal handler access
    global_server_state = &server_state;

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

    // save map path, it will be read when the game starts
    snprintf(server_state.selected_map_path, sizeof(server_state.selected_map_path), "%s", map_filename);

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

    while (server_state.server_running)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            if (errno == EINTR && !server_state.server_running)
                break;

            if (!server_state.server_running)
                break;

            perror("accept");
            continue;
        }

        pthread_mutex_lock(&server_state.mutex);
        int free_idx = add_client(&server_state, client_fd, &client_addr);
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
    close_all_client_fds(&server_state);
    main_cleanup(&server_state);
    return 0;
}


void close_all_client_fds(server_state_t *state)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (state->clients[i].connected)
        {
            close(state->clients[i].fd);
            state->clients[i].connected = false;
        }
    }
}


void main_cleanup(server_state_t *state)
{
    state->bonus_count = 0;

    for (int i = 0; i < MAX_BOMBS; i++)
    {
        if (state->explosions[i].footprint != NULL)
        {
            free(state->explosions[i].footprint);
            state->explosions[i].footprint = NULL;
            state->explosions[i].footprint_size = 0;
        }
    }

    // destroy mutex
    pthread_mutex_destroy(&state->mutex);

    // destroy event queue
    cleanup_event_queue(&state->queue);

    // memset state to 0
    memset(state, 0, sizeof(server_state_t));

    printf("Server shutdown complete.\n");
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


int add_client(server_state_t *state, int fd, struct sockaddr_in *client_addr)
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

    bool is_reconnecting = false;
    int reconnect_idx = -1;

    // check if game is already in progress
    if (state->game_status != GAME_LOBBY)
    {
        // check if this player was previously connected and is trying to reconnect
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (state->clients[i].connected)
                continue;
            if (state->clients[i].player.name[0] == '\0')
                continue; // empty slot, skip

            // check name and ip
            if (strncmp(state->clients[i].player.name, hello_player_name, MAX_NAME_LEN) == 0 &&
                state->clients[i].addr.sin_addr.s_addr == client_addr->sin_addr.s_addr)
            {
                is_reconnecting = true;
                reconnect_idx = i;
                break;
            }
        }

        if (!is_reconnecting)
        {
            printf("Game already in progress, rejecting client\n");
            send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
            close(fd);
            return -1;
        }
    }

    int free_idx = is_reconnecting
                       ? reconnect_idx
                       : find_free_slot(state->clients);
    if (free_idx < 0)
    {
        printf("Server full, rejecting client\n");
        send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
        close(fd);
        return -1;
    }

    // first client is the initiator
    if (!is_reconnecting && state->initiator_id == 255)
        state->initiator_id = free_idx;

    // check client version compatibility
    // commented as we want to allow other client versions to connect
    // if (strncmp(hello_client_id, CLIENT_ID, MAX_CLIENT_ID_LEN) != 0)
    // {
    //     printf("Unsupported client version: %s\n", hello_client_id);
    //     send_disconnect(fd, SERVER, 255); // TODO: Idk if 255 here is correct
    //     close(fd);
    //     return -1;
    // }

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

    // player id must always match the slot
    p->id = free_idx;

    // 2. register client
    c->fd = fd;
    c->connected = true;
    memcpy(&c->addr, client_addr, sizeof(struct sockaddr_in));
    // save client version
    strncpy(c->version, hello_client_id, MAX_CLIENT_ID_LEN);
    c->version[MAX_CLIENT_ID_LEN] = '\0';

    if (is_reconnecting)
    {
        p->ready = true;
        c->waiting_for_pong = false;
    }
    else
    {
        p->ready = false;
        p->last_move_tick = 0;
        p->alive = true;
        
        strncpy(p->name, hello_player_name, MAX_NAME_LEN);
        p->name[MAX_NAME_LEN] = '\0';
    }

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

    if (send_welcome(fd, free_idx, free_idx, &welcome) < 0)
    {
        printf("Failed to send WELCOME\n");
        remove_client_quietly(state, free_idx);
        return -1;
    }

    printf("Sent WELCOME to player %s (id=%d)\n", p->name, p->id);

    // send map configureation choices if this client is the initiator
    // and the client's version supports it
    if (free_idx == state->initiator_id && strcmp(c->version, CLIENT_ID) >= 0)
    {
        if (find_available_map_choices_and_send(state, fd, free_idx) < 0) {
            printf("Failed to send map choices to initiator\n");
            remove_client_quietly(state, free_idx);
            return -1;
        }
    }


    // 4. inform all other clients about the new player
    // just retranslate HELLo message to all clients
    broadcast_hello(state, free_idx, &hello);

    printf("Broadcasted HELLO of player %s (id=%d) to other clients\n", p->name, p->id);

    // Serveris ir tiesīgs nosūtīt šo ziņu arī tad, ja attiecīgais klients nav tādu nosūtījis, 
    // tādējādi “piespiedu kārtā” padarot to par spēlētāju. 
    // Tā var īstenot, piemēram, iespēju pieslēgties pēc savienojuma pazušanas spēles laikā.
    if (is_reconnecting)
    {
        printf("Player %s (id=%d) is reconnecting, setting ready state to true\n", p->name, p->id);
        p->ready = true;
        broadcast_set_ready(state, free_idx);

        // resend the running-game state so the returning client rebuilds
        // map, players, bombs, explosions, and bonuses
        sync_board_to_client(state, free_idx);
    }

    return free_idx;
}


void remove_client_quietly(server_state_t *state, int id)
{
    close(state->clients[id].fd);
    state->clients[id].fd = -1;
    state->clients[id].connected = 0;

    state->clients[id].player.ready = 0;

    state->player_count--;

    // if the initiator left, end game
    if (id == state->initiator_id)
        state->server_running = false;
}

void remove_client(server_state_t *state, int id)
{
    // broadcast LEAVE message to all other clients before removing
    broadcast_leave(state, id);

    close(state->clients[id].fd);
    state->clients[id].fd = -1;
    state->clients[id].connected = 0;

    state->clients[id].player.ready = 0;

    state->player_count--;

    // if the initiator left, end game
    if (id == state->initiator_id) 
        state->server_running = false;
}
