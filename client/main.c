#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "client.h"
#include "draw.h"
#include "configs.h"


static int read_name_screen(char *player_name, size_t player_name_size)
{
    int pos = 0;
    int ch;

    memset(player_name, 0, player_name_size);

    erase();

    while (true)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)rows;

        const char *title = "BOMBERMAN";
        const char *prompt = "Ievadiet lietotājvārdu:";

        erase();

        attron(A_BOLD);
        mvprintw(2, (cols - (int)strlen(title)) / 2, "%s", title);
        attroff(A_BOLD);

        mvprintw(5, (cols - (int)strlen(prompt)) / 2, "%s", prompt);

        int box_w = MAX_NAME_LEN + 4;
        int box_x = (cols - box_w) / 2;
        int box_y = 7;

        mvprintw(box_y, box_x, "+");
        for (int i = 0; i < box_w - 2; i++)
            addch('-');
        addch('+');

        mvprintw(box_y + 1, box_x, "| %-*s |", MAX_NAME_LEN, player_name);

        mvprintw(box_y + 2, box_x, "+");
        for (int i = 0; i < box_w - 2; i++)
            addch('-');
        addch('+');

        mvprintw(box_y + 4, box_x, "Enter: turpināt");
        mvprintw(box_y + 5, box_x, "Esc: iziet");

        move(box_y + 1, box_x + 2 + pos);
        refresh();

        ch = getch();

        if (ch == 27) // Esc
            return -1;

        if (ch == '\n' || ch == KEY_ENTER)
        {
            if (pos > 0)
                return 0;
            continue;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
        {
            if (pos > 0)
            {
                pos--;
                player_name[pos] = '\0';
            }
            continue;
        }

        if (ch >= 32 && ch <= 126 && pos < MAX_NAME_LEN)
        {
            player_name[pos++] = (char)ch;
            player_name[pos] = '\0';
        }
    }
}


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

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(-1); // blocking input while entering name
    curs_set(1);

    draw_init_colors();

    if (read_name_screen(player_name, sizeof(player_name)) < 0)
    {
        endwin();
        return 0;
    }

    timeout(1000 / TICK_RATE);
    curs_set(0);

    if (client_connect(&state, ip, port) < 0) {
        endwin();
        printf("Failed to connect\n");
        return 1;
    }

    if (client_handshake(&state, player_name) < 0) {
        endwin();
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
            case QUIT_KEY:
            case QUIT_KEY_UPPER:
                running = 0;
                break;

            case READY_KEY:
            case READY_KEY_UPPER:
                if (state.game_status == GAME_LOBBY &&
                    !state.players[state.my_id].ready) {
                    state.players[state.my_id].ready = true;
                    send_set_ready(state.fd, state.my_id, SERVER);
                }
                break;

            case UP_KEY:
            case UP_KEY_UPPER:
            case KEY_UP:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_UP);
                break;

            case DOWN_KEY:
            case DOWN_KEY_UPPER:
            case KEY_DOWN:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_DOWN);
                break;

            case LEFT_KEY:
            case LEFT_KEY_UPPER:
            case KEY_LEFT:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_LEFT);
                break;

            case RIGHT_KEY:
            case RIGHT_KEY_UPPER:
            case KEY_RIGHT:
                if (state.game_status == GAME_RUNNING)
                    send_move_attempt(state.fd, state.my_id, DIR_RIGHT);
                break;

            case BOMB_KEY:
            case BOMB_KEY_UPPER:
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

