#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "client.h"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 6969
#define FPS 20

static void draw_ui(const client_state_t *client)
{
    erase();

    mvprintw(0, 0, "Bomberman");
    mvprintw(4, 0, "Spēlētāji:");

    mvprintw(6, 0, "[1] %s ready=%u", client->name, client->ready);
    for (int i = 0; i < client->welcome.other_count; i++) {
        mvprintw(7 + i, 0, "[%u] %s ready=%u", i + 2, 
            client->welcome.others[i].name, 
            client->welcome.others[i].ready);
    }

    mvprintw(15, 0, "Press q to quit");
    refresh();
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    const char *ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    
    client_state_t client = {0};

    if (argc >= 2) ip = argv[1]; 
    if (argc >= 3) port = atoi(argv[2]);

    printf("Ievadiet lietotājvārdu: ");
    fflush(stdout);
    if (fgets(client.name, sizeof(client.name), stdin) == NULL) {
        printf("Failed to read player name\n");
        return 1;
    }
    client.name[strcspn(client.name, "\n")] = '\0';

    if (client_connect(&client, ip, port) < 0) {
        printf("Failed to connect\n");
        return 1;
    }

    if (client_handshake(&client) < 0) {
        printf("Handshake failed\n");
        client_close(&client);
        return 1;
    }

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(1000 / FPS);
    curs_set(0);

    int running = 1;
    while (running) {
        if (client_poll_network(&client) < 0) {
            running = 0;
            break;
        }
        draw_ui(&client);

        int ch = getch();
        switch (ch) {
            case 'q':
                running = 0;
                break;
        }
    }

    endwin();

    client_send_leave(&client);
    client_close(&client);
    return 0;
}

