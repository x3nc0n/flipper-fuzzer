/*
 * radio_wifi.c — Wi-Fi Dev Board (ESP32 via UART) implementation (Flipper Zero).
 *
 * Flipper-only.  Uses furi_hal_serial_* for UART communication with the
 * official Flipper Wi-Fi Dev Board (ESP32-S2).
 *
 * Physical connection:
 *   - Port:  FuriHalSerialIdUsart (expansion header USART)
 *   - Baud:  115200 8N1
 *   - Pins:  Flipper GPIO 13 = TX (→ ESP32 RX), GPIO 14 = RX (← ESP32 TX)
 *
 * NOTE: Acquiring FuriHalSerialIdUsart suspends the Flipper serial console.
 * This is expected and acceptable for Wi-Fi Dev Board apps.
 *
 * Protocol: see wifi_proto.h for full spec.
 *   detect → sends "FF?\n", expects "FF!" prefix within WIFI_DETECT_TIMEOUT_MS.
 *   init   → acquires handle, sends "FFON\n".
 *   deinit → sends "FFOFF\n", releases handle.
 *   emit   → sends "FFB <bssid12hex> <ssid>\n" (fire-and-forget).
 *
 * Power sequencing (2026-07-18):
 *   The Flipper Wi-Fi Dev Board is powered from the Flipper 5V OTG rail, which
 *   is OFF by default.  radio_wifi_power_on() enables it and waits for the
 *   ESP32-S2 to boot.  radio_wifi_power_off() disables it on teardown.
 *   See .squad/decisions/inbox/mcmanus-wifi-otg-power.md.
 *
 * SDK symbols verified against Flipper SDK 1.4.3 furi_hal_serial.h /
 * furi_hal_serial_control.h / furi_hal_power.h.
 */

#include "radio_wifi.h"
#include "wifi_proto.h"

/* Flipper SDK — only compiled on-device */
#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_power.h>

#include <string.h>

/* ---- Constants ---- */

#define WIFI_UART_BAUD          115200u

/*
 * Time to wait after enabling OTG power before the ESP32-S2 is UART-ready.
 *
 * The ESP32-S2 Arduino build runs two Serial.begin() calls plus several Serial
 * prints in setup().  ROM boot + Arduino init + USB-CDC + UART ready takes
 * roughly 800–1000 ms on real hardware.  We use 1500 ms to absorb cold-start
 * jitter and any variation across firmware builds without being excessively slow.
 */
#define WIFI_BOOT_SETTLE_MS     1500u

/* Timeout per attempt for the "FF?" → "FF!v1" handshake exchange */
#define WIFI_DETECT_TIMEOUT_MS  400u

/*
 * Number of times to retry the handshake before declaring the board absent.
 * The first attempt can lose bytes while the ESP32 UART FIFO settles post-boot.
 */
#define WIFI_DETECT_ATTEMPTS    3u

/* Gap between successive detect retries */
#define WIFI_DETECT_RETRY_MS    150u

/* Largest TX buffer needed: "FFB " + 12 hex + " " + 32-char SSID + "\n\0" = 51 */
#define WIFI_CMD_BUF_SIZE       64u

/* RX scratch buffer for the handshake reply */
#define WIFI_RX_BUF_SIZE        32u

/* ---- Static state ---- */

static FuriHalSerialHandle* wifi_serial      = NULL;
static bool                 wifi_initialized = false;

/* ---- RX context (detect handshake) ---- */

/*
 * Collected in the interrupt-context async-rx callback.
 * The callback is the only writer; the main thread only reads after the
 * semaphore has been released, so no additional locking is needed.
 */
typedef struct {
    char             buf[WIFI_RX_BUF_SIZE];
    volatile uint8_t len;
    FuriSemaphore*   sem;
} WifiRxCtx;

/*
 * Called from interrupt context.
 * Drains bytes via furi_hal_serial_async_rx (must be called from callback).
 * Signals the semaphore when a '\n' is received or the buffer is full.
 */
static void wifi_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void*                context) {

    WifiRxCtx* rx = (WifiRxCtx*)context;
    if(!(event & FuriHalSerialRxEventData)) return;

    while(furi_hal_serial_async_rx_available(handle)) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        if(rx->len < (uint8_t)(WIFI_RX_BUF_SIZE - 1u)) {
            rx->buf[rx->len++] = (char)b;
        }
        if(b == '\n' || rx->len >= (uint8_t)(WIFI_RX_BUF_SIZE - 1u)) {
            rx->buf[rx->len] = '\0';
            furi_semaphore_release(rx->sem);
            return;
        }
    }
}

/* ---- Internal TX helper ---- */

static void wifi_tx_str(FuriHalSerialHandle* h, const char* s, size_t len) {
    furi_hal_serial_tx(h, (const uint8_t*)s, len);
    furi_hal_serial_tx_wait_complete(h);
}

/*
 * Perform one detect handshake attempt on an already-opened handle.
 * Sends FF?\n, waits up to WIFI_DETECT_TIMEOUT_MS for an "FF!" reply prefix.
 * Returns true on success.
 */
static bool wifi_do_detect_once(FuriHalSerialHandle* h) {
    char   cmd[8];
    size_t cmd_len = wifi_proto_build_handshake(cmd, sizeof(cmd));
    if(cmd_len == 0u) return false;

    WifiRxCtx rx;
    memset(&rx, 0, sizeof(rx));
    rx.sem = furi_semaphore_alloc(1u, 0u);
    if(!rx.sem) return false;

    furi_hal_serial_async_rx_start(h, wifi_rx_callback, &rx, false);
    wifi_tx_str(h, cmd, cmd_len);

    FuriStatus status =
        furi_semaphore_acquire(rx.sem, furi_ms_to_ticks(WIFI_DETECT_TIMEOUT_MS));

    furi_hal_serial_async_rx_stop(h);
    furi_semaphore_free(rx.sem);

    if(status != FuriStatusOk) return false;

    /* Verify the "FF!" prefix in the reply */
    return (rx.len >= 3u &&
            rx.buf[0] == 'F' &&
            rx.buf[1] == 'F' &&
            rx.buf[2] == '!');
}

/*
 * Perform a detect handshake on an already-opened handle with retry logic.
 *
 * On the first attempt, send a lone '\n' flush before the real handshake to
 * clear any garbage in the ESP32 UART rx FIFO left over from boot prints.
 * Then try up to WIFI_DETECT_ATTEMPTS times with WIFI_DETECT_RETRY_MS gaps.
 */
static bool wifi_do_detect(FuriHalSerialHandle* h) {
    /* Pre-flush: clear the ESP32 rx line buffer of any boot-time noise */
    static const char flush_nl[] = "\n";
    wifi_tx_str(h, flush_nl, 1u);
    furi_delay_ms(20u); /* brief pause so the ESP32 echoes/discards the NL */

    for(uint8_t attempt = 0u; attempt < WIFI_DETECT_ATTEMPTS; attempt++) {
        if(wifi_do_detect_once(h)) return true;
        if(attempt + 1u < WIFI_DETECT_ATTEMPTS) {
            furi_delay_ms(WIFI_DETECT_RETRY_MS);
        }
    }
    return false;
}

/* ---- Public API — power management ---- */

void radio_wifi_power_on(void) {
    if(furi_hal_power_is_otg_enabled()) {
        /* OTG already on — board is already booted, skip delay */
        return;
    }
    furi_hal_power_enable_otg();
    /* Block until the ESP32-S2 has booted and its UART is ready */
    furi_delay_ms(WIFI_BOOT_SETTLE_MS);
}

void radio_wifi_power_off(void) {
    if(!furi_hal_power_is_otg_enabled()) return;
    furi_hal_power_disable_otg();
}

/* ---- Public API — detect / init / deinit / emit ---- */

bool radio_wifi_detect(void) {
    if(wifi_serial) {
        /*
         * init() already holds the handle (unusual call order, but handle it).
         * Reuse the open port — no re-acquire needed.
         */
        return wifi_do_detect(wifi_serial);
    }

    /* Normal path: acquire, probe, release */
    FuriHalSerialHandle* h =
        furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!h) return false;

    furi_hal_serial_init(h, WIFI_UART_BAUD);
    bool found = wifi_do_detect(h);
    furi_hal_serial_deinit(h);
    furi_hal_serial_control_release(h);
    return found;
}

bool radio_wifi_init(void) {
    if(wifi_initialized) return true;
    if(wifi_serial) return false; /* handle already held without being init'd — shouldn't happen */

    wifi_serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!wifi_serial) return false;

    furi_hal_serial_init(wifi_serial, WIFI_UART_BAUD);

    /* Tell the companion to enter raw-injection mode */
    char   cmd[8];
    size_t len = wifi_proto_build_on(cmd, sizeof(cmd));
    if(len > 0u) {
        wifi_tx_str(wifi_serial, cmd, len);
    }

    wifi_initialized = true;
    return true;
}

void radio_wifi_deinit(void) {
    if(!wifi_serial) return;

    if(wifi_initialized) {
        /* Tell companion to exit injection mode before closing the port */
        char   cmd[8];
        size_t len = wifi_proto_build_off(cmd, sizeof(cmd));
        if(len > 0u) {
            wifi_tx_str(wifi_serial, cmd, len);
        }
        wifi_initialized = false;
    }

    furi_hal_serial_deinit(wifi_serial);
    furi_hal_serial_control_release(wifi_serial);
    wifi_serial = NULL;
}

bool radio_wifi_emit(const char* ssid, const uint8_t bssid[6]) {
    if(!wifi_serial || !wifi_initialized) return false;
    if(!ssid || ssid[0] == '\0') return false;

    char   cmd[WIFI_CMD_BUF_SIZE];
    size_t len = wifi_proto_build_beacon(cmd, sizeof(cmd), ssid, bssid);
    if(len == 0u) return false;

    wifi_tx_str(wifi_serial, cmd, len);
    return true;
}
