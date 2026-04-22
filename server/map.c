#include "map.h"
#include "stdio.h"

int load_map(const char *filename, map_t *map)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    // 1. read first line with configs
    fscanf(f, "%hhu %hhu %hu %hu %hhu %hu",
           &map->rows,
           &map->cols,
           &map->configs.player_speed,
           &map->configs.explosion_duration_ticks,
           &map->configs.explosion_radius,
           &map->configs.bomb_timer_ticks);

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
                map->configs.start_row[player_idx] = r;
                map->configs.start_col[player_idx] = c;
                map->cells[make_cell_index(r, c, map->cols)] = EMPTY;
            }
        }
    }

    fclose(f);
    return 0;
}