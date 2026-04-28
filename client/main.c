#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "client.h"
#include "draw.h"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 6969

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    const char *ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    
    client_state_t state = {0};

    // initialize map choices in state to avoid drawing 
    // garbage data before we receive map choices from server
    state.map_choice_count = 0;
    state.selected_map_id = 0;
    memset(state.map_choices, 0, sizeof(state.map_choices));

    // noklusējuma skats ir spēles priekšnama spēlētāju saraksts
    state.view = VIEW_LOBBY;

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
    timeout(1000 / TICK_RATE);
    curs_set(0);

    draw_init_colors();

    int running = 1;
    while (running) {
        if (client_poll_network(&state) < 0) {
            running = 0;
            break;
        }

        switch (state.game_status) {
            case GAME_LOBBY:
                if (state.view == VIEW_MAP_SELECT)
                    draw_map_select(&state);
                else
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

            case 'm':
            case 'M':
                if (state.game_status == GAME_LOBBY &&
                    state.map_choice_count > 0 &&
                    !state.players[state.my_id].ready) {
                    state.view = VIEW_MAP_SELECT;
                }
                break;
        }

        if (state.game_status == GAME_LOBBY && state.view == VIEW_MAP_SELECT) {
            switch (ch) {
                case 27: // Esc
                    state.view = VIEW_LOBBY;
                    break;

                case KEY_UP:
                    if (state.map_choice_count > 0) {
                        if (state.selected_map_id == 0)
                            state.selected_map_id = state.map_choice_count - 1;
                        else
                            state.selected_map_id--;
                    }
                    break;

                case KEY_DOWN:
                    if (state.map_choice_count > 0)
                        state.selected_map_id = (state.selected_map_id + 1) % state.map_choice_count;
                    break;

                case '\n':
                case KEY_ENTER:
                    if (state.map_choice_count > 0) {
                        send_map_selected(state.fd, state.my_id, SERVER, state.selected_map_id);
                        state.view = VIEW_LOBBY;
                    }
                    break;

                case 'x':
                case 'X':
                    running = 0;
                    break;
            }

            continue;
        }
    }

    endwin();

    client_send_leave(&state);
    client_close(&state);
    return 0;
}

