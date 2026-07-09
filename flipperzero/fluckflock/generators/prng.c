/*
 * prng.c — xoshiro256** PRNG, seeded via splitmix64.
 *
 * Pure C. No Flipper SDK dependency. Host-testable.
 * Same seed → same sequence (deterministic).
 *
 * References:
 *   xoshiro256**: https://prng.di.unimi.it/xoshiro256starstar.c
 *   splitmix64:   https://prng.di.unimi.it/splitmix64.c
 */

#include "prng.h"

#include <stdlib.h>
#include <string.h>

/* ---- Internal state ---- */

struct ChaffRng {
    uint64_t s[4]; /* xoshiro256** state — must never be all-zero */
};

/* ---- splitmix64: used only during seed expansion ---- */

static uint64_t splitmix64(uint64_t* x) {
    uint64_t z = (*x += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

/* ---- xoshiro256** core ---- */

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t xoshiro256ss_next(uint64_t s[4]) {
    const uint64_t result = rotl64(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);
    return result;
}

/* ---- Public API ---- */

ChaffRng* chaff_rng_alloc(void) {
    ChaffRng* rng = (ChaffRng*)malloc(sizeof(ChaffRng));
    if(rng) {
        /* Default seed: arbitrary non-zero constant so alloc+use works without an explicit seed */
        chaff_rng_seed(rng, UINT64_C(0xdeadbeefcafe1234));
    }
    return rng;
}

void chaff_rng_free(ChaffRng* rng) {
    free(rng);
}

void chaff_rng_seed(ChaffRng* rng, uint64_t seed) {
    /* Expand 64-bit seed to 256-bit state with splitmix64.
     * splitmix64 guarantees non-zero output even from seed=0. */
    rng->s[0] = splitmix64(&seed);
    rng->s[1] = splitmix64(&seed);
    rng->s[2] = splitmix64(&seed);
    rng->s[3] = splitmix64(&seed);
}

uint32_t chaff_rng_u32(ChaffRng* rng) {
    /* Use the upper 32 bits of the 64-bit output — higher-quality bits */
    return (uint32_t)(xoshiro256ss_next(rng->s) >> 32);
}

void chaff_rng_bytes(ChaffRng* rng, uint8_t* buf, size_t len) {
    size_t i = 0;
    while(i < len) {
        uint64_t word = xoshiro256ss_next(rng->s);
        size_t chunk = (len - i < 8) ? (len - i) : 8;
        memcpy(buf + i, &word, chunk);
        i += chunk;
    }
}

uint32_t chaff_rng_bounded(ChaffRng* rng, uint32_t upper_bound) {
    /*
     * Lemire's nearly-divisionless algorithm — no modulo bias.
     * D. Lemire, "Fast Random Integer Generation in an Interval", 2018.
     */
    if(upper_bound <= 1) return 0;

    uint64_t threshold = ((uint64_t)(UINT32_MAX - upper_bound + 1)) % upper_bound;
    for(;;) {
        uint64_t r = chaff_rng_u32(rng);
        uint64_t m = r * (uint64_t)upper_bound;
        if((m & UINT32_MAX) >= threshold) {
            return (uint32_t)(m >> 32);
        }
    }
}
