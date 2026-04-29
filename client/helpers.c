#define _POSIX_C_SOURCE 200809L

#include <time.h>

#include "helpers.h"

void mark_explosion(client_state_t *state, uint16_t center, uint8_t radius, uint8_t value)
{
    uint16_t row = center / state->map.cols;
    uint16_t col = center % state->map.cols;

    state->overlay_map.cells[center] = value;

    int dirs[4][2] = {
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1}
    };

    for (int d = 0; d < 4; d++) {
        for (int step = 1; step <= radius; step++) {
            int r = row + dirs[d][0] * step;
            int c = col + dirs[d][1] * step;

            if (r < 0 || r >= state->map.rows ||
                c < 0 || c >= state->map.cols)
                break;

            uint16_t idx = make_cell_index(r, c, state->map.cols);

            // Sprādziens apstājas pie nesalaužamās sienas
            if (state->map.cells[idx] == HARD_BLOCK)
                break;

            state->overlay_map.cells[idx] = value;

            // Sprādziens vēl sasniedz mīksto bloku, bet tālāk neturpinās
            if (state->map.cells[idx] == SOFT_BLOCK)
                break;
        }
    }
}

uint64_t get_monotonic_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}


uint32_t client_estimated_tick(const client_state_t *state)
{
    if (state->game_status != GAME_RUNNING)
        return state->current_tick;

    if (state->last_tick_time_ms == 0)
        return state->current_tick;

    uint64_t now_ms = get_monotonic_time_ms();
    uint64_t elapsed_ms = now_ms - state->last_tick_time_ms;

    uint64_t elapsed_ticks = (elapsed_ms * TICK_RATE) / 1000;

    return state->current_tick + (uint32_t)elapsed_ticks;
}
