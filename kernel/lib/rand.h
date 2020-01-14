#pragma once

#include "types.h"

__BEGIN_DECLS

int rand_r(uint64_t *seed);
int rand_r_range(uint64_t *seed, int min, int max);

struct lfsr113_state_t {
    uint32_t seed_z1 = 12345;
    uint32_t seed_z2 = 12345;
    uint32_t seed_z3 = 12345;
    uint32_t seed_z4 = 12345;
};

void lfsr113_autoseed(lfsr113_state_t *state);
void lfsr113_seed(lfsr113_state_t *state, uint32_t seed);
uint32_t lfsr113_rand(lfsr113_state_t *state);

class rand_lfs113_t : public lfsr113_state_t {
public:
    uint32_t lfsr113_rand()
    {
       unsigned b;

       b  = ((seed_z1 << 6) ^ seed_z1) >> 13;
       seed_z1 = ((seed_z1 & 0xFFFFFFFEU) << 18) ^ b;

       b  = ((seed_z2 << 2) ^ seed_z2) >> 27;
       seed_z2 = ((seed_z2 & 0xFFFFFFF8U) << 2) ^ b;

       b  = ((seed_z3 << 13) ^ seed_z3) >> 21;
       seed_z3 = ((seed_z3 & 0xFFFFFFF0U) << 7) ^ b;

       b  = ((seed_z4 << 3) ^ seed_z4) >> 12;
       seed_z4 = ((seed_z4 & 0xFFFFFF80U) << 13) ^ b;

       return seed_z1 ^ seed_z2 ^ seed_z3 ^ seed_z4;
    }
};

class padded_rand_lfs113_t : public rand_lfs113_t {
    uint32_t padding[12];
};

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

__END_DECLS
