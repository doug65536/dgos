#include "rand.h"
#include "time.h"
#include "utility.h"
#include "cpuid.h"
#include "export.h"
#include "likely.h"

EXPORT void lfsr113_seed(lfsr113_state_t *state, uint32_t seed)
{
    /**** VERY IMPORTANT **** :
      The initial seeds z1, z2, z3, z4  MUST be larger than
      1, 7, 15, and 127 respectively.
    ****/

    uint64_t seed_seed = uint64_t(seed) << 16;

    state->seed_z1 = rand_r(&seed_seed);
    state->seed_z2 = rand_r(&seed_seed);
    state->seed_z3 = rand_r(&seed_seed);
    state->seed_z4 = rand_r(&seed_seed);

    if (state->seed_z1 < 1)
        state->seed_z1 = 71;
    if (state->seed_z2 < 7)
        state->seed_z2 += 61;
    if (state->seed_z3 < 15)
        state->seed_z3 += 93;
    if (state->seed_z4 < 127)
        state->seed_z4 += 293;
}

EXPORT void lfsr113_autoseed(lfsr113_state_t *state)
{
    lfsr113_seed(state, (uint32_t)nano_time());
}

EXPORT uint32_t lfsr113_rand(lfsr113_state_t *state)
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

EXPORT uint32_t rand_range(lfsr113_state_t *state, uint32_t st, uint32_t en)
{
    uint32_t n = en - st;

    // Find highest random number that is an even multiple of the range needed
    uint32_t limit = (UINT64_C(1) << 32) - (UINT64_C(1) << 32) % n;

    for (;;) {
        uint32_t r = lfsr113_rand(state);

        if (likely(r < limit || !limit)) {
            r %= n;
            r += st;
            return r;
        }

        // Unlikely case that random number fell in last partial range
        // try again. Chance of looping is very low.
    }
}

EXPORT int rand_r(uint64_t *seed)
{
    return (int)((*seed = *seed *
            6364136223846793005ULL + 1U) >> 33);
}

EXPORT int rand_r_range(uint64_t *seed, int min, int max)
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

    // Keep going until we are both done and at index 0
    for (size_t ki = 0, i = 0; (i &= 0xFF) || !done; ++i) {
        b = (b + state[i] + key[ki]) & 0xFF;
        std::swap(state[i], state[b]);

        // If we are at the end of the key, wrap around
        if (++ki >= len) {
            done = true;
            ki = 0;
        }
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
