// Task 2: compare MPU-6500 register reads over SPI and I2C.
// Board: XIAO ESP32S3. Serial Monitor: 115200 baud.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// Set to 1 for SPI or 0 for I2C. Rewire and power-cycle before switching buses.
#ifndef LAB2_USE_SPI
#define LAB2_USE_SPI 1
#endif
const bool USE_SPI = LAB2_USE_SPI != 0;
const uint16_t TARGET_HZ = 200; // Try 100, 200, 500, and 1000.
const uint32_t SPI_CLOCK_HZ = 1000000;
const uint32_t I2C_CLOCK_HZ = 400000;
const bool WAIT_FOR_DATA_READY = true;
const bool SHOW_VALUES = true; // Print every completed read; set false for timing comparisons.

#include "imu_compare_support.h"

uint8_t spiReadAddress(uint8_t reg) {
  // TODO 2.1: SPI reads set bit 7 of the register address.
  // Use bitwise OR (|) with 0x80.
  return 0x80; // YOUR CODE HERE
}

bool selectI2cRegister(uint8_t reg) {
  // TODO 2.2: Send the register address and keep the bus for a repeated START.
  Wire.beginTransmission(IMU_ADDR);
  Wire.write((uint8_t)0x00); // YOUR CODE HERE: send reg.
  return Wire.endTransmission(true) == 0; // YOUR CODE HERE: use false.
}

uint8_t sampleDivider(uint16_t rateHz) {
  // TODO 2.3: rateHz = 1000 / (1 + DIV). Return DIV for the requested rate.
  return 0; // YOUR CODE HERE
}

void decodeFrame(const uint8_t *raw, RawSample &sample) {
  // TODO 2.4: Use combineBytes(high, low) for each signed 16-bit axis.
  // Bytes 0..5 are accel, 6..7 temperature, and 8..13 gyro.
  sample.ax = 0; // YOUR CODE HERE
  sample.ay = 0; // YOUR CODE HERE
  sample.az = 0; // YOUR CODE HERE
  sample.gx = 0; // YOUR CODE HERE
  sample.gy = 0; // YOUR CODE HERE
  sample.gz = 0; // YOUR CODE HERE
}

float rateFromCount(uint32_t count, uint32_t elapsedMs) {
  // TODO 2.5: Convert elapsedMs to seconds and return reads per second.
  if (elapsedMs == 0) return 0.0f;
  return 0.0f; // YOUR CODE HERE
}

