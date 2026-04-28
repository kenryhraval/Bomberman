#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "configs.h"
#include "helpers.h"

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


static int clamp_int(int value, int min, int max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}


void draw_lobby(const client_state_t *state)
{
    erase();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    const char *title = "BOMBERMAN";
    const char *subtitle = "Priekšnams";

    attron(A_BOLD);
    mvprintw(1, (cols - (int)strlen(title)) / 2, "%s", title);
    attroff(A_BOLD);

    mvprintw(3, (cols - (int)strlen(subtitle)) / 2, "%s", subtitle);

    player_t me = state->players[state->my_id];

    int box_w = 54;
    int start_x = (cols - box_w) / 2;
    int start_y = 5;

    mvprintw(start_y,     start_x, "+----------------------------------------------------+");
    mvprintw(start_y + 1, start_x, "| Spēlētāji                                          |");
    mvprintw(start_y + 2, start_x, "+------+------------------------------+--------------+");
    mvprintw(start_y + 3, start_x, "| ID   | Vārds                        | Statuss      |");
    mvprintw(start_y + 4, start_x, "+------+------------------------------+--------------+");

    int row = start_y + 5;

    for (int id = 0; id < MAX_PLAYERS; id++) {
        if (state->players[id].name[0] == '\0')
            continue;

        int color = state->players[id].ready ? C_READY : C_NOT_READY;
        const char *status = state->players[id].ready ? "Gatavs" : "Nav gatavs";

        char display_name[LOBBY_NAME_COL_W + 1];

        if (id == state->my_id) {
            snprintf(display_name, sizeof(display_name), "%.*s (tu)",
                    LOBBY_NAME_COL_W - 5,
                    state->players[id].name);
        } else {
            snprintf(display_name, sizeof(display_name), "%.*s",
                    LOBBY_NAME_COL_W,
                    state->players[id].name);
        }

        mvprintw(row, start_x, "| %-4u | %-28s | %-12s |",
                state->players[id].id,
                display_name,
                "");

        attron(COLOR_PAIR(color));
        mvprintw(row, start_x + 40, "%-12s", status);
        attroff(COLOR_PAIR(color));

        row++;
    }

    mvprintw(row++, start_x, "+------+------------------------------+--------------+");

    row += 2;

    if (!me.ready) {
        attron(A_BOLD);
        mvprintw(row++, start_x, "Spied R, lai paziņotu gatavību");
        attroff(A_BOLD);
    } else {
        mvprintw(row++, start_x, "Jūs esat gatavs. Gaida pārējos spēlētājus...");
    }

    if (state->map_choice_count > 0 && !me.ready) {
        mvprintw(row + 1, start_x, "Spied M, lai izvēlētos karti");
        mvprintw(row + 2, start_x, "Spied X, lai pamestu spēli");
    } else {
        mvprintw(row + 1, start_x, "Spied X, lai pamestu spēli");
    }

    refresh();
}


void draw_running(const client_state_t *state)
{
    erase();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    uint32_t tick = client_estimated_tick(state);
    uint32_t time_left = 0;

    if (tick < GAME_DRAW_TICK_TIMEOUT)
        time_left = GAME_DRAW_TICK_TIMEOUT - tick;

    player_t me = state->players[state->my_id];

    char header[64];
    snprintf(header, sizeof(header), "BOMBERMAN   Laiks: %u", time_left);

    attron(A_BOLD);
    mvprintw(0, (cols - (int)strlen(header)) / 2, "%s", header);
    attroff(A_BOLD);

    char bonuses[128];
    snprintf(bonuses, sizeof(bonuses),
             "Bumbas: %u   Rādius: %u   Ilgums: %u   Ātrums: %u",
             me.bomb_count,
             me.bomb_radius,
             me.bomb_explosion_duration_ticks,
             me.speed);

    mvprintw(1, (cols - (int)strlen(bonuses)) / 2, "%s", bonuses);

    int start_row = 3;
    int start_col = 2;

    /*
    * Use almost the whole terminal width for the map.
    */
    int available_w = cols - start_col - 4;
    int available_h = rows - start_row - 4;

    int visible_cols = available_w / TILE_W - 2; // -2 for border
    int visible_rows = available_h / TILE_H - 2; // -2 for border

    if (visible_cols > state->map.cols)
        visible_cols = state->map.cols;
    if (visible_rows > state->map.rows)
        visible_rows = state->map.rows;

    if (visible_cols < 1)
        visible_cols = 1;
    if (visible_rows < 1)
        visible_rows = 1;

    /*
     * Camera follows the current player.
     */
    int first_col = me.col - visible_cols / 2;
    int first_row = me.row - visible_rows / 2;

    first_col = clamp_int(first_col, 0, state->map.cols - visible_cols);
    first_row = clamp_int(first_row, 0, state->map.rows - visible_rows);

    int last_col = first_col + visible_cols - 1;
    int last_row = first_row + visible_rows - 1;

    /*
     * Draw border around visible part of map.
     */
    for (int r = -1; r <= visible_rows; r++) {
        for (int c = -1; c <= visible_cols; c++) {
            if (r >= 0 && r < visible_rows && c >= 0 && c < visible_cols)
                continue;

            int y = start_row + (r + 1) * TILE_H;
            int x = start_col + (c + 1) * TILE_W;

            draw_tile(y, x, C_BORDER, '#', NULL);
        }
    }

    /*
     * Draw only visible map cells.
     */
    for (int r = first_row; r <= last_row; r++) {
        for (int c = first_col; c <= last_col; c++) {
            uint16_t idx = make_cell_index(r, c, state->map.cols);

            char cell = state->map.cells[idx];
            if (state->overlay_map.cells[idx] != EMPTY) {
                cell = state->overlay_map.cells[idx];
            }

            int screen_r = r - first_row;
            int screen_c = c - first_col;

            int y = start_row + (screen_r + 1) * TILE_H;
            int x = start_col + (screen_c + 1) * TILE_W;

            draw_tile(y, x, color_for_cell(cell), fill_for_cell(cell), "");
        }
    }

    /*
     * Draw only players who are inside the visible viewport.
     */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].name[0] == '\0')
            continue;
        if (!state->players[i].alive)
            continue;

        int pr = state->players[i].row;
        int pc = state->players[i].col;

        if (pr < first_row || pr > last_row || pc < first_col || pc > last_col)
            continue;

        int screen_r = pr - first_row;
        int screen_c = pc - first_col;

        int y = start_row + (screen_r + 1) * TILE_H;
        int x = start_col + (screen_c + 1) * TILE_W;

        char label[4];
        snprintf(label, sizeof(label), "%u", state->players[i].id);

        draw_tile(y, x, i == state->my_id ? C_ME : C_PLAYER, ' ', label);
    }

    const char *hint = "WASD: kustēties   B: spridzināt   X: beigt spēli";
    mvprintw(rows - 2, (cols - (int)strlen(hint)) / 2, "%s", hint);

    refresh();
}

void draw_end(const client_state_t *state)
{
    erase();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    const char *title = "BOMBERMAN";
    mvprintw(1, (cols - (int)strlen(title)) / 2, "%s", title);

    attron(A_BOLD);

    if (state->winner_id == 255) {
        const char *draw_msg = "Spēle beigusies: neizšķirts!";
        mvprintw(3, (cols - (int)strlen(draw_msg)) / 2, "%s", draw_msg);
    } else if (state->winner_id < MAX_PLAYERS &&
               state->players[state->winner_id].name[0] != '\0') {
        char winner_msg[128];
        snprintf(winner_msg, sizeof(winner_msg),
                 "Spēle beigusies: uzvarēja %s!",
                 state->players[state->winner_id].name);

        mvprintw(3, (cols - (int)strlen(winner_msg)) / 2, "%s", winner_msg);
    } else {
        const char *unknown_msg = "Spēle beigusies!";
        mvprintw(3, (cols - (int)strlen(unknown_msg)) / 2, "%s", unknown_msg);
    }

    attroff(A_BOLD);

    int box_w = 48;
    int box_h = 9;
    int start_y = 6;
    int start_x = (cols - box_w) / 2;

    mvprintw(start_y,     start_x, "+----------------------------------------------+");
    mvprintw(start_y + 1, start_x, "|                 Tava statistika              |");
    mvprintw(start_y + 2, start_x, "+----------------------------------------------+");
    mvprintw(start_y + 3, start_x, "| Uzbombīti spēlētāji:            %-12u |", state->stats.kills);
    mvprintw(start_y + 4, start_x, "| Iznīcinātas  kastes:            %-12u |", state->stats.blocks_destroyed);
    mvprintw(start_y + 5, start_x, "| Savākti pārsteigumi:            %-12u |", state->stats.bonuses_collected);
    mvprintw(start_y + 6, start_x, "+----------------------------------------------+");

    const char *hint = "Spied R, lai sāktu no jauna   X, lai izietu";
    mvprintw(start_y + box_h, (cols - (int)strlen(hint)) / 2, "%s", hint);

    refresh();
}


void draw_map_select(const client_state_t *state)
{
    erase();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    const char *title = "BOMBERMAN";
    const char *subtitle = "Kartes izvēle";

    attron(A_BOLD);
    mvprintw(1, (cols - (int)strlen(title)) / 2, "%s", title);
    attroff(A_BOLD);

    mvprintw(3, (cols - (int)strlen(subtitle)) / 2, "%s", subtitle);

    int box_w = 76;
    int start_x = (cols - box_w) / 2;
    int start_y = 5;

    mvprintw(start_y,     start_x, "+---------------------------------------------------------------------------+");
    mvprintw(start_y + 1, start_x, "| ID | Karte                    | Izmērs  | Spēl. | Ātr. | Spr. | Rād. | T  |");
    mvprintw(start_y + 2, start_x, "+----+--------------------------+---------+-------+------+------+------+----+");

    int row = start_y + 3;

    for (uint8_t i = 0; i < state->map_choice_count; i++) {
        const client_map_choice_t *choice = &state->map_choices[i];

        if (choice->id == state->selected_map_id)
            attron(A_REVERSE);

        mvprintw(row, start_x,
                 "| %-2u | %-24.24s | %3ux%-3u | %-5u | %-4u | %-4u | %-4u | %-1u |",
                 choice->id,
                 choice->name,
                 choice->rows,
                 choice->cols,
                 choice->supported_players,
                 choice->player_speed,
                 choice->explosion_duration_ticks,
                 choice->explosion_radius,
                 choice->bomb_timer_ticks);

        if (choice->id == state->selected_map_id)
            attroff(A_REVERSE);

        row++;
    }

    mvprintw(row++, start_x, "+----+--------------------------+---------+-------+------+------+------+----+");

    row += 2;
    mvprintw(row++, start_x, "↑/↓: izvēlēties karti");
    mvprintw(row++, start_x, "Enter: apstiprināt karti");
    mvprintw(row++, start_x, "Esc: atpakaļ uz priekšnamu");

    refresh();
}

