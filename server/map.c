#include "map.h"
#include "server.h"
#include "stdio.h"

int load_map(const char *filename, server_state_t *server_state)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    // 1. read first line with configs
    fscanf(f, "%hhu %hhu %hu %hu %hhu %hu",
           &server_state->map.rows,
           &server_state->map.cols,
           &server_state->config.player_speed,
           &server_state->config.explosion_duration_ticks,
           &server_state->config.explosion_radius,
           &server_state->config.bomb_timer_ticks);

    // 2. read each cell and save player start positions
    for (int r = 0; r < server_state->map.rows; r++)
    {
        for (int c = 0; c < server_state->map.cols; c++)
        {
            char cell;
            fscanf(f, " %c", &cell); // space before %c to skip any whitespace
            server_state->map.cells[make_cell_index(r, c, server_state->map.cols)] = cell;

            // if bonus cell, add to bonuses array
            if (cell == BONUS_BOMB_COUNT ||
                cell == BONUS_RADIUS ||
                cell == BONUS_SPEED ||
                cell == BONUS_TIMER)
            {
                bonus_t *new_bonuses = realloc(server_state->bonuses, (server_state->bonus_count + 1) * sizeof(bonus_t));
                if (new_bonuses == NULL)
                {
                    perror("Failed to allocate memory for bonuses");
                    exit(EXIT_FAILURE);
                }
                server_state->bonuses = new_bonuses;
                server_state->bonuses[server_state->bonus_count].active = true;
                server_state->bonuses[server_state->bonus_count].row = r;
                server_state->bonuses[server_state->bonus_count].col = c;
                server_state->bonuses[server_state->bonus_count].type = (bonus_type_t)cell;
                server_state->bonus_count++;
                server_state->bonus_count++;
            }

            // if player position, save it to configs and set cell to empty
            if (cell >= PLAYER_1 && cell <= PLAYER_LAST)
            {
                int player_idx = cell - PLAYER_1;
                server_state->config.start_row[player_idx] = r;
                server_state->config.start_col[player_idx] = c;
                server_state->map.cells[make_cell_index(r, c, server_state->map.cols)] = EMPTY;
            }
        }
    }

    fclose(f);
    return 0;
}