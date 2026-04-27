#include <ncurses.h>
#include <stdio.h>
#include "draw.h"

void draw_init_colors(void)
{
    start_color();
    use_default_colors();

    // color number, foreground, background
    init_pair(C_READY, COLOR_GREEN, -1);
    init_pair(C_NOT_READY, COLOR_RED, -1);
    init_pair(C_WALL, COLOR_WHITE, COLOR_BLACK);
    init_pair(C_BORDER, COLOR_WHITE, COLOR_BLACK);
    init_pair(C_SOFT, COLOR_YELLOW, COLOR_BLACK);
    init_pair(C_EMPTY, COLOR_BLACK, COLOR_BLACK);
    init_pair(C_BOMB, COLOR_BLACK, COLOR_CYAN);
    init_pair(C_EXPLOSION, COLOR_RED, COLOR_BLACK);
    init_pair(C_BONUS, COLOR_GREEN, COLOR_BLACK);
    init_pair(C_PLAYER, COLOR_WHITE, COLOR_BLUE);
    init_pair(C_ME, COLOR_WHITE, COLOR_MAGENTA);

    if (can_change_color()) {
        // ncurses use color scale from 0 to 1000
        init_color(COLOR_GRAY, 100, 100, 100);
        init_pair(C_BORDER, COLOR_WHITE, COLOR_GRAY);
    }
}


void draw_tile(int y, int x, int color_pair, char fill, const char *label)
{
    attron(COLOR_PAIR(color_pair));

    for (int dy = 0; dy < TILE_H; dy++) {
        for (int dx = 0; dx < TILE_W; dx++) {
            mvaddch(y + dy, x + dx, fill);
        }
    }

    if (label != NULL && label[0] != '\0') {
        attron(A_BOLD);
        mvprintw(y + TILE_H / 2, x + TILE_W / 2, "%-2s", label);
        attroff(A_BOLD);
    }

    attroff(COLOR_PAIR(color_pair));
}

int color_for_cell(char cell)
{
    switch (cell) {
        case HARD_BLOCK: return C_WALL;
        case SOFT_BLOCK: return C_SOFT;
        case BOMB: return C_BOMB;
        case EXPLOSION_CELL: return C_EXPLOSION;
        case SPEED_BONUS:
        case BOMB_RADIUS_BONUS:
        case BOMB_TIMER_BONUS:
            return C_BONUS;
        case EMPTY:
        default:
            return C_EMPTY;
    }
}

char fill_for_cell(char cell)
{
    switch (cell) {
        case HARD_BLOCK: return 'X';
        case SOFT_BLOCK: return '/';
        case EMPTY: return ' ';
        case BOMB: return ' ';
        case EXPLOSION_CELL: return '~';
        case SPEED_BONUS: return '@';
        case BOMB_RADIUS_BONUS: return '$';
        case BOMB_TIMER_BONUS: return '%';
        default: return ' ';
    }
}


void draw_lobby(const client_state_t *state)
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


void draw_running(const client_state_t *state)
{
    erase();

    attron(A_BOLD);
    mvprintw(0, 2, "BOMBERMAN");
    attroff(A_BOLD);

    int start_row = 2;
    int start_col = 2;

    for (int r = -1; r <= state->map.rows; r++) {
        for (int c = -1; c <= state->map.cols; c++) {
            if (r >= 0 && r < state->map.rows && c >= 0 && c < state->map.cols)
                continue;

            int y = start_row + (r + 1) * TILE_H;
            int x = start_col + (c + 1) * TILE_W;
            draw_tile(y, x, C_BORDER, '#', NULL);
        }
    }

    for (int r = 0; r < state->map.rows; r++) {
        for (int c = 0; c < state->map.cols; c++) {
            uint16_t idx = make_cell_index(r, c, state->map.cols);

            char cell = state->map.cells[idx];
            if (state->overlay_map.cells[idx] != EMPTY) {
                cell = state->overlay_map.cells[idx];
            }

            // offset by 1 tile for border
            int y = start_row + (r + 1) * TILE_H;
            int x = start_col + (c + 1) * TILE_W;

            const char *label = "";

            draw_tile(y, x, color_for_cell(cell), fill_for_cell(cell), label);
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].name[0] == '\0')
            continue;
        if (!state->players[i].alive)
            continue;

        // offset by 1 tile for border
        int y = start_row + (state->players[i].row + 1) * TILE_H;
        int x = start_col + (state->players[i].col + 1) * TILE_W;

        char label[4];
        snprintf(label, sizeof(label), "%u", state->players[i].id);

        draw_tile(y, x, i == state->my_id ? C_ME : C_PLAYER, ' ', label);
    }

    // offset by 2 tile for border
    int panel_x = start_col + (state->map.cols+2) * TILE_W + 4;

    attron(A_BOLD);
    mvprintw(2, panel_x, "Spēlētāji");
    attroff(A_BOLD);

    int row = 4;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].name[0] == '\0')
            continue;

        mvprintw(row++, panel_x, "[%u] %s%s",
                 state->players[i].id,
                 state->players[i].name,
                 i == state->my_id ? " (jūs)" : "");
    }

    // offset by 2 tile for border
    mvprintw(start_row + (state->map.rows+2) * TILE_H + 2, 2,
             "WASD: kustēties   B: spridzini   X: beigt spēli");

    refresh();
}


void draw_end(const client_state_t *state)
{
    erase();
    mvprintw(0, 0, "Bomberman");
    mvprintw(2, 0, "Spēle beigusies: uzvarēja %s", state->players[state->winner_id].name);
    refresh();
}
