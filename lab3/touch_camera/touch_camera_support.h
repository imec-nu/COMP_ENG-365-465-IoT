#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "esp_camera.h"

// XIAO ESP32S3 Sense expansion board; no external camera or SD wiring.
const uint8_t TOUCH_PIN = D0;  // GPIO1
const uint8_t SD_CS = 21;     // Also used by the built-in LED; leave LED control off.
const uint8_t SD_SCK = D8;
const uint8_t SD_MISO = D9;
const uint8_t SD_MOSI = D10;
const uint8_t DEBOUNCE_COUNT = 3;
const uint32_t TOUCH_INTERVAL_MS = 30;
const uint32_t IMAGE_NUMBER_LIMIT = 1000000;

struct TouchState {
  uint8_t touchCount = 0;
  uint8_t releaseCount = 0;
  bool latched = false;
};

bool captureRequested(bool touched, TouchState &state);
bool nextImagePath(char *path, size_t capacity, uint32_t &number);
bool capturePhoto(const char *path);

// Formatting is supplied; deciding which filename is unused is your task.
bool formatImagePath(char *path, size_t capacity, uint32_t number) {
  const int count = snprintf(path, capacity, "/image_%04lu.jpg", (unsigned long)number);
  return count > 0 && (size_t)count < capacity;
}

// With one framebuffer, an image may have been queued before the touch.
// Return that image and acquire a new one. The caller owns the returned frame.
camera_fb_t *captureFreshFrame() {
  camera_fb_t *oldFrame = esp_camera_fb_get();
  if (!oldFrame) return nullptr;
  esp_camera_fb_return(oldFrame);
  return esp_camera_fb_get();
}

bool initCamera() {
  if (!psramFound()) {
    Serial.println("Enable Tools > PSRAM > OPI PSRAM, then upload again.");
    return false;
  }
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 15;
  config.pin_d1 = 17;
  config.pin_d2 = 18;
  config.pin_d3 = 16;
  config.pin_d4 = 14;
  config.pin_d5 = 12;
  config.pin_d6 = 11;
  config.pin_d7 = 48;
  config.pin_xclk = 10;
  config.pin_pclk = 13;
  config.pin_vsync = 38;
  config.pin_href = 47;
  config.pin_sccb_sda = 40;
  config.pin_sccb_scl = 39;
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_XGA;  // 1024 x 768
  config.jpeg_quality = 15;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("CAMERA_INIT_FAIL: 0x%x\n", error);
    return false;
  }
  Serial.println("CAMERA_INIT_OK: JPEG, 1024 x 768");
  return true;
}

bool initSD() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI) || SD.cardType() == CARD_NONE) {
    Serial.println("SD_INIT_FAIL: check the card and FAT32 format.");
    return false;
  }
  Serial.println("SD_INIT_OK");
  return true;
}

uint32_t calibrateTouch() {
  uint64_t total = 0;
  for (uint8_t i = 0; i < 30; ++i) {
    total += touchRead(TOUCH_PIN);
    delay(30);
  }
  return (uint32_t)(total / 30);
}

TouchState touchState;
uint32_t touchBaseline = 0;
uint32_t imageNumber = 1;
bool hardwareReady = false;

void setup() {
  Serial.begin(115200);
  const uint32_t startMs = millis();
  while (!Serial && millis() - startMs < 1500) delay(10);

  const bool sdReady = initSD();
  const bool cameraReady = initCamera();
  if (!sdReady || !cameraReady) return;

  Serial.println("Leave the D0 wire untouched during calibration.");
  delay(1000);
  touchBaseline = calibrateTouch();
  if (touchBaseline == 0) {
    Serial.println("TOUCH_CALIBRATION_FAIL");
    return;
  }
  Serial.printf("baseline=%lu threshold=%lu\n", (unsigned long)touchBaseline,
                (unsigned long)(touchBaseline * TOUCH_THRESHOLD_RATIO));
  hardwareReady = true;
  Serial.println("Ready. Touch D0 to take a photo; release before the next touch.");
}

void loop() {
  if (!hardwareReady) {
    Serial.println("Initialization failed. Read the startup messages, then reset.");
    delay(2000);
    return;
  }

  const uint32_t raw = touchRead(TOUCH_PIN);
  const uint32_t threshold = (uint32_t)(touchBaseline * TOUCH_THRESHOLD_RATIO);
  const bool touched = raw > threshold;
  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 250) {
    lastPrintMs = millis();
    Serial.printf("raw=%lu threshold=%lu touched=%d\n", (unsigned long)raw,
                  (unsigned long)threshold, touched);
  }

  if (captureRequested(touched, touchState)) {
    char path[32];
    if (!nextImagePath(path, sizeof(path), imageNumber)) {
      Serial.println("No unused filename found. Check TODO 4.2 or the file limit.");
    } else if (capturePhoto(path)) {
      Serial.printf("PHOTO_SAVED: %s\n", path);
      ++imageNumber;
    } else {
      Serial.println("CAPTURE_OR_SAVE_FAILED");
    }
  }
  delay(TOUCH_INTERVAL_MS);
}
