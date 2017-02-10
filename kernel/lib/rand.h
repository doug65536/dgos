#pragma once

#include "types.h"

int rand_r(uint64_t *seed);
int rand_r_range(uint64_t *seed, int min, int max);

void lfsr113_autoseed(void);
void lfsr113_seed(uint32_t seed);
uint32_t lfsr113_rand(void);
uint32_t rand_range(uint32_t st, uint32_t en);
