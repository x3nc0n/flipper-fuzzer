#include "status_led.h"

#include <stdatomic.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO CONFIG_FLUCKFLOCK_STATUS_LED_GPIO

static atomic_int s_state = FF_LED_BOOTING;

static void blink(int on_ms, int off_ms) {
    gpio_set_level(LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    gpio_set_level(LED_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

static void status_led_task(void *arg) {
    (void)arg;
    while (1) {
        switch ((ff_led_state_t)atomic_load(&s_state)) {
            case FF_LED_BOOTING:
                blink(100, 100);   // fast blink while radios are starting up
                break;
            case FF_LED_BROADCASTING:
                blink(50, 1450);   // slow heartbeat: alive + actively rotating
                break;
            case FF_LED_ERROR_WIFI:
                blink(300, 300);  // single slow blink pattern: one radio down
                break;
            case FF_LED_ERROR_BLE:
                blink(300, 300);
                blink(300, 700);  // double blink: the other radio down
                break;
            case FF_LED_ERROR_BOTH:
                blink(150, 150);
                blink(150, 150);
                blink(150, 850);  // triple blink: nothing came up
                break;
        }
    }
}

void status_led_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    xTaskCreate(status_led_task, "ff_led", 2048, NULL, 3, NULL);
}

void status_led_set_state(ff_led_state_t state) {
    atomic_store(&s_state, (int)state);
}

void status_led_notify_broadcast_pulse(void) {
    status_led_set_state(FF_LED_BROADCASTING);
}
