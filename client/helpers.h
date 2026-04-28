#pragma once
#include "client.h"

uint64_t get_monotonic_time_ms(void);
uint32_t client_estimated_tick(const client_state_t *state);
void mark_explosion(client_state_t *state, uint16_t center, uint8_t radius, uint8_t value);
