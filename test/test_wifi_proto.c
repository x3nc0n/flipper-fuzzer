/*
 * test_wifi_proto.c — Host-side unit tests for the FluckFlock Wi-Fi UART
 * line-protocol builder (wifi_proto.h / wifi_proto.c).
 *
 * Contract under test: radio/wifi_proto.h
 *   wifi_proto_build_handshake(buf, buf_len)  → size_t
 *   wifi_proto_build_on(buf, buf_len)         → size_t
 *   wifi_proto_build_off(buf, buf_len)        → size_t
 *   wifi_proto_build_beacon(buf, buf_len, ssid, bssid[6]) → size_t
 *
 * All functions return bytes written (excl. '\0'), or 0 on error.
 * The module is pure C — no Flipper SDK, compilable on any host.
 *
 * Build (from test/):
 *   cc -std=c11 -Wall -Wextra -Wpedantic \
 *      -I../flipperzero/fluckflock/radio \
 *      -o test_wifi_proto test_wifi_proto.c \
 *      ../flipperzero/fluckflock/radio/wifi_proto.c
 *
 * Run: ./test_wifi_proto
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "test_util.h"
#include "../flipperzero/fluckflock/radio/wifi_proto.h"

/* =========================================================================
 * 1. Simple command builders — exact string, return value, null termination
 * ======================================================================= */

static void test_handshake_exact_string(void) {
    char buf[16];
    memset(buf, 0xAA, sizeof(buf));
    size_t n = wifi_proto_build_handshake(buf, sizeof(buf));
    /* Must return exactly strlen("FF?\n") == 4 */
    ASSERT_EQ(n, (size_t)4);
    /* Content must match the documented command verbatim */
    ASSERT(strcmp(buf, WIFI_PROTO_HANDSHAKE_CMD) == 0);
    /* Must be null-terminated at index n */
    ASSERT_EQ((unsigned char)buf[n], (unsigned char)'\0');
    /* Sentinel beyond n+1 must be untouched */
    ASSERT_EQ((unsigned char)buf[n + 1], (unsigned char)0xAA);
    PASS("handshake_exact_string");
}

static void test_on_exact_string(void) {
    char buf[16];
    memset(buf, 0xAA, sizeof(buf));
    size_t n = wifi_proto_build_on(buf, sizeof(buf));
    /* "FFON\n" = 5 bytes */
    ASSERT_EQ(n, (size_t)5);
    ASSERT(strcmp(buf, WIFI_PROTO_ON_CMD) == 0);
    ASSERT_EQ((unsigned char)buf[n], (unsigned char)'\0');
    ASSERT_EQ((unsigned char)buf[n + 1], (unsigned char)0xAA);
    PASS("on_exact_string");
}

static void test_off_exact_string(void) {
    char buf[16];
    memset(buf, 0xAA, sizeof(buf));
    size_t n = wifi_proto_build_off(buf, sizeof(buf));
    /* "FFOFF\n" = 6 bytes */
    ASSERT_EQ(n, (size_t)6);
    ASSERT(strcmp(buf, WIFI_PROTO_OFF_CMD) == 0);
    ASSERT_EQ((unsigned char)buf[n], (unsigned char)'\0');
    ASSERT_EQ((unsigned char)buf[n + 1], (unsigned char)0xAA);
    PASS("off_exact_string");
}

/* Too-small buffer → must return 0 (not write anything useful, certainly no
 * overrun).  We use a guarded buffer to verify the sentinel is untouched. */
static void test_simple_builders_too_small_buf(void) {
    char guarded[16];

    /* Handshake needs 5 bytes ("FF?\n\0"); buf_len=4 is too small */
    memset(guarded, 0xBB, sizeof(guarded));
    ASSERT_EQ(wifi_proto_build_handshake(guarded, 4), (size_t)0);
    /* No bytes past index 3 should have been written as protocol data */
    for(size_t i = 4; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xBB);

    /* ON needs 6 bytes; buf_len=5 is too small */
    memset(guarded, 0xBB, sizeof(guarded));
    ASSERT_EQ(wifi_proto_build_on(guarded, 5), (size_t)0);
    for(size_t i = 5; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xBB);

    /* OFF needs 7 bytes; buf_len=6 is too small */
    memset(guarded, 0xBB, sizeof(guarded));
    ASSERT_EQ(wifi_proto_build_off(guarded, 6), (size_t)0);
    for(size_t i = 6; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xBB);

    PASS("simple_builders_too_small_buf");
}

/* Exact-fit buffers (len == required) must succeed */
static void test_simple_builders_exact_fit_buf(void) {
    char buf[16];

    /* "FF?\n\0" = 5 bytes needed */
    ASSERT_EQ(wifi_proto_build_handshake(buf, 5), (size_t)4);
    ASSERT(strcmp(buf, "FF?\n") == 0);

    /* "FFON\n\0" = 6 bytes needed */
    ASSERT_EQ(wifi_proto_build_on(buf, 6), (size_t)5);
    ASSERT(strcmp(buf, "FFON\n") == 0);

    /* "FFOFF\n\0" = 7 bytes needed */
    ASSERT_EQ(wifi_proto_build_off(buf, 7), (size_t)6);
    ASSERT(strcmp(buf, "FFOFF\n") == 0);

    PASS("simple_builders_exact_fit_buf");
}

/* =========================================================================
 * 2. Beacon framing — known ssid + known bssid → exact expected string
 * ======================================================================= */

static void test_beacon_known_ssid_and_bssid(void) {
    /* bssid = AA:BB:CC:DD:EE:FF → "aabbccddeeff" */
    const uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    const char*   ssid     = "TestNet";
    char buf[64];
    memset(buf, 0xAA, sizeof(buf));

    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), ssid, bssid);

    /* Expected: "FFB aabbccddeeff TestNet\n" (25 bytes) */
    const char* expected = "FFB aabbccddeeff TestNet\n";
    size_t      exp_len  = strlen(expected);
    ASSERT_EQ(n, exp_len);
    ASSERT(strcmp(buf, expected) == 0);
    /* Null-terminated at index n */
    ASSERT_EQ((unsigned char)buf[n], (unsigned char)'\0');
    /* Sentinel beyond is untouched */
    ASSERT_EQ((unsigned char)buf[n + 1], (unsigned char)0xAA);

    PASS("beacon_known_ssid_and_bssid");
}

/* Verify exact byte-order: first BSSID byte → leftmost hex pair. */
static void test_beacon_bssid_byte_order(void) {
    /* bssid[0]=0x12 must appear as "12" at the start of the hex field */
    const uint8_t bssid[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    char buf[64];
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), "X", bssid);
    /* Expected: "FFB 123456789abc X\n" (19 bytes) */
    const char* expected = "FFB 123456789abc X\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);
    PASS("beacon_bssid_byte_order");
}

/* =========================================================================
 * 3. Hex correctness — 0x00, 0x0f, 0xa5, 0xff + leading zeros preserved
 * ======================================================================= */

static void test_beacon_hex_edge_bytes(void) {
    /* 0x00 → "00", 0x0f → "0f", 0xa5 → "a5", 0xff → "ff",
       0x10 → "10", 0xf0 → "f0"  (ensures nibble order in each byte) */
    const uint8_t bssid[6] = {0x00, 0x0f, 0xa5, 0xff, 0x10, 0xf0};
    char buf[64];
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), "Z", bssid);
    /* Expected hex segment: "000fa5ff10f0" */
    const char* expected = "FFB 000fa5ff10f0 Z\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);

    /* Spot-check: index 4..15 is the 12-char hex field */
    ASSERT_EQ(buf[4],  '0');  /* 0x00 high nibble */
    ASSERT_EQ(buf[5],  '0');  /* 0x00 low  nibble */
    ASSERT_EQ(buf[6],  '0');  /* 0x0f high nibble — leading zero */
    ASSERT_EQ(buf[7],  'f');  /* 0x0f low  nibble */
    ASSERT_EQ(buf[8],  'a');  /* 0xa5 high nibble */
    ASSERT_EQ(buf[9],  '5');  /* 0xa5 low  nibble */
    ASSERT_EQ(buf[10], 'f');  /* 0xff high nibble */
    ASSERT_EQ(buf[11], 'f');  /* 0xff low  nibble */
    ASSERT_EQ(buf[12], '1');  /* 0x10 high nibble */
    ASSERT_EQ(buf[13], '0');  /* 0x10 low  nibble */
    ASSERT_EQ(buf[14], 'f');  /* 0xf0 high nibble */
    ASSERT_EQ(buf[15], '0');  /* 0xf0 low  nibble */

    PASS("beacon_hex_edge_bytes");
}

/* All-zero BSSID → "000000000000" (12 zeros, no separators) */
static void test_beacon_all_zero_bssid(void) {
    const uint8_t bssid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char buf[64];
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), "A", bssid);
    const char* expected = "FFB 000000000000 A\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);
    PASS("beacon_all_zero_bssid");
}

/* All-ff BSSID → "ffffffffffff" (12 'f' chars, lowercase) */
static void test_beacon_all_ff_bssid(void) {
    const uint8_t bssid[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char buf[64];
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), "A", bssid);
    const char* expected = "FFB ffffffffffff A\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);
    PASS("beacon_all_ff_bssid");
}

/* =========================================================================
 * 4. Bounds / safety — too-small buf_len must not overrun, must return 0
 * ======================================================================= */

static void test_beacon_too_small_buf(void) {
    const uint8_t bssid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const char*   ssid     = "Hello";

    /* Minimum needed = 19 + strlen("Hello") = 24 bytes.
     * Test with buf_len = 4 — far too small. */
    char guarded[64];
    memset(guarded, 0xCC, sizeof(guarded));
    size_t n = wifi_proto_build_beacon(guarded, 4, ssid, bssid);
    /* Must return 0 — cannot fit the frame */
    ASSERT_EQ(n, (size_t)0);
    /* Sentinel bytes past index 3 must be untouched */
    for(size_t i = 4; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xCC);

    /* buf_len = 23 is one byte short of the 24 needed */
    memset(guarded, 0xCC, sizeof(guarded));
    n = wifi_proto_build_beacon(guarded, 23, ssid, bssid);
    ASSERT_EQ(n, (size_t)0);
    for(size_t i = 23; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xCC);

    /* buf_len = 24 is exactly right — must succeed */
    memset(guarded, 0xCC, sizeof(guarded));
    n = wifi_proto_build_beacon(guarded, 24, ssid, bssid);
    ASSERT(n > (size_t)0);
    /* '\0' at index n, sentinel at n+1 still untouched */
    ASSERT_EQ((unsigned char)guarded[n], (unsigned char)'\0');
    for(size_t i = 24; i < sizeof(guarded); i++)
        ASSERT_EQ((unsigned char)guarded[i], (unsigned char)0xCC);

    PASS("beacon_too_small_buf");
}

/* =========================================================================
 * 5. SSID edge cases — 1-char and 32-char SSIDs
 * ======================================================================= */

static void test_beacon_ssid_one_char(void) {
    const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    char buf[64];
    memset(buf, 0xAA, sizeof(buf));

    /* Single-char SSID: "X"
     * Expected: "FFB deadbeef0001 X\n" (19 bytes) */
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), "X", bssid);
    const char* expected = "FFB deadbeef0001 X\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);
    ASSERT_EQ((unsigned char)buf[n], (unsigned char)'\0');

    PASS("beacon_ssid_one_char");
}

static void test_beacon_ssid_max_length(void) {
    const uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    /* 32-char SSID: exactly the max allowed */
    const char ssid32[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456"; /* 32 chars */
    char buf[64]; /* WIFI_PROTO_BEACON_CMD_MAX_LEN+1 = 51; 64 is safe */
    memset(buf, 0xAA, sizeof(buf));

    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), ssid32, bssid);

    /* Expected: "FFB 001122334455 " + 32 chars + "\n" = 50 bytes */
    ASSERT_EQ(n, (size_t)WIFI_PROTO_BEACON_CMD_MAX_LEN);

    /* Verify prefix */
    ASSERT(strncmp(buf, "FFB 001122334455 ", 17) == 0);
    /* Verify SSID body */
    ASSERT(strncmp(buf + 17, ssid32, 32) == 0);
    /* Verify trailing newline */
    ASSERT_EQ(buf[49], '\n');
    /* Null-terminated at 50 */
    ASSERT_EQ((unsigned char)buf[50], (unsigned char)'\0');
    /* Sentinel at 51 untouched */
    ASSERT_EQ((unsigned char)buf[51], (unsigned char)0xAA);

    PASS("beacon_ssid_max_length");
}

/* A 33-char SSID exceeds the 1–32 byte rule → must return 0 */
static void test_beacon_ssid_too_long(void) {
    const uint8_t bssid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    /* 33 'A' chars — one over the limit */
    const char ssid33[34] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" "A";
    char buf[64];
    ASSERT_EQ(wifi_proto_build_beacon(buf, sizeof(buf), ssid33, bssid), (size_t)0);
    PASS("beacon_ssid_too_long");
}

/* An empty SSID (zero-length) must return 0 */
static void test_beacon_ssid_empty(void) {
    const uint8_t bssid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char buf[64];
    ASSERT_EQ(wifi_proto_build_beacon(buf, sizeof(buf), "", bssid), (size_t)0);
    PASS("beacon_ssid_empty");
}

/* SSID is appended verbatim — no escaping or transformation */
static void test_beacon_ssid_appended_verbatim(void) {
    const uint8_t bssid[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x12, 0x34};
    const char*   ssid     = "My Network!";
    char buf[64];
    size_t n = wifi_proto_build_beacon(buf, sizeof(buf), ssid, bssid);
    /* Expected: "FFB cafebabe1234 My Network!\n" */
    const char* expected = "FFB cafebabe1234 My Network!\n";
    ASSERT_EQ(n, strlen(expected));
    ASSERT(strcmp(buf, expected) == 0);
    PASS("beacon_ssid_appended_verbatim");
}

/* =========================================================================
 * main
 * ======================================================================= */
int main(void) {
    printf("=== test_wifi_proto ===\n");

    printf("\n-- Simple command builders --\n");
    test_handshake_exact_string();
    test_on_exact_string();
    test_off_exact_string();
    test_simple_builders_too_small_buf();
    test_simple_builders_exact_fit_buf();

    printf("\n-- Beacon framing --\n");
    test_beacon_known_ssid_and_bssid();
    test_beacon_bssid_byte_order();

    printf("\n-- Hex correctness --\n");
    test_beacon_hex_edge_bytes();
    test_beacon_all_zero_bssid();
    test_beacon_all_ff_bssid();

    printf("\n-- Bounds / safety --\n");
    test_beacon_too_small_buf();

    printf("\n-- SSID edge cases --\n");
    test_beacon_ssid_one_char();
    test_beacon_ssid_max_length();
    test_beacon_ssid_too_long();
    test_beacon_ssid_empty();
    test_beacon_ssid_appended_verbatim();

    printf("\nAll wifi_proto tests PASSED (%d assertions checked).\n",
           g_assert_count);
    return 0;
}
