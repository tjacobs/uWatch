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

#include "img/Font34.h"
#include "img/Font30.h"
#include "img/FontAvenir60.h"

// Start date time
datetime_t default_time = {
    .year  = 2023,
    .month = 2,
    .day   = 1,
    .dotw  = 3, // 0 is Sunday
    .hour  = 12,
    .min   = 0,
    .sec   = 0
};

// Screen positions
#define POS_TIME_X 64
#define POS_TIME_Y 120
#define POS_DATE_X 46
#define POS_DATE_Y 66
#define POS_DOW_X  20
#define POS_DOW_Y  111
#define POS_IMU_X  60
#define POS_IMU_Y  40
#define COLUMN_WIDTH 14

// Buttons
#define BUTTON_A 22
#define BUTTON_B 3

// Clock
#define CCLK 16
#define CDT 17
#define CSW 19

// Date
char* week[7] = {"Sun\0", "Mon\0", "Tue\0", "Wed\0", "Thu\0", "Fri\0", "Sat\0"};

// Buttons
bool button_a_pressed;
bool button_b_pressed;

// IMU
float acc[3];
float gyro[3];

// Sensors
float temperature = 0;

// Battery
float battery_voltage = 0;

// Text buffer
char buffer[10];

// Screen
bool draw_text_enabled = true;
DOImage* screen_image;

// Battery
typedef struct battery_t {
    float voltage;
    float min;
    float max;
} battery_t;

// Data storage
typedef struct {
    datetime_t datetime;
    battery_t battery;
    uint8_t brightness;
    uint8_t save_crc;
} DATA_t;
static __attribute__((section (".noinit")))char data_buffer[4096];
static DATA_t* data = (DATA_t*)data_buffer;

// Functions
void gpio_callback(uint gpio, uint32_t events);
void draw_init();
void draw_screen();

int main(void) {
    // Init
    stdio_init_all();

    // Init screen
    lcd_init();
    lcd_make_cosin();
    draw_init();
    lcd_set_brightness(100);
    lcd_setimg((uint16_t*)(malloc(LCD_SZ)));

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

    // Init IMU
    QMI8658_init();

    // Draw screen
    draw_screen();

    // Done
    return 0;
}

void draw_init() {
    screen_image = DOImage_new(240 - (32 + 16), 120 - 16, 32, 32, BLACK, 0);
}

void draw_screen() {
    // Draw text
    if (!draw_text_enabled) return;

    // Draw IMU
    if (true) {
        lcd_str(POS_IMU_X,        POS_IMU_Y +   1, "GYR_X =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y +  15, "GYR_Y =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y +  48, "GYR_Z =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y +  98, "ACC_X =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y + 130, "ACC_Y =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y + 144, "ACC_Z =",   &Font12, WHITE, BLACK);
        lcd_str(POS_IMU_X,        POS_IMU_Y + 156, "TEMP  =",   &Font12, WHITE, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y +   1, gyro[0],     &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y +  15, gyro[1],     &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y +  48, gyro[2],     &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y +  98, acc[0],      &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y + 130, acc[1],      &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y + 144, acc[2],      &Font12, YELLOW, BLACK);
        lcd_float(POS_IMU_X + 70, POS_IMU_Y + 156, temperature, &Font12, YELLOW, BLACK);
    }

    // Buttons
    sprintf(buffer, "A: %d B: %d", button_a_pressed, button_b_pressed);
    lcd_str(100, 190, buffer, &Font16, ORANGE, BLACK);

    // Battery
    lcd_str(50, 208, "BATTERY", &Font16, WHITE, BLACK);
    lcd_floatshort(130, 208, data->battery.voltage, &Font16, ORANGE, BLACK);

    // Day of the week
    lcd_str(POS_DOW_X, POS_DOW_Y, week[data->datetime.dotw], &Font16, WHITE, BLACK);

    // Day
    sprintf(buffer, "%2d", data->datetime.day);
    lcd_str(POS_DATE_X + 0 * COLUMN_WIDTH, POS_DATE_Y, buffer, &FontAvenir60, WHITE, BLACK);
    lcd_str(POS_DATE_X + 2 * COLUMN_WIDTH, POS_DATE_Y, ".",    &FontAvenir60, WHITE, BLACK);

    // Hour
    int hour = data->datetime.hour;
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;
    sprintf(buffer, "%2d", hour);
    lcd_str(POS_TIME_X + 0 * COLUMN_WIDTH, POS_TIME_Y, buffer, &FontAvenir60, WHITE, BLACK);

    // Min
    sprintf(buffer, "%02d", data->datetime.min);
    lcd_str(POS_TIME_X + 1 * COLUMN_WIDTH, POS_TIME_Y, ":",    &FontAvenir60, WHITE, BLACK);
    lcd_str(POS_TIME_X + 3 * COLUMN_WIDTH, POS_TIME_Y, buffer, &FontAvenir60, WHITE, BLACK);

    // Sec
    sprintf(buffer, "%02d", data->datetime.sec);
    lcd_str(POS_TIME_X + 7 * COLUMN_WIDTH, POS_TIME_Y, ":",    &FontAvenir60, WHITE, BLACK);
    lcd_str(POS_TIME_X + 6 * COLUMN_WIDTH, POS_TIME_Y, buffer, &FontAvenir60, WHITE, BLACK);
}

void gpio_callback(uint gpio, uint32_t events) {
    // Press
    if (events & GPIO_IRQ_EDGE_RISE) {
        // Buttons
        if (gpio == BUTTON_A) {
        }
    }
}

