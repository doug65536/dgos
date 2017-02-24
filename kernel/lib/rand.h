#pragma once

#include "types.h"

int rand_r(uint64_t *seed);
int rand_r_range(uint64_t *seed, int min, int max);

typedef struct lfsr113_state_t {
    uint32_t seed_z1;
    uint32_t seed_z2;
    uint32_t seed_z3;
    uint32_t seed_z4;
} lfsr113_state_t;

void lfsr113_autoseed(lfsr113_state_t *state);
void lfsr113_seed(lfsr113_state_t *state, uint32_t seed);
uint32_t lfsr113_rand(lfsr113_state_t *state);
uint32_t rand_range(lfsr113_state_t *state, uint32_t st, uint32_t en);
