// Lab 2, Task 1: a software clock on an I2C OLED.
// XIAO ESP32S3, SSD1306 128x64, U8g2 library (U8x8 text interface).
// Complete TODO 1.1 through 1.5.
#include <Arduino.h>
#include <U8x8lib.h>
#include <Wire.h>

const int OLED_SDA = D4;  // GPIO5
const int OLED_SCL = D5;  // GPIO6
const uint8_t OLED_ADDRESS = 0x3C;
// Constructor order: reset, clock, data.
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

const int TOUCH1_PIN = D1;  // GPIO2: enter/exit setting mode
const int TOUCH2_PIN = D3;  // GPIO4: select next field
const int TOUCH3_PIN = D8;  // GPIO7: increment selected field
// TODO 1.1: Calibrate each threshold using released and touched readings.
// The initial values are starting points; touch readings increase on ESP32-S3.
const uint32_t THRESHOLD_1 = 200000;
const uint32_t THRESHOLD_2 = 200000;
const uint32_t THRESHOLD_3 = 200000;
const uint32_t DEBOUNCE_MS = 40;
const int FIRST_YEAR = 2025;
const int LAST_YEAR = 2099;

struct DateTime {
  int year, month, day, hour, minute, second;
};
DateTime nowDT = {2025, 1, 1, 12, 0, 0};

enum Field : uint8_t { F_YEAR, F_MONTH, F_DAY, F_HOUR, F_MIN, F_SEC };
uint8_t selectedField = F_YEAR;
bool timeSetMode = false;
bool blinkOn = true;
uint32_t lastTickMs = 0;
uint32_t lastBlinkMs = 0;

void displayDateTime(const char *dateLine, const char *timeLine) {
  // TODO 1.2: Use u8x8.setCursor(column, row) and u8x8.print(text).
  // Print dateLine at (0, 1), then timeLine at (0, 3).
  // The strings already include formatting and the selected-field blink.
  // YOUR CODE HERE
  (void)dateLine;
  (void)timeLine;
}

void toggleSetMode() {
  // TODO 1.3: Toggle the Boolean timeSetMode using logical NOT (!).
  // Timing and blink resets are handled in loop().
  // YOUR CODE HERE
}

void nextField() {
  // TODO 1.4: Advance selectedField and wrap after F_SEC to F_YEAR.
  // Fields are numbered 0 through 5; modulo (%) is useful here.
  // YOUR CODE HERE
}

void incrementField(uint8_t field) {
  // TODO 1.5: Increment only the selected field, with wraparound.
  // Year: FIRST_YEAR..LAST_YEAR; month: 1..12; day: 1..daysInMonth(...).
  // Hour: 0..23; minute/second: 0..59. Use switch(field) or if/else.
  // Incrementing an edited field must not carry into a different field.
  // YOUR CODE HERE
  (void)field;
  // Keep a valid date after changing the month or year.
  clampDay();
}

// Support code below is provided.

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month) {
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

void clampDay() {
  const int lastDay = daysInMonth(nowDT.year, nowDT.month);
  if (nowDT.day > lastDay) nowDT.day = lastDay;
}

void advanceOneSecond() {
  if (++nowDT.second < 60) return;
  nowDT.second = 0;
  if (++nowDT.minute < 60) return;
  nowDT.minute = 0;
  if (++nowDT.hour < 24) return;
  nowDT.hour = 0;
  if (++nowDT.day <= daysInMonth(nowDT.year, nowDT.month)) return;
  nowDT.day = 1;
  if (++nowDT.month <= 12) return;
  nowDT.month = 1;
  ++nowDT.year;
}

uint8_t readTouchPress() {
  const uint32_t raw1 = touchRead(TOUCH1_PIN);
  const uint32_t raw2 = touchRead(TOUCH2_PIN);
  const uint32_t raw3 = touchRead(TOUCH3_PIN);
  const bool t1 = raw1 > THRESHOLD_1;
  const bool t2 = raw2 > THRESHOLD_2;
  const bool t3 = raw3 > THRESHOLD_3;
  const uint8_t mask = (t1 ? 1 : 0) | (t2 ? 2 : 0) | (t3 ? 4 : 0);
  const uint32_t nowMs = millis();

  static uint32_t lastPrintMs = 0;
  if (nowMs - lastPrintMs >= 100) {
    lastPrintMs = nowMs;
    Serial.printf("raw: %lu %lu %lu | touched: %d %d %d\n",
                  (unsigned long)raw1, (unsigned long)raw2,
                  (unsigned long)raw3, t1, t2, t3);
  }

  static uint8_t candidateMask = 0;
  static uint32_t changedAtMs = nowMs;
  static bool armed = false;
  if (mask != candidateMask) {
    candidateMask = mask;
    changedAtMs = nowMs;
  }
  if (nowMs - changedAtMs < DEBOUNCE_MS) return 0;

  if (candidateMask == 0) {
    armed = true;
    return 0;
  }
  if (!armed) return 0;
  armed = false;  // Any gesture requires a full release before another press.
  if (candidateMask == 1) return 1;
  if (candidateMask == 2) return 2;
  if (candidateMask == 4) return 3;
  return 0;  // Ignore simultaneous touches.
}

void printStatusLine(uint8_t row, const char *text) {
  char line[17];
  snprintf(line, sizeof(line), "%-16.16s", text);
  u8x8.setCursor(0, row);
  u8x8.print(line);
}

void drawClock() {
  char dateLine[17];
  char timeLine[17];
  snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d",
           nowDT.year, nowDT.month, nowDT.day);
  snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d",
           nowDT.hour, nowDT.minute, nowDT.second);

  // Hide only the selected field during the blink's off phase.
  if (timeSetMode && !blinkOn) {
    switch (selectedField) {
      case F_YEAR:
        for (int i = 0; i < 4; ++i) dateLine[i] = ' ';
        break;
      case F_MONTH: dateLine[5] = dateLine[6] = ' '; break;
      case F_DAY: dateLine[8] = dateLine[9] = ' '; break;
      case F_HOUR: timeLine[0] = timeLine[1] = ' '; break;
      case F_MIN: timeLine[3] = timeLine[4] = ' '; break;
      case F_SEC: timeLine[6] = timeLine[7] = ' '; break;
    }
  }
  displayDateTime(dateLine, timeLine);

  if (timeSetMode) {
    const char *names[] = {"YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND"};
    char status[17];
    snprintf(status, sizeof(status), "SET: %s", names[selectedField]);
    printStatusLine(5, status);
    printStatusLine(6, "T1:Exit T2:Next");
    printStatusLine(7, "T3:+1");
  } else {
    printStatusLine(5, "RUNNING");
    printStatusLine(6, "T1:Set time");
    printStatusLine(7, "");
  }
}

void setup() {
  Serial.begin(115200);
  // U8x8 takes the 8-bit address: shift the 7-bit I2C address left by one.
  u8x8.setI2CAddress(OLED_ADDRESS << 1);
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clearDisplay();

  lastTickMs = millis();
  lastBlinkMs = lastTickMs;
  drawClock();
}

void loop() {
  const uint8_t press = readTouchPress();
  const uint32_t nowMs = millis();
  bool redraw = false;

  if (timeSetMode) {
    lastTickMs = nowMs;  // Paused time must not accumulate.
  } else {
    while (nowMs - lastTickMs >= 1000) {
      lastTickMs += 1000;
      advanceOneSecond();
      redraw = true;
    }
  }

  if (press == 1 || (timeSetMode && (press == 2 || press == 3))) {
    if (press == 1) toggleSetMode();
    if (press == 2) nextField();
    if (press == 3) incrementField(selectedField);
    lastTickMs = nowMs;  // Resume with a fresh one-second interval.
    blinkOn = true;
    lastBlinkMs = nowMs;
    redraw = true;
  }

  if (timeSetMode && nowMs - lastBlinkMs >= 500) {
    lastBlinkMs = nowMs;
    blinkOn = !blinkOn;
    redraw = true;
  }
  if (redraw) drawClock();
  delay(5);
}
