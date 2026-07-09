/*
 * test_prng.c — Host-side unit tests for the FluckFlock PRNG (ChaffRng).
 *
 * Contract under test: generators/prng.h
 *   chaff_rng_alloc / chaff_rng_free
 *   chaff_rng_seed(rng, uint64_t seed)
 *   chaff_rng_u32(rng) → uint32_t
 *   chaff_rng_bytes(rng, buf, len)
 *   chaff_rng_bounded(rng, upper_bound) → uint32_t in [0, upper_bound)
 *
 * Build (from test/):
 *   cc -std=c11 -Wall -I../flipperzero/fluckflock/generators \
 *      -o test_prng test_prng.c ../flipperzero/fluckflock/generators/prng.c
 *
 * Run:  ./test_prng
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "test_util.h"
#include "../flipperzero/fluckflock/generators/prng.h"

/* -----------------------------------------------------------------------
 * Determinism: same seed → identical u32 sequence
 * Two independent RNG instances seeded identically must produce the same
 * values at every step.  This is the core guarantee that makes test results
 * reproducible.
 * --------------------------------------------------------------------- */
static void test_determinism_u32(void) {
    ChaffRng *a = chaff_rng_alloc();
    ChaffRng *b = chaff_rng_alloc();
    chaff_rng_seed(a, 0xDEADBEEFCAFEBABEULL);
    chaff_rng_seed(b, 0xDEADBEEFCAFEBABEULL);
    for (int i = 0; i < 1000; i++) {
        uint32_t va = chaff_rng_u32(a);
        uint32_t vb = chaff_rng_u32(b);
        ASSERT_EQ(va, vb);
    }
    chaff_rng_free(a);
    chaff_rng_free(b);
    PASS("determinism_u32");
}

/* -----------------------------------------------------------------------
 * Determinism: same seed → identical byte sequences
 * --------------------------------------------------------------------- */
static void test_determinism_bytes(void) {
    uint8_t ba[64], bb[64];
    ChaffRng *a = chaff_rng_alloc();
    ChaffRng *b = chaff_rng_alloc();
    chaff_rng_seed(a, 42ULL);
    chaff_rng_seed(b, 42ULL);
    chaff_rng_bytes(a, ba, sizeof(ba));
    chaff_rng_bytes(b, bb, sizeof(bb));
    ASSERT(memcmp(ba, bb, sizeof(ba)) == 0);
    chaff_rng_free(a);
    chaff_rng_free(b);
    PASS("determinism_bytes");
}

/* -----------------------------------------------------------------------
 * Determinism: re-seeding with the same seed restarts the same sequence
 * --------------------------------------------------------------------- */
static void test_reseed_restarts_sequence(void) {
    ChaffRng *rng = chaff_rng_alloc();
    uint32_t first[20], second[20];
    chaff_rng_seed(rng, 0xBEEF0001ULL);
    for (int i = 0; i < 20; i++) first[i]  = chaff_rng_u32(rng);
    chaff_rng_seed(rng, 0xBEEF0001ULL);
    for (int i = 0; i < 20; i++) second[i] = chaff_rng_u32(rng);
    ASSERT(memcmp(first, second, sizeof(first)) == 0);
    chaff_rng_free(rng);
    PASS("reseed_restarts_sequence");
}

/* -----------------------------------------------------------------------
 * Different seeds → different sequences
 * The probability of 100 consecutive matching u32 values with two truly
 * independent PRNG states is astronomically small; any reasonable PRNG
 * passes this trivially.
 * --------------------------------------------------------------------- */
static void test_different_seeds_diverge(void) {
    ChaffRng *a = chaff_rng_alloc();
    ChaffRng *b = chaff_rng_alloc();
    chaff_rng_seed(a, 1ULL);
    chaff_rng_seed(b, 2ULL);
    int same_count = 0;
    for (int i = 0; i < 100; i++) {
        if (chaff_rng_u32(a) == chaff_rng_u32(b)) same_count++;
    }
    /* Allow a tiny number of coincidental matches but not all (or most). */
    ASSERT(same_count < 5);
    chaff_rng_free(a);
    chaff_rng_free(b);
    PASS("different_seeds_diverge");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bounded: every returned value is in [0, N)
 * Tested across a range of small upper bounds.
 * --------------------------------------------------------------------- */
static void test_bounded_range(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 99ULL);
    static const uint32_t bounds[] = { 2, 3, 5, 7, 10, 13, 16, 64, 100 };
    for (size_t bi = 0; bi < sizeof(bounds) / sizeof(bounds[0]); bi++) {
        uint32_t N = bounds[bi];
        for (int i = 0; i < 5000; i++) {
            uint32_t v = chaff_rng_bounded(rng, N);
            ASSERT(v < N);
        }
    }
    chaff_rng_free(rng);
    PASS("bounded_range");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bounded(rng, 1) must always return 0 — only valid value.
 * --------------------------------------------------------------------- */
static void test_bounded_one_always_zero(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 7ULL);
    for (int i = 0; i < 2000; i++) {
        ASSERT_EQ(chaff_rng_bounded(rng, 1), (uint32_t)0);
    }
    chaff_rng_free(rng);
    PASS("bounded_one_always_zero");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bounded: rough uniformity over many samples.
 * For N=4 and 20 000 samples, every bucket should receive between 50 % and
 * 150 % of the expected count (5 000).  This is a very wide band — it
 * catches pathological bias (e.g. bucket 0 always wins) while tolerating
 * any reasonable distribution.
 * --------------------------------------------------------------------- */
static void test_bounded_uniformity(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 12345ULL);
    const uint32_t N       = 4;
    const int      samples = 20000;
    uint32_t counts[4]     = { 0 };
    for (int i = 0; i < samples; i++) {
        counts[chaff_rng_bounded(rng, N)]++;
    }
    const uint32_t expected = (uint32_t)(samples / N); /* 5 000 */
    for (uint32_t i = 0; i < N; i++) {
        ASSERT(counts[i] >= expected / 2);             /* >= 2 500 */
        ASSERT(counts[i] <= expected * 3 / 2);         /* <= 7 500 */
    }
    chaff_rng_free(rng);
    PASS("bounded_uniformity");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bytes: fills exactly `len` bytes; does not write beyond.
 * Strategy: surround the target region with 0xAA sentinel bytes, call
 * chaff_rng_bytes for the middle region, then verify the sentinels are
 * untouched.  Also verify the filled region is not still all-0xAA
 * (astronomically unlikely for any real PRNG with len >= 8).
 * --------------------------------------------------------------------- */
static void test_bytes_exact_fill(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0xABCDULL);

    uint8_t buf[80];
    const size_t offset   = 8;   /* leading sentinels */
    const size_t fill_len = 32;  /* region to fill */

    memset(buf, 0xAA, sizeof(buf));
    chaff_rng_bytes(rng, buf + offset, fill_len);

    /* Leading sentinel region must be untouched. */
    for (size_t i = 0; i < offset; i++) {
        ASSERT_EQ(buf[i], (uint8_t)0xAA);
    }
    /* Trailing sentinel region must be untouched. */
    for (size_t i = offset + fill_len; i < sizeof(buf); i++) {
        ASSERT_EQ(buf[i], (uint8_t)0xAA);
    }
    /* Filled region should not be entirely 0xAA (astronomically unlikely). */
    int unchanged = 0;
    for (size_t i = offset; i < offset + fill_len; i++) {
        if (buf[i] == 0xAA) unchanged++;
    }
    ASSERT(unchanged < (int)fill_len - 1);

    chaff_rng_free(rng);
    PASS("bytes_exact_fill");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bytes with len==0 must be a no-op (no crash, no writes).
 * --------------------------------------------------------------------- */
static void test_bytes_zero_length(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 1ULL);
    uint8_t buf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    chaff_rng_bytes(rng, buf, 0);
    for (int i = 0; i < 4; i++) ASSERT_EQ(buf[i], (uint8_t)0xFF);
    chaff_rng_free(rng);
    PASS("bytes_zero_length");
}

/* -----------------------------------------------------------------------
 * chaff_rng_bytes: two consecutive calls with different effective lengths
 * produce different output (output depends on internal state progression).
 * --------------------------------------------------------------------- */
static void test_bytes_output_varies(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0xF00DULL);
    uint8_t a[16], b[16];
    chaff_rng_bytes(rng, a, sizeof(a));
    chaff_rng_bytes(rng, b, sizeof(b));
    ASSERT(memcmp(a, b, sizeof(a)) != 0);
    chaff_rng_free(rng);
    PASS("bytes_output_varies");
}

/* -----------------------------------------------------------------------
 * alloc + seed + free cycle: no crash, no leak (leak detection is external;
 * this test just exercises the lifecycle).
 * --------------------------------------------------------------------- */
static void test_alloc_free_lifecycle(void) {
    for (int i = 0; i < 50; i++) {
        ChaffRng *rng = chaff_rng_alloc();
        ASSERT(rng != NULL);
        chaff_rng_seed(rng, (uint64_t)i * 0x9E3779B97F4A7C15ULL);
        (void)chaff_rng_u32(rng);
        chaff_rng_free(rng);
    }
    PASS("alloc_free_lifecycle");
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */
int main(void) {
    printf("=== test_prng ===\n");
    test_alloc_free_lifecycle();
    test_determinism_u32();
    test_determinism_bytes();
    test_reseed_restarts_sequence();
    test_different_seeds_diverge();
    test_bounded_range();
    test_bounded_one_always_zero();
    test_bounded_uniformity();
    test_bytes_exact_fill();
    test_bytes_zero_length();
    test_bytes_output_varies();
    printf("All PRNG tests PASSED (%d assertions checked).\n", g_assert_count);
    return 0;
}
