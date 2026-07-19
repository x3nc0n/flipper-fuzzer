#include "console_cfg.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "ff_console";

static void print_config(void) {
    printf("rotation_interval_ms  = %u\n", (unsigned)g_ff_config.rotation_interval_ms);
    printf("jitter_ms             = %u\n", (unsigned)g_ff_config.jitter_ms);
    printf("wifi_beacons_per_tick = %u\n", (unsigned)g_ff_config.wifi_beacons_per_tick);
    printf("wifi_channel          = %u\n", (unsigned)g_ff_config.wifi_channel);
    printf("no_repeat_window      = %u\n", (unsigned)g_ff_config.no_repeat_window);
    printf("enable_wifi           = %s\n", g_ff_config.enable_wifi ? "true" : "false");
    printf("enable_ble            = %s\n", g_ff_config.enable_ble ? "true" : "false");
    printf("verbose_logging       = %s\n", g_ff_config.verbose_logging ? "true" : "false");
}

static void print_usage(void) {
    printf("usage: ffcfg show\n"
           "       ffcfg set <key> <value>\n"
           "       ffcfg save\n"
           "keys: rotation_interval_ms, jitter_ms, wifi_beacons_per_tick,\n"
           "      wifi_channel, no_repeat_window, enable_wifi, enable_ble,\n"
           "      verbose_logging\n");
}

static int cmd_ffcfg(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "show") == 0) {
        print_config();
        return 0;
    }

    if (strcmp(argv[1], "save") == 0) {
        esp_err_t err = fluckflock_config_save();
        if (err == ESP_OK) {
            printf("saved to NVS\n");
            return 0;
        }
        printf("save failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            print_usage();
            return 1;
        }
        const char *key = argv[2];
        const char *val = argv[3];

        if (strcmp(key, "rotation_interval_ms") == 0) {
            g_ff_config.rotation_interval_ms = (uint32_t)atoi(val);
        } else if (strcmp(key, "jitter_ms") == 0) {
            g_ff_config.jitter_ms = (uint32_t)atoi(val);
        } else if (strcmp(key, "wifi_beacons_per_tick") == 0) {
            g_ff_config.wifi_beacons_per_tick = (uint8_t)atoi(val);
        } else if (strcmp(key, "wifi_channel") == 0) {
            g_ff_config.wifi_channel = (uint8_t)atoi(val);
        } else if (strcmp(key, "no_repeat_window") == 0) {
            g_ff_config.no_repeat_window = (uint32_t)atoi(val);
        } else if (strcmp(key, "enable_wifi") == 0) {
            g_ff_config.enable_wifi = atoi(val) != 0;
        } else if (strcmp(key, "enable_ble") == 0) {
            g_ff_config.enable_ble = atoi(val) != 0;
        } else if (strcmp(key, "verbose_logging") == 0) {
            g_ff_config.verbose_logging = atoi(val) != 0;
        } else {
            printf("unknown key: %s\n", key);
            return 1;
        }

        printf("%s = %s (call 'ffcfg save' to persist; radio init options need a reboot)\n",
               key, val);
        return 0;
    }

    print_usage();
    return 1;
}

void console_cfg_start(void) {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "fluckflock>";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "console init failed: %s (field-tuning console disabled)",
                 esp_err_to_name(err));
        return;
    }

    esp_console_register_help_command();

    const esp_console_cmd_t ffcfg_cmd = {
        .command = "ffcfg",
        .help = "Show/set/save FluckFlock runtime config -- see 'ffcfg' with no args for usage",
        .hint = NULL,
        .func = &cmd_ffcfg,
    };
    err = esp_console_cmd_register(&ffcfg_cmd);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd register failed: %s", esp_err_to_name(err));
        return;
    }

    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "repl start failed: %s", esp_err_to_name(err));
    }
}
