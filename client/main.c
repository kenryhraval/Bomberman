#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "client.h"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 6969
#define FPS 20


static void draw_lobby(const client_state_t *state)
{
    erase();

    player_t me = state->players[state->my_id];

    mvprintw(0, 0, "Bomberman");
    mvprintw(2, 0, "Jauna spēle: jūs esat priekšnamā!");

    if (!me.ready)
        mvprintw(4, 0, "Spied 'R', lai paziņotu gatavību");
    else
        mvprintw(4, 0, "Esat gatavs! Gaidiet citus spēlētājus...");

    mvprintw(6, 0, "Spēlētāji:");

    int row = 8;
    for (int id = 0; id < MAX_PLAYERS; id++) {
        if (state->players[id].name[0] == '\0')
            continue;

        attron(COLOR_PAIR(state->players[id].ready ? 1 : 2));
        mvprintw(row, 0, "[%u] %s%s",
                 state->players[id].id,
                 state->players[id].name,
                 id == state->my_id ? " (jūs)" : "");
        attroff(COLOR_PAIR(state->players[id].ready ? 1 : 2));

        row++;
    }

    mvprintw(18, 0, "Spied 'X', lai pamestu spēli");
    refresh();
}


static void draw_running(const client_state_t *state)
{
    erase();

    mvprintw(0, 0, "Bomberman - spēle notiek");

    int start_row = 2;
    int start_col = 2;

    for (int r = 0; r < state->map.rows; r++) {
    for (int c = 0; c < state->map.cols; c++) {
        uint16_t idx = make_cell_index(r, c, state->map.cols);

        char cell = state->map.cells[idx];

        if (state->overlay_map.cells[idx] != EMPTY) {
            cell = state->overlay_map.cells[idx];
        }

        mvprintw(start_row + r, start_col + c * 2, "%c", cell);
    }
}

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].name[0] == '\0')
            continue;
        if (!state->players[i].alive)
            continue;

        mvprintw(start_row + state->players[i].row,
                 start_col + state->players[i].col * 2,
                 "%u",
                 state->players[i].id);
    }

    mvprintw(start_row + state->map.rows + 2, 0, "WASD - move, B - bomb, X - exit");

    refresh();
}


static void draw_end(const client_state_t *state)
{
    erase();
    mvprintw(0, 0, "Bomberman");
    mvprintw(2, 0, "Spēle beigusies: uzvarēja %s", state->players[state->winner_id].name);
    refresh();
}


int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    const char *ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    
    client_state_t state = {0};

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

    if (client_connect(&state, ip, port) < 0) {
        printf("Failed to connect\n");
        return 1;
    }

    if (client_handshake(&state, player_name) < 0) {
        printf("Handshake failed\n");
        client_close(&state);
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
        if (client_poll_network(&state) < 0) {
            running = 0;
            break;
        }

        switch (state.game_status) {
            case GAME_LOBBY:
                draw_lobby(&state);
                break;

            case GAME_RUNNING:
                draw_running(&state);
                break;

            case GAME_END:
                draw_end(&state);
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
                if (state.game_status == GAME_LOBBY &&
                    !state.players[state.my_id].ready) {
                    state.players[state.my_id].ready = true;
                    send_set_ready(state.fd, state.my_id, SERVER);
                }
                break;

            case 'w':
            case 'W':
            case KEY_UP:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_UP);
                break;

            case 's':
            case 'S':
            case KEY_DOWN:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_DOWN);
                break;

            case 'a':
            case 'A':
            case KEY_LEFT:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_LEFT);
                break;

            case 'd':
            case 'D':
            case KEY_RIGHT:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_RIGHT);
                break;

            case 'b':
            case 'B':
                if (state.game_status == GAME_RUNNING)
                    send_bomb_attempt(state.fd, state.my_id, state.players[state.my_id].row, state.players[state.my_id].col, state.map.cols);
                break;
        }
    }

    endwin();

    client_send_leave(&state);
    client_close(&state);
    return 0;
}

