#include "rand.h"

static uint32_t seed_z1 = 12345;
static uint32_t seed_z2 = 12345;
static uint32_t seed_z3 = 12345;
static uint32_t seed_z4 = 12345;

void lfsr113_seed(uint32_t seed)
{
    seed_z1 = seed;
    seed_z2 = seed;
    seed_z3 = seed;
    seed_z4 = seed;
}

uint32_t lfsr113_rand()
{

   unsigned int b;
   b  = ((seed_z1 << 6) ^ seed_z1) >> 13;
   seed_z1 = ((seed_z1 & 0xFFFFFFFEU) << 18) ^ b;
   b  = ((seed_z2 << 2) ^ seed_z2) >> 27;
   seed_z2 = ((seed_z2 & 0xFFFFFFF8U) << 2) ^ b;
   b  = ((seed_z3 << 13) ^ seed_z3) >> 21;
   seed_z3 = ((seed_z3 & 0xFFFFFFF0U) << 7) ^ b;
   b  = ((seed_z4 << 3) ^ seed_z4) >> 12;
   seed_z4 = ((seed_z4 & 0xFFFFFF80U) << 13) ^ b;
   return (seed_z1 ^ seed_z2 ^ seed_z3 ^ seed_z4);
}

uint32_t rand_range(uint32_t st, uint32_t en)
{
    uint32_t n = lfsr113_rand();
    n %= (en - st);
    n += st;
    return n;
}
