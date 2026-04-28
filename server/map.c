#include "map.h"
#include "server.h"
#include "stdio.h"

#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int load_map(const char *filename, server_state_t *server_state)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    // reset map-related runtime state
    server_state->bonus_count = 0;
    memset(server_state->bonuses, 0, sizeof(server_state->bonuses));
    memset(&server_state->map, 0, sizeof(server_state->map));
    memset(&server_state->config, 0, sizeof(server_state->config));

    // 1. read first line with configs
    if (fscanf(f, "%hhu %hhu %hu %hu %hhu %hu",
               &server_state->map.rows,
               &server_state->map.cols,
               &server_state->config.player_speed,
               &server_state->config.explosion_duration_ticks,
               &server_state->config.explosion_radius,
               &server_state->config.bomb_timer_ticks) != 6)
    {
        fclose(f);
        return -1;
    }

    // 2. read each cell and save player start positions
    for (int r = 0; r < server_state->map.rows; r++)
    {
        for (int c = 0; c < server_state->map.cols; c++)
        {
            char cell;
            if (fscanf(f, " %c", &cell) != 1)
            {
                fclose(f);
                return -1;
            }

            uint16_t cell_idx = make_cell_index(r, c, server_state->map.cols);
            server_state->map.cells[cell_idx] = cell;

            // if bonus cell, add to bonuses array
            if (cell == SPEED_BONUS ||
                cell == BOMB_RADIUS_BONUS ||
                cell == BOMB_TIMER_BONUS ||
                cell == BOMB_COUNT_BONUS)
            {
                if (server_state->bonus_count < MAX_BONUSES)
                {
                    bonus_t *bonus = &server_state->bonuses[server_state->bonus_count];

                    bonus->row = r;
                    bonus->col = c;
                    bonus->type = (bonus_type_t)cell;

                    server_state->bonus_count++;
                }
                else
                {
                    printf("Too many bonuses in map, ignoring bonus at row=%d col=%d\n", r, c);
                    server_state->map.cells[cell_idx] = EMPTY;
                }
            }

            // if player position, save it to configs and set cell to empty
            if (cell >= PLAYER_1 && cell <= PLAYER_LAST)
            {
                int player_idx = cell - PLAYER_1;

                if (player_idx >= 0 && player_idx < MAX_PLAYERS)
                {
                    server_state->config.start_row[player_idx] = r;
                    server_state->config.start_col[player_idx] = c;
                    server_state->map.cells[cell_idx] = EMPTY;
                }
            }
        }
    }

    fclose(f);
    return 0;
}


static int has_txt_suffix(const char *name)
{
    size_t len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".txt") == 0;
}

static void copy_map_name(char *dst, size_t dst_size, const char *filename)
{
    snprintf(dst, dst_size, "%s", filename);

    char *dot = strrchr(dst, '.');
    if (dot != NULL)
        *dot = '\0';
}

static int parse_map_metadata(const char *path, server_map_choice_t *choice)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return -1;

    unsigned rows, cols;
    unsigned player_speed;
    unsigned explosion_duration_ticks;
    unsigned explosion_radius;
    unsigned bomb_timer_ticks;

    if (fscanf(fp, "%u %u %u %u %u %u",
               &rows,
               &cols,
               &player_speed,
               &explosion_duration_ticks,
               &explosion_radius,
               &bomb_timer_ticks) != 6)
    {
        fclose(fp);
        return -1;
    }

    if (rows == 0 || rows > 255 || cols == 0 || cols > 255) {
        fclose(fp);
        return -1;
    }

    int seen_players[MAX_PLAYERS + 1] = {0};
    int ch;

    while ((ch = fgetc(fp)) != EOF) {
        if (ch >= '1' && ch <= '8') {
            seen_players[ch - '0'] = 1;
        }
    }

    fclose(fp);

    uint8_t player_count = 0;
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        if (seen_players[i])
            player_count++;
    }

    choice->rows = (uint8_t)rows;
    choice->cols = (uint8_t)cols;
    choice->player_speed = (uint16_t)player_speed;
    choice->explosion_duration_ticks = (uint16_t)explosion_duration_ticks;
    choice->explosion_radius = (uint8_t)explosion_radius;
    choice->bomb_timer_ticks = (uint16_t)bomb_timer_ticks;
    choice->supported_players = player_count;

    return 0;
}

size_t scan_map_choices(const char *maps_dir, server_map_choice_t choices[], size_t max_choices)
{
    DIR *dir = opendir(maps_dir);
    if (dir == NULL) {
        perror("opendir maps");
        return 0;
    }

    size_t count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && count < max_choices) {
        if (entry->d_name[0] == '.')
            continue;

        if (!has_txt_suffix(entry->d_name))
            continue;

        server_map_choice_t choice = {0};

        int written = snprintf(choice.path, sizeof(choice.path), "%s/%s", maps_dir, entry->d_name);

        if (written < 0 || written >= (int)sizeof(choice.path)) {
            printf("Skipping map file with too long path: %s/%s\n",
                maps_dir, entry->d_name);
            continue;
        }

        copy_map_name(choice.name, sizeof(choice.name), entry->d_name);

        if (parse_map_metadata(choice.path, &choice) < 0) {
            printf("Skipping invalid map file: %s\n", choice.path);
            continue;
        }

        choices[count++] = choice;
    }

    closedir(dir);
    return count;
}


int find_available_map_choices_and_send(server_state_t *state, int fd, uint8_t target_id)
{
    if (state->map_choice_count == 0) 
        state->map_choice_count = scan_map_choices(MAPS_DIR, state->map_choices, MAX_MAP_CHOICES);

    msg_map_choices_t choices_msg = {0};
    choices_msg.count = (uint8_t)state->map_choice_count;

    for (size_t i = 0; i < state->map_choice_count; i++)
    {
        server_map_choice_t *src = &state->map_choices[i];
        client_map_choice_t *dst = &choices_msg.entries[i];

        dst->id = (uint8_t)i;
        snprintf(dst->name, sizeof(dst->name), "%s", src->name);
        dst->rows = src->rows;
        dst->cols = src->cols;
        dst->supported_players = src->supported_players;
        dst->player_speed = htons(src->player_speed);
        dst->explosion_duration_ticks = htons(src->explosion_duration_ticks);
        dst->explosion_radius = src->explosion_radius;
        dst->bomb_timer_ticks = htons(src->bomb_timer_ticks);
    }

    return send_map_choices(fd, SERVER, target_id, &choices_msg);
}
