// uWatch
// Tom Jacobs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/util/datetime.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/types.h"
#include "pico/bootrom/sf_table.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "hardware/gpio.h"
#include <hardware/flash.h>
#include "hardware/watchdog.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include "lcd.h"
#include "lib/draw.h"

#include "QMI8658.h"

// Screen
uint8_t* screen = NULL;

// Buttons
#define BUTTON_A 22
#define BUTTON_B 3

// Functions
void gpio_callback(uint gpio, uint32_t events);
void draw_init();

// Clock
#define CCLK 16
#define CDT 17
#define CSW 19

int main(void) {
    // Init
    stdio_init_all();
    lcd_init();
    lcd_make_cosin();
    draw_init();
    lcd_set_brightness(100);

    // Allocate screen image
    screen = malloc(LCD_SZ);
    lcd_setimg((uint16_t*)screen);

    // Init buttons
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled(BUTTON_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled_with_callback(CCLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Init realtime clock
    gpio_set_irq_enabled(CDT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CSW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    rtc_init();
    rtc_set_datetime(NULL);

    // IMU
    QMI8658_init();

    return 0;
}

void draw_init() {
    DOImage_new(240 - (32 + 16), 120 - 16, 32, 32, BLACK, 0);
}

void gpio_callback(uint gpio, uint32_t events) {
    // Press
    if (events & GPIO_IRQ_EDGE_RISE) {
        // Buttons
        if (gpio == BUTTON_A) {
        }
    }
}

