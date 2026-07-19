#include "identity_gen.h"
#include "ssid_words.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "esp_random.h"

void ff_random_mac(uint8_t mac[6]) {
    esp_fill_random(mac, 6);
    mac[0] |= 0x02;   // U/L bit: locally administered
    mac[0] &= ~0x01;  // I/G bit: unicast
}

void ff_random_ble_static_addr(uint8_t addr[6]) {
    esp_fill_random(addr, 6);
    addr[5] |= 0xC0;  // Bluetooth Core Spec static random address: top 2 bits of MSB = 0b11
}

void ff_random_bytes(uint8_t *out, size_t len) {
    esp_fill_random(out, len);
}

static uint32_t ff_rand_range(uint32_t n) {
    // Word/suffix/connector lists are small (<= ~30 entries); modulo bias
    // from esp_random()'s 32-bit range is negligible here.
    return esp_random() % n;
}

static const char *pick(const char *const *list, size_t n) {
    return list[ff_rand_range((uint32_t)n)];
}

typedef enum { CASE_ASIS = 0, CASE_UPPER = 1, CASE_LOWER = 2 } case_mode_t;

static void apply_case(char *s, case_mode_t mode) {
    if (mode == CASE_ASIS) return;
    for (char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        *p = (mode == CASE_UPPER) ? (char)toupper(c) : (char)tolower(c);
    }
}

size_t ff_generate_ssid(char *out, size_t out_cap) {
    char buf[FF_SSID_MAX_LEN + 32]; // headroom before truncation to the 32-byte cap
    buf[0] = '\0';

    // Weighting per spec.md 5.2.1: ~70% of outputs contain a protest word
    // (15% double-protest + 55% single-protest mixes), remaining 30% pull
    // from everyday_words[] only.
    uint32_t r = ff_rand_range(100);
    if (r < 30) {
        // Everyday-only -- keeps some outputs fully mundane.
        const char *w1 = pick(everyday_words, everyday_words_count);
        if (ff_rand_range(2) == 0) {
            const char *suf = pick(suffixes, suffixes_count);
            snprintf(buf, sizeof(buf), "%s%s", w1, suf);
        } else {
            const char *w2 = pick(everyday_words, everyday_words_count);
            const char *conn = pick(connectors, connectors_count);
            snprintf(buf, sizeof(buf), "%s%s%s", w1, conn, w2);
        }
    } else if (r < 45) {
        // Double-protest (template 4), ~15% of all generations.
        const char *w1 = pick(protest_words, protest_words_count);
        const char *w2 = pick(protest_words, protest_words_count);
        const char *conn = pick(connectors, connectors_count);
        snprintf(buf, sizeof(buf), "%s%s%s", w1, conn, w2);
    } else if (r < 63) {
        // Template 1: protest + everyday
        const char *w1 = pick(protest_words, protest_words_count);
        const char *w2 = pick(everyday_words, everyday_words_count);
        const char *conn = pick(connectors, connectors_count);
        snprintf(buf, sizeof(buf), "%s%s%s", w1, conn, w2);
    } else if (r < 81) {
        // Template 2: everyday + protest + suffix
        const char *w1 = pick(everyday_words, everyday_words_count);
        const char *w2 = pick(protest_words, protest_words_count);
        const char *conn = pick(connectors, connectors_count);
        const char *suf = pick(suffixes, suffixes_count);
        snprintf(buf, sizeof(buf), "%s%s%s%s", w1, conn, w2, suf);
    } else {
        // Template 3: protest + suffix
        const char *w1 = pick(protest_words, protest_words_count);
        const char *suf = pick(suffixes, suffixes_count);
        snprintf(buf, sizeof(buf), "%s%s", w1, suf);
    }

    apply_case(buf, (case_mode_t)ff_rand_range(3));

    size_t len = strlen(buf);
    if (len > FF_SSID_MAX_LEN) len = FF_SSID_MAX_LEN; // 802.11 SSID hard limit

    size_t copy_len = (len < out_cap - 1) ? len : out_cap - 1;
    memcpy(out, buf, copy_len);
    out[copy_len] = '\0';
    return copy_len;
}

size_t ff_generate_ble_name(char *out, size_t out_cap) {
    char ssid_buf[FF_SSID_MAX_LEN + 1];
    ff_generate_ssid(ssid_buf, sizeof(ssid_buf));

    size_t len = strlen(ssid_buf);
    if (len > FF_BLE_NAME_MAX_LEN) len = FF_BLE_NAME_MAX_LEN;

    size_t copy_len = (len < out_cap - 1) ? len : out_cap - 1;
    memcpy(out, ssid_buf, copy_len);
    out[copy_len] = '\0';
    return copy_len;
}
