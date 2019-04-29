#include "rand.h"
#include "time.h"
#include "utility.h"
#include "cpuid.h"

void lfsr113_seed(lfsr113_state_t *state, uint32_t seed)
{
    state->seed_z1 = seed;
    state->seed_z2 = seed;
    state->seed_z3 = seed;
    state->seed_z4 = seed;
}

void lfsr113_autoseed(lfsr113_state_t *state)
{
    lfsr113_seed(state, (uint32_t)nano_time());
}

uint32_t lfsr113_rand(lfsr113_state_t *state)
{
   unsigned int b;
   b  = ((state->seed_z1 << 6) ^ state->seed_z1) >> 13;
   state->seed_z1 = ((state->seed_z1 & 0xFFFFFFFEU) << 18) ^ b;
   b  = ((state->seed_z2 << 2) ^ state->seed_z2) >> 27;
   state->seed_z2 = ((state->seed_z2 & 0xFFFFFFF8U) << 2) ^ b;
   b  = ((state->seed_z3 << 13) ^ state->seed_z3) >> 21;
   state->seed_z3 = ((state->seed_z3 & 0xFFFFFFF0U) << 7) ^ b;
   b  = ((state->seed_z4 << 3) ^ state->seed_z4) >> 12;
   state->seed_z4 = ((state->seed_z4 & 0xFFFFFF80U) << 13) ^ b;
   return (state->seed_z1 ^ state->seed_z2 ^
           state->seed_z3 ^ state->seed_z4);
}

uint32_t rand_range(lfsr113_state_t *state, uint32_t st, uint32_t en)
{
    uint32_t n = lfsr113_rand(state);
    n %= (en - st);
    n += st;
    return n;
}

int rand_r(uint64_t *seed)
{
    return (int)((*seed = *seed *
            6364136223846793005ULL + 1U) >> 33);
}

int rand_r_range(uint64_t *seed, int min, int max)
{
    return (((int64_t)rand_r(seed) * (max-min)) >> 31) + min;
}

void c4rand::seed(void const *data, size_t len)
{
    for (size_t i = 0; i < 256; ++i)
        state[i] = i;

    b = 0;

    write(data, len);
}

void c4rand::write(void const *data, size_t len)
{
    uint8_t const *key = (uint8_t const *)data;

    bool done = false;

    for (size_t i = 0, ki = 0; i || !done; ++i, ++ki) {
        if (ki >= len) {
            ki = 0;
            done = true;
        }

        if (i >= 256)
            i = 0;

        b = (b + state[i] + key[ki]) & 0xFF;
        std::swap(state[i], state[b]);
    }

    a = 0;
}

void c4rand::read(void *data, size_t len)
{
    uint8_t *k = (uint8_t*)data;

    for (size_t i = 0; i < len; ++i) {
        a = (a + 1) & 0xFF;
        b = (b + state[a]) & 0xFF;
        std::swap(state[a], state[b]);
        k[i] ^= state[(state[a] + state[b]) & 0xFF];
    }
}
