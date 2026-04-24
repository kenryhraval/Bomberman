#include "map.h"
#include "server.h"
#include "stdio.h"

int load_map(const char *filename, map_t *map, config_t *config)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    // 1. read first line with configs
    fscanf(f, "%hhu %hhu %hu %hu %hhu %hu",
           &map->rows,
           &map->cols,
           &config->player_speed,
           &config->explosion_duration_ticks,
           &config->explosion_radius,
           &config->bomb_timer_ticks);

    // 2. read each cell and save player start positions
    for (int r = 0; r < map->rows; r++)
    {
        for (int c = 0; c < map->cols; c++)
        {
            char cell;
            fscanf(f, " %c", &cell); // space before %c to skip any whitespace
            map->cells[make_cell_index(r, c, map->cols)] = cell;

            // if player position, save it to configs and set cell to empty
            if (cell >= PLAYER_1 && cell <= PLAYER_LAST)
            {
                int player_idx = cell - PLAYER_1;
                config->start_row[player_idx] = r;
                config->start_col[player_idx] = c;
                map->cells[make_cell_index(r, c, map->cols)] = EMPTY;
            }
        }
    }

    fclose(f);
    return 0;
}