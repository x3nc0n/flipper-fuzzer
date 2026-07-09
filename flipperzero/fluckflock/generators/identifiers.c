/*
 * identifiers.c — plausible wireless identifier generators.
 *
 * Pure C. No Flipper SDK dependency. Host-testable.
 * All functions take a ChaffRng* for reproducible randomness.
 */

#include "identifiers.h"
#include "prng.h"
#include "oui_table.h"
#include "ssid_table.h"
#include "ble_name_table.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---- Internal helpers ---- */

static const char HEX_UPPER[] = "0123456789ABCDEF";
static const char HEX_LOWER[] = "0123456789abcdef";

/* Append n uppercase hex nibbles from val into buf[pos], respecting buf_len.
 * Returns new pos (may be clamped to buf_len-1 before null). */
static size_t append_hex(char* buf, size_t pos, size_t buf_len, uint32_t val, int nibbles, const char* hex) {
    for(int i = nibbles - 1; i >= 0; i--) {
        if(pos + 1 >= buf_len) break; /* leave room for null */
        buf[pos++] = hex[(val >> (i * 4)) & 0xF];
    }
    return pos;
}

/* Append a decimal number (up to 4 digits) into buf. */
static size_t append_dec(char* buf, size_t pos, size_t buf_len, uint32_t val, int digits) {
    char tmp[8];
    int tlen = 0;
    /* Build digits in reverse, then emit forward with leading zeros to `digits` width */
    uint32_t v = val;
    uint32_t div = 1;
    for(int i = 1; i < digits; i++) div *= 10;
    for(int i = 0; i < digits; i++) {
        uint32_t d = (v / div) % 10;
        tmp[tlen++] = (char)('0' + d);
        div /= 10;
        if(div == 0) div = 1;
    }
    for(int i = 0; i < tlen; i++) {
        if(pos + 1 >= buf_len) break;
        buf[pos++] = tmp[i];
    }
    return pos;
}

/* Append a string literal safely. */
static size_t append_str(char* buf, size_t pos, size_t buf_len, const char* s) {
    while(*s && pos + 1 < buf_len) {
        buf[pos++] = *s++;
    }
    return pos;
}

/* ---- chaff_generate_ssid ---- */

size_t chaff_generate_ssid(char* buf, size_t buf_len, ChaffRng* rng) {
    if(!buf || buf_len < 2) return 0;

    uint32_t tmpl = chaff_rng_bounded(rng, SSID_TEMPLATE_COUNT);
    size_t pos = 0;

    switch(tmpl) {
    case 0: {
        /* "<ISP_PREFIX><4 HEX digits>" — e.g. "NETGEAR4F2A", "TP-Link_9B3D" */
        uint32_t pidx = chaff_rng_bounded(rng, SSID_ISP_PREFIX_COUNT);
        uint32_t hex4 = chaff_rng_u32(rng) & 0xFFFF;
        pos = append_str(buf, pos, buf_len, SSID_ISP_PREFIXES[pidx]);
        pos = append_hex(buf, pos, buf_len, hex4, 4, HEX_UPPER);
        break;
    }
    case 1: {
        /* "<ISP_PREFIX><2 decimal digits>" — e.g. "ATT47" */
        uint32_t pidx = chaff_rng_bounded(rng, SSID_ISP_PREFIX_COUNT);
        uint32_t n2 = chaff_rng_bounded(rng, 100);
        pos = append_str(buf, pos, buf_len, SSID_ISP_PREFIXES[pidx]);
        pos = append_dec(buf, pos, buf_len, n2, 2);
        break;
    }
    case 2: {
        /* "<Name>'s <HotspotDevice>" — e.g. "Sarah's iPhone 14" */
        uint32_t nidx = chaff_rng_bounded(rng, SSID_FIRST_NAME_COUNT);
        uint32_t didx = chaff_rng_bounded(rng, SSID_HOTSPOT_DEVICE_COUNT);
        pos = append_str(buf, pos, buf_len, SSID_FIRST_NAMES[nidx]);
        pos = append_str(buf, pos, buf_len, "'s ");
        pos = append_str(buf, pos, buf_len, SSID_HOTSPOT_DEVICES[didx]);
        break;
    }
    case 3: {
        /* Static well-known SSID — e.g. "xfinitywifi", "eduroam" */
        uint32_t sidx = chaff_rng_bounded(rng, SSID_STATIC_NAME_COUNT);
        pos = append_str(buf, pos, buf_len, SSID_STATIC_NAMES[sidx]);
        break;
    }
    case 4: {
        /* "HP-Print-<2 HEX>-<HPModel>" */
        uint32_t hex2 = chaff_rng_u32(rng) & 0xFF;
        uint32_t midx = chaff_rng_bounded(rng, SSID_HP_MODEL_COUNT);
        pos = append_str(buf, pos, buf_len, "HP-Print-");
        pos = append_hex(buf, pos, buf_len, hex2, 2, HEX_UPPER);
        pos = append_str(buf, pos, buf_len, "-");
        pos = append_str(buf, pos, buf_len, SSID_HP_MODELS[midx]);
        break;
    }
    case 5: {
        /* "DIRECT-<2 HEX>-<HotspotDevice>" — Wi-Fi Direct style */
        uint32_t hex2 = chaff_rng_u32(rng) & 0xFF;
        uint32_t didx = chaff_rng_bounded(rng, SSID_HOTSPOT_DEVICE_COUNT);
        pos = append_str(buf, pos, buf_len, "DIRECT-");
        pos = append_hex(buf, pos, buf_len, hex2, 2, HEX_LOWER);
        pos = append_str(buf, pos, buf_len, "-");
        pos = append_str(buf, pos, buf_len, SSID_HOTSPOT_DEVICES[didx]);
        break;
    }
    case 6: {
        /* "AndroidAP_<4 decimal digits>" */
        uint32_t n4 = chaff_rng_bounded(rng, 10000);
        pos = append_str(buf, pos, buf_len, "AndroidAP_");
        pos = append_dec(buf, pos, buf_len, n4, 4);
        break;
    }
    case 7: {
        /* "Redmi Note <digit> Pro" / "Redmi Note <digit>" */
        uint32_t ver = chaff_rng_bounded(rng, 9) + 1;
        uint32_t pro = chaff_rng_bounded(rng, 2);
        pos = append_str(buf, pos, buf_len, "Redmi Note ");
        if(pos + 1 < buf_len) buf[pos++] = (char)('0' + ver);
        if(pro) pos = append_str(buf, pos, buf_len, " Pro");
        break;
    }
    case 8: {
        /* "Setup<4 HEX>" — captive-portal / first-boot router pattern */
        uint32_t hex4 = chaff_rng_u32(rng) & 0xFFFF;
        pos = append_str(buf, pos, buf_len, "Setup");
        pos = append_hex(buf, pos, buf_len, hex4, 4, HEX_UPPER);
        break;
    }
    case 9:
    default: {
        /* "ASUS_<4 HEX>" */
        uint32_t hex4 = chaff_rng_u32(rng) & 0xFFFF;
        pos = append_str(buf, pos, buf_len, "ASUS_");
        pos = append_hex(buf, pos, buf_len, hex4, 4, HEX_UPPER);
        break;
    }
    }

    /* Guarantee null termination */
    if(pos >= buf_len) pos = buf_len - 1;
    buf[pos] = '\0';
    return pos;
}

/* ---- chaff_generate_bssid ---- */

void chaff_generate_bssid(uint8_t out[6], ChaffRng* rng) {
    /* First 3 bytes: pick a real vendor OUI */
    uint32_t idx = chaff_rng_bounded(rng, (uint32_t)CHAFF_OUI_COUNT);
    out[0] = CHAFF_OUI_TABLE[idx][0];
    out[1] = CHAFF_OUI_TABLE[idx][1];
    out[2] = CHAFF_OUI_TABLE[idx][2];

    /* Last 3 bytes: random host portion */
    chaff_rng_bytes(rng, out + 3, 3);

    /*
     * Enforce globally-administered unicast:
     *   bit0 of out[0] = 0  (unicast)
     *   bit1 of out[0] = 0  (globally administered)
     *
     * All OUIs in the table already have these bits clear, but be explicit
     * in case the table is ever edited carelessly.
     */
    out[0] &= 0xFC; /* clear bits 0 and 1 */
}

/* ---- chaff_generate_ble_mac ---- */

void chaff_generate_ble_mac(uint8_t out[6], ChaffRng* rng) {
    chaff_rng_bytes(rng, out, 6);

    /*
     * BLE Random Static Address per Bluetooth Core Spec 5.4 §6.B.1.3.2.1:
     *   - Bits[47:46] = 0b11 (static random address marker)
     *
     * Keaton's contract maps this to:
     *   bit0 = 0 (unicast — multicast bit clear)
     *   bit1 = 1 (locally administered — set to distinguish from vendor MACs)
     *
     * Reconciling: BLE random static uses bits[47:46] of the 48-bit address,
     * which is bits[7:6] of out[0] (MSB-first). We set both to 1 per spec.
     * Keaton's unicast/LA bits refer to the Ethernet/802.3 convention on the
     * same byte; we honour his contract directly since this is BLE-only:
     *   out[0] bit0 = 0 (unicast)
     *   out[0] bit1 = 1 (locally administered / random marker)
     *
     * For a valid BLE random static address we also set the two MSBs:
     *   out[0] bits [7:6] = 0b11
     * Combined mask on out[0]: 0b11xxxxxx & ~0x01 | 0x02 → (out[0] | 0xC2) & ~0x01
     */
    out[0] = (out[0] | 0xC2) & 0xFE; /* set bits 7,6,1; clear bit 0 */
}

/* ---- chaff_generate_ble_name ---- */

size_t chaff_generate_ble_name(char* buf, size_t buf_len, ChaffRng* rng) {
    if(!buf || buf_len < 2) return 0;

    /*
     * Pick from 5 pools + the possessive pattern.
     * Weights (out of 12): earbuds=3, wearables=2, speakers=2, trackers=2, phones=1, possessive=2
     */
    uint32_t pool = chaff_rng_bounded(rng, 12);
    const char* base = NULL;
    size_t pos = 0;

    if(pool < 3) {
        /* Earbuds */
        uint32_t i = chaff_rng_bounded(rng, BLE_NAME_EARBUDS_COUNT);
        base = BLE_NAME_EARBUDS[i];
    } else if(pool < 5) {
        /* Wearables */
        uint32_t i = chaff_rng_bounded(rng, BLE_NAME_WEARABLES_COUNT);
        base = BLE_NAME_WEARABLES[i];
    } else if(pool < 7) {
        /* Speakers */
        uint32_t i = chaff_rng_bounded(rng, BLE_NAME_SPEAKERS_COUNT);
        base = BLE_NAME_SPEAKERS[i];
    } else if(pool < 9) {
        /* Trackers */
        uint32_t i = chaff_rng_bounded(rng, BLE_NAME_TRACKERS_COUNT);
        base = BLE_NAME_TRACKERS[i];
    } else if(pool < 10) {
        /* Phones */
        uint32_t i = chaff_rng_bounded(rng, BLE_NAME_PHONES_COUNT);
        base = BLE_NAME_PHONES[i];
    } else {
        /* "<Name>'s <Device>" possessive pattern */
        uint32_t ni = chaff_rng_bounded(rng, BLE_NAME_FIRST_NAMES_COUNT);
        uint32_t di = chaff_rng_bounded(rng, BLE_NAME_POSSESSIVE_DEVICES_COUNT);
        pos = append_str(buf, pos, buf_len, BLE_NAME_FIRST_NAMES[ni]);
        pos = append_str(buf, pos, buf_len, "'s ");
        pos = append_str(buf, pos, buf_len, BLE_NAME_POSSESSIVE_DEVICES[di]);
        buf[pos] = '\0';
        return pos;
    }

    pos = append_str(buf, pos, buf_len, base);

    /*
     * ~30% of the time, append a short numeric suffix for variation:
     * "JBL Flip 6 (42)" or "AirPods [3]" so scanners see distinct entries.
     */
    if(chaff_rng_bounded(rng, 10) < 3 && pos + 5 < buf_len) {
        uint32_t suffix = chaff_rng_bounded(rng, 100);
        uint32_t style = chaff_rng_bounded(rng, 3);
        if(style == 0) {
            pos = append_str(buf, pos, buf_len, " (");
            pos = append_dec(buf, pos, buf_len, suffix, 2);
            pos = append_str(buf, pos, buf_len, ")");
        } else if(style == 1) {
            pos = append_str(buf, pos, buf_len, " [");
            pos = append_dec(buf, pos, buf_len, suffix, 2);
            pos = append_str(buf, pos, buf_len, "]");
        } else {
            pos = append_str(buf, pos, buf_len, "-");
            pos = append_dec(buf, pos, buf_len, suffix, 2);
        }
    }

    if(pos >= buf_len) pos = buf_len - 1;
    buf[pos] = '\0';
    return pos;
}

/* ---- chaff_generate_subghz_payload ---- */

void chaff_generate_subghz_payload(uint8_t* buf, size_t len, ChaffRng* rng) {
    chaff_rng_bytes(rng, buf, len);
}
