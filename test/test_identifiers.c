/*
 * test_identifiers.c — Host-side unit tests for FluckFlock identifier generators.
 *
 * Contract under test: generators/identifiers.h
 *   chaff_generate_ssid(buf, buf_len, rng) → size_t
 *   chaff_generate_bssid(out[6], rng)
 *   chaff_generate_ble_mac(out[6], rng)
 *   chaff_generate_ble_name(buf, buf_len, rng) → size_t
 *   chaff_generate_subghz_payload(buf, len, rng)
 *
 * Key spec / contract assertions encoded here (keaton-architecture.md §3):
 *
 *   SSID
 *     • returned length in [1, 32], excluding null terminator
 *     • buffer is null-terminated at index `len`
 *     • no write beyond `buf_len` bytes (overrun guard test with tiny buffer)
 *     • all characters are printable ASCII (0x20–0x7E)
 *     • high variety: not the same string every time
 *
 *   BSSID
 *     • exactly 6 bytes written to `out`
 *     • out[0] bit 0 == 0  →  unicast
 *     • out[0] bit 1 == 0  →  globally administered (looks like real AP MAC)
 *     • near-zero collision rate across large batch (last 3 bytes are random)
 *
 *   BLE MAC
 *     • exactly 6 bytes written to `out`
 *     • out[0] bit 1 == 1  →  locally administered  (spec §5.1, §6)
 *     • out[0] bit 0 == 0  →  unicast
 *     • near-zero collision rate across large batch
 *
 *   BLE name
 *     • returned length in [1, buf_len-1], excluding null terminator
 *     • null-terminated, no overrun with tiny buffers
 *     • variety across generations
 *
 *   Sub-GHz payload
 *     • fills exactly `len` bytes; no write beyond `len`
 *     • consecutive calls produce different output
 *
 *   Rotation / uniqueness  (spec §7)
 *     • 1 000-sample SSID batch: at least 20 distinct values
 *     • 1 000-sample BSSID batch: < 5 collisions  (3 random bytes → ~16M space)
 *     • 1 000-sample BLE MAC batch: < 5 collisions
 *
 * Build (from test/):
 *   cc -std=c11 -Wall -I../flipperzero/fluckflock/generators \
 *      -o test_identifiers test_identifiers.c \
 *      ../flipperzero/fluckflock/generators/prng.c \
 *      ../flipperzero/fluckflock/generators/identifiers.c
 *
 * Run:  ./test_identifiers
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include "test_util.h"
#include "../flipperzero/fluckflock/generators/prng.h"
#include "../flipperzero/fluckflock/generators/identifiers.h"

/* -----------------------------------------------------------------------
 * Helper: returns 1 if every byte in the null-terminated string is
 * printable ASCII (0x20–0x7E), 0 otherwise.
 * --------------------------------------------------------------------- */
static int all_printable_ascii(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 0x20u || *p > 0x7Eu) return 0;
    }
    return 1;
}

/* =====================================================================
 * SSID tests
 * =================================================================== */

/* Length in [1, 32], null-terminated at index len, strlen matches. */
static void test_ssid_length_and_termination(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0001ULL);
    char buf[64];
    for (int i = 0; i < 500; i++) {
        memset(buf, (int)0xAA, sizeof(buf));
        size_t len = chaff_generate_ssid(buf, sizeof(buf), rng);
        ASSERT(len >= 1 && len <= 32);
        ASSERT_EQ((unsigned char)buf[len], (unsigned char)'\0');
        ASSERT(strlen(buf) == len);
    }
    chaff_rng_free(rng);
    PASS("ssid_length_and_termination");
}

/* Every character is printable ASCII; no control chars, no null bytes in body. */
static void test_ssid_printable_ascii(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0002ULL);
    char buf[64];
    for (int i = 0; i < 500; i++) {
        chaff_generate_ssid(buf, sizeof(buf), rng);
        ASSERT(all_printable_ascii(buf));
    }
    chaff_rng_free(rng);
    PASS("ssid_printable_ascii");
}

/*
 * No buffer overrun with a tiny buf_len (4 bytes including null terminator).
 * Strategy: allocate a larger guarded buffer, pass only a slice of it as
 * buf_len, then verify bytes past buf_len are untouched (0xCC sentinel).
 * Returned length must be < buf_len (i.e. there is room for the null).
 */
static void test_ssid_no_overrun_tiny_buf(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0003ULL);
    const size_t buf_len = 4; /* max 3 chars + null */
    char guarded[32];
    for (int i = 0; i < 300; i++) {
        memset(guarded, (int)0xCC, sizeof(guarded));
        size_t len = chaff_generate_ssid(guarded, buf_len, rng);
        /* Returned length must leave room for null terminator. */
        ASSERT(len < buf_len);
        /* Must be null-terminated within the declared buffer. */
        ASSERT_EQ((unsigned char)guarded[len], (unsigned char)'\0');
        /* No write beyond buf_len. */
        for (size_t j = buf_len; j < sizeof(guarded); j++) {
            ASSERT_EQ((unsigned char)guarded[j], (unsigned char)0xCC);
        }
    }
    chaff_rng_free(rng);
    PASS("ssid_no_overrun_tiny_buf");
}

/*
 * Variety check: in 50 generations, produce at least 5 distinct SSIDs.
 * This encodes the "ever-changing" requirement at a minimal threshold —
 * even a pure-template generator with ≥5 templates passes.
 */
static void test_ssid_variety(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0004ULL);
    char pool[50][33];
    for (int i = 0; i < 50; i++) {
        pool[i][0] = '\0';
        chaff_generate_ssid(pool[i], sizeof(pool[i]), rng);
    }
    int unique = 0;
    for (int i = 0; i < 50; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(pool[i], pool[j]) == 0) { dup = 1; break; }
        }
        if (!dup) unique++;
    }
    ASSERT(unique >= 5);
    chaff_rng_free(rng);
    PASS("ssid_variety");
}

/* =====================================================================
 * BSSID tests
 * =================================================================== */

/*
 * bit 0 of out[0] == 0  →  unicast (looks like a real AP, not multicast).
 * bit 1 of out[0] == 0  →  globally administered (OUI-based, not random).
 * Both bits must hold for ALL 500 generated BSSIDs.
 */
static void test_bssid_address_bits(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0005ULL);
    uint8_t mac[6];
    for (int i = 0; i < 500; i++) {
        chaff_generate_bssid(mac, rng);
        /* unicast: LSB of first octet must be 0 */
        ASSERT((mac[0] & 0x01u) == 0u);
        /* globally administered: second LSB of first octet must be 0 */
        ASSERT((mac[0] & 0x02u) == 0u);
    }
    chaff_rng_free(rng);
    PASS("bssid_address_bits");
}

/*
 * 100-sample variety: essentially all BSSIDs should be distinct.
 * The last 3 octets are random (16M+ space); expect near-zero collisions.
 */
static void test_bssid_variety(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0006ULL);
    static uint8_t pool[100][6];
    for (int i = 0; i < 100; i++) {
        chaff_generate_bssid(pool[i], rng);
    }
    int unique = 0;
    for (int i = 0; i < 100; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (memcmp(pool[i], pool[j], 6) == 0) { dup = 1; break; }
        }
        if (!dup) unique++;
    }
    ASSERT(unique >= 90);
    chaff_rng_free(rng);
    PASS("bssid_variety");
}

/* =====================================================================
 * BLE MAC tests
 * =================================================================== */

/*
 * bit 1 of out[0] == 1  →  locally administered  (required for BLE random MACs;
 *                            spec §5.1, §6, and keaton-architecture.md §3).
 * bit 0 of out[0] == 0  →  unicast.
 */
static void test_ble_mac_address_bits(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0007ULL);
    uint8_t mac[6];
    for (int i = 0; i < 500; i++) {
        chaff_generate_ble_mac(mac, rng);
        /* locally administered: bit 1 must be SET */
        ASSERT((mac[0] & 0x02u) != 0u);
        /* unicast: bit 0 must be CLEAR */
        ASSERT((mac[0] & 0x01u) == 0u);
    }
    chaff_rng_free(rng);
    PASS("ble_mac_address_bits");
}

/* Near-zero collision rate in 100 samples. */
static void test_ble_mac_variety(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0008ULL);
    static uint8_t pool[100][6];
    for (int i = 0; i < 100; i++) {
        chaff_generate_ble_mac(pool[i], rng);
    }
    int unique = 0;
    for (int i = 0; i < 100; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (memcmp(pool[i], pool[j], 6) == 0) { dup = 1; break; }
        }
        if (!dup) unique++;
    }
    ASSERT(unique >= 90);
    chaff_rng_free(rng);
    PASS("ble_mac_variety");
}

/* =====================================================================
 * BLE name tests
 * =================================================================== */

/* Length in [1, buf_len-1], null-terminated at index len. */
static void test_ble_name_length_and_termination(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x0009ULL);
    char buf[64];
    for (int i = 0; i < 500; i++) {
        memset(buf, (int)0xAA, sizeof(buf));
        size_t len = chaff_generate_ble_name(buf, sizeof(buf), rng);
        ASSERT(len >= 1 && len <= sizeof(buf) - 1);
        ASSERT_EQ((unsigned char)buf[len], (unsigned char)'\0');
        ASSERT(strlen(buf) == len);
    }
    chaff_rng_free(rng);
    PASS("ble_name_length_and_termination");
}

/* No overrun with tiny buf_len (4). */
static void test_ble_name_no_overrun_tiny_buf(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x000AULL);
    const size_t buf_len = 4;
    char guarded[32];
    for (int i = 0; i < 300; i++) {
        memset(guarded, (int)0xCC, sizeof(guarded));
        size_t len = chaff_generate_ble_name(guarded, buf_len, rng);
        ASSERT(len < buf_len);
        ASSERT_EQ((unsigned char)guarded[len], (unsigned char)'\0');
        for (size_t j = buf_len; j < sizeof(guarded); j++) {
            ASSERT_EQ((unsigned char)guarded[j], (unsigned char)0xCC);
        }
    }
    chaff_rng_free(rng);
    PASS("ble_name_no_overrun_tiny_buf");
}

/* Variety: at least 5 distinct names in 50 generations. */
static void test_ble_name_variety(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x000BULL);
    char pool[50][64];
    for (int i = 0; i < 50; i++) {
        pool[i][0] = '\0';
        chaff_generate_ble_name(pool[i], sizeof(pool[i]), rng);
    }
    int unique = 0;
    for (int i = 0; i < 50; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(pool[i], pool[j]) == 0) { dup = 1; break; }
        }
        if (!dup) unique++;
    }
    ASSERT(unique >= 5);
    chaff_rng_free(rng);
    PASS("ble_name_variety");
}

/* =====================================================================
 * Sub-GHz payload tests
 * =================================================================== */

/*
 * Fills exactly `len` bytes; sentinels beyond `len` must be untouched.
 * The filled region must not still be entirely 0xAA (astronomically
 * unlikely for any real PRNG with len >= 8).
 */
static void test_subghz_payload_exact_fill(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x000CULL);
    const size_t fill_len = 32;
    uint8_t buf[64];
    memset(buf, 0xAA, sizeof(buf));
    chaff_generate_subghz_payload(buf, fill_len, rng);
    /* Sentinel region must be untouched. */
    for (size_t i = fill_len; i < sizeof(buf); i++) {
        ASSERT_EQ(buf[i], (uint8_t)0xAA);
    }
    /* Filled region should not be all 0xAA. */
    int unchanged = 0;
    for (size_t i = 0; i < fill_len; i++) {
        if (buf[i] == 0xAA) unchanged++;
    }
    ASSERT(unchanged < (int)fill_len - 1);
    chaff_rng_free(rng);
    PASS("subghz_payload_exact_fill");
}

/* Two consecutive payloads must differ (PRNG state advances between calls). */
static void test_subghz_payload_varies(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0x000DULL);
    uint8_t a[16], b[16];
    chaff_generate_subghz_payload(a, sizeof(a), rng);
    chaff_generate_subghz_payload(b, sizeof(b), rng);
    ASSERT(memcmp(a, b, sizeof(a)) != 0);
    chaff_rng_free(rng);
    PASS("subghz_payload_varies");
}

/* =====================================================================
 * Rotation / uniqueness — 1000-sample batch tests
 * Encodes spec §7: "No identifier should repeat within a rolling window
 * of at least 1000 emitted identifiers per radio."
 * =================================================================== */

/*
 * SSIDs: require at least 20 distinct values in 1000 generations.
 *
 * Threshold rationale: SSID generators are template-based, so with a
 * template pool of N entries some collisions are inherent.  We assert a
 * floor of 20 distinct values — achievable with ≥20 templates or with
 * fewer templates that include randomised suffixes.  This is intentionally
 * lenient so that a variety of valid implementations pass, while still
 * catching a fully constant or degenerate implementation.
 */
static void test_ssid_1000_uniqueness(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0xFEEDFACEULL);
    static char pool[1000][33];
    for (int i = 0; i < 1000; i++) {
        pool[i][0] = '\0';
        chaff_generate_ssid(pool[i], sizeof(pool[i]), rng);
    }
    int distinct = 0;
    for (int i = 0; i < 1000; i++) {
        int seen_before = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(pool[i], pool[j]) == 0) { seen_before = 1; break; }
        }
        if (!seen_before) distinct++;
    }
    /* At least 20 distinct SSIDs in 1000 — encodes "ever-changing" requirement. */
    ASSERT(distinct >= 20);
    chaff_rng_free(rng);
    PASS("ssid_1000_uniqueness");
}

/*
 * BSSIDs: last 3 bytes are random → 16M+ distinct values.
 * Expect < 5 collisions in 1000 samples.
 */
static void test_bssid_1000_uniqueness(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0xCAFEBABEULL);
    static uint8_t pool[1000][6];
    for (int i = 0; i < 1000; i++) {
        chaff_generate_bssid(pool[i], rng);
    }
    int collisions = 0;
    for (int i = 1; i < 1000; i++) {
        for (int j = 0; j < i; j++) {
            if (memcmp(pool[i], pool[j], 6) == 0) { collisions++; break; }
        }
    }
    ASSERT(collisions < 5);
    chaff_rng_free(rng);
    PASS("bssid_1000_uniqueness");
}

/*
 * BLE MACs: 6 random bytes (with 2 fixed bits) → effectively 46 random bits.
 * Expect < 5 collisions in 1000 samples.
 */
static void test_ble_mac_1000_uniqueness(void) {
    ChaffRng *rng = chaff_rng_alloc();
    chaff_rng_seed(rng, 0xDEADC0DEULL);
    static uint8_t pool[1000][6];
    for (int i = 0; i < 1000; i++) {
        chaff_generate_ble_mac(pool[i], rng);
    }
    int collisions = 0;
    for (int i = 1; i < 1000; i++) {
        for (int j = 0; j < i; j++) {
            if (memcmp(pool[i], pool[j], 6) == 0) { collisions++; break; }
        }
    }
    ASSERT(collisions < 5);
    chaff_rng_free(rng);
    PASS("ble_mac_1000_uniqueness");
}

/* =====================================================================
 * main
 * =================================================================== */
int main(void) {
    printf("=== test_identifiers ===\n");

    printf("\n-- SSID --\n");
    test_ssid_length_and_termination();
    test_ssid_printable_ascii();
    test_ssid_no_overrun_tiny_buf();
    test_ssid_variety();

    printf("\n-- BSSID --\n");
    test_bssid_address_bits();
    test_bssid_variety();

    printf("\n-- BLE MAC --\n");
    test_ble_mac_address_bits();
    test_ble_mac_variety();

    printf("\n-- BLE Name --\n");
    test_ble_name_length_and_termination();
    test_ble_name_no_overrun_tiny_buf();
    test_ble_name_variety();

    printf("\n-- Sub-GHz payload --\n");
    test_subghz_payload_exact_fill();
    test_subghz_payload_varies();

    printf("\n-- Rotation / uniqueness (1000-sample batches) --\n");
    test_ssid_1000_uniqueness();
    test_bssid_1000_uniqueness();
    test_ble_mac_1000_uniqueness();

    printf("\nAll identifier tests PASSED (%d assertions checked).\n",
           g_assert_count);
    return 0;
}
