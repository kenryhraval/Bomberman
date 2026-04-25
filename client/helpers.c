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

            // explosion stops at hard walls
            if (state->map.cells[idx] == HARD_BLOCK)
                break;

            state->overlay_map.cells[idx] = value;

            // explosion stops after soft block
            if (state->map.cells[idx] == SOFT_BLOCK)
                break;
        }
    }
}