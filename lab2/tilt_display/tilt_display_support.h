#pragma once

// Supplied support code for Task 3.
// Keep this file in the same folder as tilt_display.ino.
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include <string.h>

const int SDA_PIN = D4;  // XIAO ESP32S3: D4 is GPIO5.
const int SCL_PIN = D5;  // XIAO ESP32S3: D5 is GPIO6.
const uint32_t I2C_HZ = 400000;
const uint8_t IMU_ADDRESS = 0x68;   // AD0 low; CS high for I2C mode.
const uint8_t OLED_ADDRESS = 0x3C;
const int SCREEN_W = 128;
const int SCREEN_H = 64;
const int FRAME_BYTES = SCREEN_W * SCREEN_H / 8;
const float ACCEL_LSB_PER_G = 16384.0f;   // Explicitly configured to +/-2 g.
const float GYRO_LSB_PER_DPS = 131.0f;    // Explicitly configured to +/-250 dps.

struct RawSample {
  int16_t ax = 0, ay = 0, az = 0;
  int16_t gx = 0, gy = 0, gz = 0;
};

struct Sample {
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // degrees/second
};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

bool oledReady = false;
uint8_t cachedText[FRAME_BYTES] = {0};

// Combine two register bytes into one signed, two's-complement value.
int16_t combineBytes(uint8_t highByte, uint8_t lowByte) {
  uint16_t bits = (uint16_t(highByte) << 8) | lowByte;
  int32_t value = bits;
  if (bits & 0x8000) value -= 65536;
  return int16_t(value);
}

bool devicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(IMU_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// One register-address write followed by one repeated-start burst read.
// A failed/short read is rejected; callers must not decode its buffer.
bool readRegisters(uint8_t firstRegister, uint8_t *data, size_t count) {
  Wire.beginTransmission(IMU_ADDRESS);
  Wire.write(firstRegister);
  if (Wire.endTransmission(false) != 0) return false;

  size_t received = Wire.requestFrom(IMU_ADDRESS, count, true);
  if (received != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < count; i++) {
    int value = Wire.read();
    if (value < 0) return false;
    data[i] = uint8_t(value);
  }
  return true;
}

bool initializeImu() {
  uint8_t who = 0;
  if (!readRegisters(0x75, &who, 1)) return false;  // WHO_AM_I
  if (Serial && Serial.availableForWrite() >= 32) {
    Serial.printf("WHO_AM_I = 0x%02X\n", who);
  }
  if (who != 0x70) return false;  // This sketch expects an MPU-6500.

  if (!writeRegister(0x6B, 0x80)) return false;  // Reset.
  delay(100);
  if (!writeRegister(0x6B, 0x01)) return false;  // Wake; use PLL clock.
  if (!writeRegister(0x6C, 0x00)) return false;  // Enable all axes.
  if (!writeRegister(0x1A, 0x03)) return false;  // Gyro DLPF: 41 Hz.
  if (!writeRegister(0x1B, 0x00)) return false;  // +/-250 dps; DLPF enabled.
  if (!writeRegister(0x1C, 0x00)) return false;  // Accel: +/-2 g.
  if (!writeRegister(0x1D, 0x03)) return false;  // Accel DLPF: 41 Hz.
  if (!writeRegister(0x19, 0x09)) return false;  // Internal updates: 100 Hz.
  delay(50);

  // Check that the requested configuration reached the device.
  uint8_t config[5];
  if (!readRegisters(0x19, config, sizeof(config))) return false;
  return config[0] == 0x09 && config[1] == 0x03 &&
         config[2] == 0x00 && config[3] == 0x00 && config[4] == 0x03;
}

void stopWithError(const char *message) {
  if (oledReady) {
    display.clearBuffer();
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(0, 12, "Initialization stopped");
    display.drawStr(0, 28, message);
    display.drawStr(0, 44, "Check wiring; then reset.");
    display.sendBuffer();
  }
  // Repeat so the message is still visible if Serial Monitor opens late.
  while (true) {
    if (Serial && Serial.availableForWrite() >= int(strlen(message) + 2)) {
      Serial.println(message);
    }
    delay(1000);
  }
}

void initializeHardware() {
  if (!Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ)) {
    stopWithError("I2C bus init failed");
  }
  Wire.setTimeOut(25);
  if (!devicePresent(OLED_ADDRESS)) stopWithError("OLED 0x3C not found");

  display.setI2CAddress(OLED_ADDRESS << 1);  // U8g2 uses an 8-bit address.
  display.setBusClock(I2C_HZ);
  display.begin();
  Wire.setClock(I2C_HZ);
  oledReady = true;
  if (!initializeImu()) stopWithError("MPU-6500 init failed");
}

Sample scaleSample(const RawSample &raw) {
  Sample value;
  value.ax = raw.ax / ACCEL_LSB_PER_G;
  value.ay = raw.ay / ACCEL_LSB_PER_G;
  value.az = raw.az / ACCEL_LSB_PER_G;
  value.gx = raw.gx / GYRO_LSB_PER_DPS;
  value.gy = raw.gy / GYRO_LSB_PER_DPS;
  value.gz = raw.gz / GYRO_LSB_PER_DPS;
  return value;
}

// Build an unshifted picture from the most recent sample.
// The 5-pixel-wide font and columns at x=0/64 keep every value on screen.
void cacheText(const Sample &value, const char *status) {
  display.clearBuffer();
  display.setFont(u8g2_font_5x8_tf);
  display.drawStr(0, 8, "Accel (g)");
  display.drawStr(64, 8, "Gyro (dps)");
  char line[20];
  snprintf(line, sizeof(line), "Ax:%6.2f", value.ax);
  display.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "Ay:%6.2f", value.ay);
  display.drawStr(0, 32, line);
  snprintf(line, sizeof(line), "Az:%6.2f", value.az);
  display.drawStr(0, 44, line);
  snprintf(line, sizeof(line), "Gx:%7.1f", value.gx);
  display.drawStr(64, 20, line);
  snprintf(line, sizeof(line), "Gy:%7.1f", value.gy);
  display.drawStr(64, 32, line);
  snprintf(line, sizeof(line), "Gz:%7.1f", value.gz);
  display.drawStr(64, 44, line);
  display.drawStr(0, 60, status);
  memcpy(cachedText, display.getBufferPtr(), FRAME_BYTES);
}

// The SSD1306 stores eight vertical pixels per byte.
// Shift the cached picture directly into the display buffer, wrapping both axes.
void renderWrapped(float scrollX, float scrollY) {
  int offsetX = int(lroundf(scrollX)) % SCREEN_W;
  int offsetY = int(lroundf(scrollY)) % SCREEN_H;
  if (offsetX < 0) offsetX += SCREEN_W;
  if (offsetY < 0) offsetY += SCREEN_H;

  uint8_t *destination = display.getBufferPtr();
  for (int page = 0; page < SCREEN_H / 8; ++page) {
    const int sourceY = (page * 8 - offsetY + SCREEN_H) % SCREEN_H;
    const int sourcePage = sourceY / 8;
    const int bitShift = sourceY % 8;
    const uint8_t *first = cachedText + sourcePage * SCREEN_W;
    const uint8_t *second = cachedText +
        ((sourcePage + 1) % (SCREEN_H / 8)) * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++) {
      const int sourceX = (x - offsetX + SCREEN_W) % SCREEN_W;
      destination[page * SCREEN_W + x] = uint8_t(
          (uint16_t(first[sourceX]) >> bitShift) |
          (uint16_t(second[sourceX]) << (8 - bitShift)));
    }
  }
  display.sendBuffer();
}

float wrapPosition(float position, int extent) {
  position = fmodf(position, float(extent));
  if (position < 0) position += extent;
  return position;
}
