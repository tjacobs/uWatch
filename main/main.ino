// Board: Raspberry Pi Pico
// Core: Arduino RP2040 board

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

// Start date time
datetime_t default_time = {
    .year  = 2023,
    .month = 2,
    .day   = 1,
    .dotw  = 3,
    .hour  = 12,
    .min   = 0,
    .sec   = 0
};

// Screen positions
#define POS_TIME_X 40
#define POS_TIME_Y 90
#define POS_DOW_X  30
#define POS_DOW_Y  140
#define POS_DATE_X 40
#define POS_DATE_Y 60
#define POS_IMU_X  60
#define POS_IMU_Y  40

// Date
char* week[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Battery
typedef struct battery_t {
    float voltage;
    float minimum;
    float maximum;
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

// Colors
#define RED     0xF000
#define GREEN   0x0A00
#define BLUE    0x00AF
#define CYAN    0x00FA
#define YELLOW  0xFFF0
#define MAGENTA 0xFF00
#define WHITE   0xFFFF
#define BLACK   0x0000

// Screen
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_DC 9
#define SCREEN_CS 10
Adafruit_GC9A01A tft = Adafruit_GC9A01A(SCREEN_CS, SCREEN_DC);

// Canvas
#define CANVAS_WIDTH 180
#define CANVAS_HEIGHT 50
GFXcanvas1 canvas(CANVAS_WIDTH, CANVAS_HEIGHT);

void setup() {
  // Start screen
  tft.begin();

  // Clear screen
  tft.fillScreen(BLACK);
  tft.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, SCREEN_WIDTH / 2 - 1, WHITE);

  // Test
  data->datetime.hour = 12 + 3;
  data->datetime.min = 30;
  data->datetime.dotw = 0;
}

void loop(void) {
  // Draw screen
  drawScreen();

  // Test
  data->datetime.min++;  if (data->datetime.min > 60)  data->datetime.min = 0;
  data->datetime.dotw++; if (data->datetime.dotw >= 7) data->datetime.dotw = 0;

  // Wait
  delay(5000);
}

void drawScreen() {
  // Set text
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextColor(WHITE);
  canvas.setTextSize(1);

  // Clear
  canvas.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, BLACK);

  // Hour
  char* ampm = "AM";
  int hour = data->datetime.hour;
  if (hour > 12) {hour -= 12; ampm = "PM";}
  if (hour == 0) hour = 12;
  canvas.setCursor(0, CANVAS_HEIGHT - 5);
  canvas.print(hour);

  // Minute
  int minute = data->datetime.min;
  canvas.print(":");
  if (minute < 10) canvas.print("0");
  canvas.print(minute);

  // AM PM
  canvas.print(ampm);

  // Draw time to screen
  tft.drawBitmap(POS_TIME_X, POS_TIME_Y, canvas.getBuffer(), CANVAS_WIDTH, CANVAS_HEIGHT, WHITE, BLACK);

  // Clear
  canvas.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, BLACK);

  // Day of week
  canvas.setFont(&FreeSansBold9pt7b);
  canvas.setCursor(60, 30);
  canvas.print(week[data->datetime.dotw]);

  // Draw day to screen
  tft.drawBitmap(POS_DOW_X, POS_DOW_Y, canvas.getBuffer(), CANVAS_WIDTH, CANVAS_HEIGHT, YELLOW, BLACK);
}
