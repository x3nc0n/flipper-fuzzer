/*
 * FluckFlock Wi-Fi Companion Firmware — ESP32-S2 (Official Flipper Wi-Fi Dev Board)
 *
 * Implements the FluckFlock UART line protocol:
 *
 *   Flipper → ESP32     ESP32 → Flipper   Action
 *   ─────────────────   ───────────────   ─────────────────────────────────────
 *   FF?\n               FF!v1\n           Handshake / board detection
 *   FFON\n              OK\n              Enter 802.11 raw-injection mode
 *   FFOFF\n             OK\n              Exit injection mode
 *   FFB <bssid> <ssid>  (none)            Inject one 802.11 beacon frame
 *
 * bssid: 12 lowercase hex chars, no separators  e.g. "aabbccddeeff"
 * ssid:  raw 1–32 char printable string
 *
 * UART wiring to Flipper Zero expansion header:
 *   Flipper GPIO 13 (TX) → ESP32-S2 GPIO 44 (UART0 RX)
 *   Flipper GPIO 14 (RX) ← ESP32-S2 GPIO 43 (UART0 TX)
 *
 * Debug output on USB CDC (Serial0 on ESP32-S2 Arduino).
 *
 * ⚠ LEGAL NOTICE: 802.11 frame injection is regulated.  Use only in
 *   controlled research environments or for anti-tracking purposes where
 *   permitted by local law.  You are responsible for ensuring compliance.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

/* ── Configuration ── */

#ifndef FLUCKFLOCK_CHANNEL
#define FLUCKFLOCK_CHANNEL 6
#endif

/* UART0 hardware pins on the ESP32-S2 that the dev board wires to Flipper */
#define FLIPPER_UART_RX_PIN 44  /* ← Flipper GPIO 13 TX */
#define FLIPPER_UART_TX_PIN 43  /* → Flipper GPIO 14 RX */
#define FLIPPER_UART_BAUD   115200

/* Maximum line length: "FFB " + 12 hex + " " + 32-char SSID + "\n" + '\0' = 51 */
#define LINE_BUF_LEN 64

/* ── Globals ── */

static bool injection_enabled = false;
static uint8_t current_channel = FLUCKFLOCK_CHANNEL;

/* Line accumulator for the Flipper serial link */
static char line_buf[LINE_BUF_LEN];
static size_t line_pos = 0;

/* ── 802.11 Beacon Frame Template ──
 *
 * Layout (standard management frame + tagged parameters):
 *
 *  [MAC Header]      24 bytes
 *    fc[2]           Frame Control: type=0 (Mgmt), subtype=8 (Beacon)
 *    dur[2]          Duration: 0x0000
 *    da[6]           Destination: ff:ff:ff:ff:ff:ff (broadcast)
 *    sa[6]           Source address = BSSID
 *    bssid[6]        BSSID
 *    seq[2]          Sequence control: 0x0000
 *  [Fixed Parameters] 12 bytes
 *    timestamp[8]    0x0000000000000000
 *    interval[2]     Beacon interval: 0x6400 = 100 TUs (~102.4 ms)
 *    capinfo[2]      Capability info: 0x0431 (ESS, Short Preamble, Short Slot)
 *  [Tagged Parameters]
 *    SSID element    tag=0x00, len, ssid_bytes
 *    Supported Rates tag=0x01, len=8, rates[8]
 *    DS Param        tag=0x03, len=1, channel
 */

/* Static parts of the beacon (everything before the SSID tag) */
static const uint8_t BEACON_HDR[] = {
    /* Frame Control */
    0x80, 0x00,
    /* Duration */
    0x00, 0x00,
    /* Destination: broadcast */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* Source / BSSID (6+6 = 12 bytes — filled in at runtime) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* SA placeholder */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* BSSID placeholder */
    /* Sequence control */
    0x00, 0x00,
    /* Timestamp */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Beacon interval: 100 TU */
    0x64, 0x00,
    /* Capability info: ESS + Short Preamble + Short Slot Time */
    0x31, 0x04,
};

static const uint8_t BEACON_RATES[] = {
    0x01, 0x08, /* tag=Supported Rates, len=8 */
    0x82,       /* 1 Mbps  (basic) */
    0x84,       /* 2 Mbps  (basic) */
    0x8B,       /* 5.5 Mbps (basic) */
    0x96,       /* 11 Mbps (basic) */
    0x24,       /* 18 Mbps */
    0x30,       /* 24 Mbps */
    0x48,       /* 36 Mbps */
    0x6C,       /* 54 Mbps */
};

/* Build a complete beacon frame into frame_buf, return total length */
static size_t build_beacon(
    uint8_t*       frame_buf,
    size_t         frame_buf_len,
    const uint8_t  bssid[6],
    const char*    ssid,
    size_t         ssid_len,
    uint8_t        channel) {

    /* Minimum buffer: header(36) + SSID tag(2+ssid_len) + rates(10) + DS(3) */
    size_t needed = sizeof(BEACON_HDR) + 2 + ssid_len + sizeof(BEACON_RATES) + 3;
    if(frame_buf_len < needed) return 0;

    size_t pos = 0;

    memcpy(frame_buf + pos, BEACON_HDR, sizeof(BEACON_HDR));
    pos += sizeof(BEACON_HDR);

    /* Patch SA (offset 10) and BSSID (offset 16) */
    memcpy(frame_buf + 10, bssid, 6);
    memcpy(frame_buf + 16, bssid, 6);

    /* SSID element */
    frame_buf[pos++] = 0x00;               /* tag: SSID */
    frame_buf[pos++] = (uint8_t)ssid_len;
    memcpy(frame_buf + pos, ssid, ssid_len);
    pos += ssid_len;

    /* Supported Rates */
    memcpy(frame_buf + pos, BEACON_RATES, sizeof(BEACON_RATES));
    pos += sizeof(BEACON_RATES);

    /* DS Parameter Set (current channel) */
    frame_buf[pos++] = 0x03;   /* tag: DS Param */
    frame_buf[pos++] = 0x01;   /* length */
    frame_buf[pos++] = channel;

    return pos;
}

/* ── Helpers ── */

/* Parse 12 lowercase hex chars into a 6-byte array.  Returns true on success. */
static bool parse_bssid(const char* hex12, uint8_t out[6]) {
    for(int i = 0; i < 6; i++) {
        char hi = hex12[i * 2];
        char lo = hex12[i * 2 + 1];
        auto hex_val = [](char c) -> int {
            if(c >= '0' && c <= '9') return c - '0';
            if(c >= 'a' && c <= 'f') return c - 'a' + 10;
            if(c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hv = hex_val(hi);
        int lv = hex_val(lo);
        if(hv < 0 || lv < 0) return false;
        out[i] = (uint8_t)((hv << 4) | lv);
    }
    return true;
}

/* ── Wi-Fi injection control ── */

static void wifi_injection_start(void) {
    nvs_flash_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);

    /*
     * Promiscuous mode is required by the esp_wifi_80211_tx() injection path.
     * The promiscuous callback is not used — we only need the mode flag set.
     */
    esp_wifi_set_promiscuous(true);

    injection_enabled = true;

    Serial0.print("[FF] injection ON ch=");
    Serial0.println(current_channel);
}

static void wifi_injection_stop(void) {
    if(!injection_enabled) return;
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    injection_enabled = false;
    Serial0.println("[FF] injection OFF");
}

/* ── Command handler ── */

/* Inject one beacon for the given SSID + BSSID */
static void cmd_beacon(const char* bssid_hex12, const char* ssid, size_t ssid_len) {
    if(!injection_enabled) {
        Serial0.println("[FF] ERR beacon while injection off");
        return;
    }
    uint8_t bssid[6];
    if(!parse_bssid(bssid_hex12, bssid)) {
        Serial0.println("[FF] ERR bad bssid");
        return;
    }
    if(ssid_len == 0 || ssid_len > 32) {
        Serial0.println("[FF] ERR bad ssid len");
        return;
    }

    uint8_t frame[128];
    size_t  frame_len = build_beacon(frame, sizeof(frame), bssid, ssid, ssid_len, current_channel);
    if(frame_len == 0) {
        Serial0.println("[FF] ERR frame build failed");
        return;
    }

    esp_wifi_80211_tx(WIFI_IF_STA, frame, (int)frame_len, false);

    Serial0.print("[FF] beacon ");
    Serial0.print(bssid_hex12);
    Serial0.print(" ");
    Serial0.println(ssid);
}

/* Process a single complete line (with trailing '\n' stripped) */
static void process_line(char* line) {
    Serial0.print("[FF] rx: ");
    Serial0.println(line);

    if(strcmp(line, "FF?") == 0) {
        Serial.println("FF!v1");
    } else if(strcmp(line, "FFON") == 0) {
        wifi_injection_start();
        Serial.println("OK");
    } else if(strcmp(line, "FFOFF") == 0) {
        wifi_injection_stop();
        Serial.println("OK");
    } else if(strncmp(line, "FFB ", 4) == 0) {
        /*
         * "FFB <12hex> <ssid>"
         *       ^4      ^17   (positions relative to line start)
         */
        const char* rest = line + 4;
        /* Expect exactly 12 hex chars then a space */
        if(strlen(rest) < 14 || rest[12] != ' ') {
            Serial0.println("[FF] ERR malformed FFB");
            return;
        }
        char bssid_hex[13];
        memcpy(bssid_hex, rest, 12);
        bssid_hex[12] = '\0';

        const char* ssid = rest + 13;
        size_t ssid_len = strlen(ssid);

        cmd_beacon(bssid_hex, ssid, ssid_len);
    } else {
        Serial0.print("[FF] unknown cmd: ");
        Serial0.println(line);
    }
}

/* ── Arduino entry points ── */

void setup(void) {
    /* USB CDC debug output */
    Serial0.begin(115200);
    Serial0.println("[FF] FluckFlock companion v1 booting");

    /* UART to Flipper on hardware UART0 pins 43/44 */
    Serial.begin(FLIPPER_UART_BAUD, SERIAL_8N1, FLIPPER_UART_RX_PIN, FLIPPER_UART_TX_PIN);
    Serial0.println("[FF] Flipper UART ready");
}

void loop(void) {
    while(Serial.available()) {
        char c = (char)Serial.read();
        if(c == '\n') {
            /* Strip trailing '\r' if present */
            if(line_pos > 0 && line_buf[line_pos - 1] == '\r') {
                line_pos--;
            }
            line_buf[line_pos] = '\0';
            if(line_pos > 0) {
                process_line(line_buf);
            }
            line_pos = 0;
        } else if(line_pos < LINE_BUF_LEN - 1) {
            line_buf[line_pos++] = c;
        }
        /* Silently drop bytes if the line buffer overflows */
    }
}
