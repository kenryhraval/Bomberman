#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "client.h"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 6969
#define FPS 20


static void draw_lobby(const client_state_t *client)
{
    erase();

    player_t me = client->players[client->my_id];

    mvprintw(0, 0, "Bomberman");
    mvprintw(2, 0, "Jauna spēle: jūs esat priekšnamā!");

    if (!me.ready)
        mvprintw(4, 0, "Spied 'R', lai paziņotu gatavību");
    else
        mvprintw(4, 0, "Esat gatavs! Gaidiet citus spēlētājus...");

    mvprintw(6, 0, "Spēlētāji:");

    int row = 8;
    for (int id = 0; id < MAX_PLAYERS; id++) {
        if (client->players[id].name[0] == '\0')
            continue;

        attron(COLOR_PAIR(client->players[id].ready ? 1 : 2));
        mvprintw(row, 0, "[%u] %s%s",
                 client->players[id].id,
                 client->players[id].name,
                 id == client->my_id ? " (jūs)" : "");
        attroff(COLOR_PAIR(client->players[id].ready ? 1 : 2));

        row++;
    }

    mvprintw(18, 0, "Spied 'X', lai pamestu spēli");
    refresh();
}


static void draw_running(const client_state_t *client)
{
    erase();

    refresh();
}


static void draw_end(const client_state_t *client)
{
    erase();

    refresh();
}


int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    const char *ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    
    client_state_t game_state = {0};

    if (argc >= 2) ip = argv[1]; 
    if (argc >= 3) port = atoi(argv[2]);

    char player_name[MAX_NAME_LEN + 1] = {0};

    printf("Ievadiet lietotājvārdu: ");
    fflush(stdout);
    if (fgets(player_name, sizeof(player_name), stdin) == NULL) {
        printf("Failed to read player name\n");
        return 1;
    }
    player_name[strcspn(player_name, "\n")] = '\0';

    if (client_connect(&game_state, ip, port) < 0) {
        printf("Failed to connect\n");
        return 1;
    }

    if (client_handshake(&game_state, player_name) < 0) {
        printf("Handshake failed\n");
        client_close(&game_state);
        return 1;
    }

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(1000 / FPS);
    curs_set(0);

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);

    int running = 1;
    while (running) {
        if (client_poll_network(&game_state) < 0) {
            running = 0;
            break;
        }

        switch (game_state.game_status) {
            case GAME_LOBBY:
                draw_lobby(&game_state);
                break;

            case GAME_RUNNING:
                draw_running(&game_state);
                break;

            case GAME_END:
                draw_end(&game_state);
                break;
        }

        int ch = getch();
        switch (ch) {
            case 'x':
            case 'X':
                running = 0;
                break;
            case 'r':
            case 'R':
                if (!game_state.players[game_state.my_id].ready) {
                    game_state.players[game_state.my_id].ready = true;
                    send_set_ready(game_state.fd, game_state.my_id, SERVER);
                }
                break;
        }
    }

    endwin();

    client_send_leave(&game_state);
    client_close(&game_state);
    return 0;
}

