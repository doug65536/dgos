#pragma once

#include "types.h"

int rand_r(uint64_t *seed);
int rand_r_range(uint64_t *seed, int min, int max);

struct lfsr113_state_t {
    uint32_t seed_z1;
    uint32_t seed_z2;
    uint32_t seed_z3;
    uint32_t seed_z4;
};

void lfsr113_autoseed(lfsr113_state_t *state);
void lfsr113_seed(lfsr113_state_t *state, uint32_t seed);
uint32_t lfsr113_rand(lfsr113_state_t *state);
uint32_t rand_range(lfsr113_state_t *state, uint32_t st, uint32_t en);

class alignas(64) c4rand {
public:
    void seed(void const *data, size_t len);

    // Feed entropy
    void write(void const *data, size_t len);

    // Get random values
    void read(void *data, size_t len);

private:
    uint8_t state[256];
    unsigned a;
    unsigned b;
};
