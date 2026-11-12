#pragma once
#include "camera_frontend.h"

bool captureExample();
static uint32_t sequence = 0;
static uint32_t captureUs = 0, preprocessUs = 0;

static uint32_t crcUpdate(uint32_t crc, const uint8_t *data, size_t count) {
  while (count--) {
    crc ^= *data++;
    for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
  }
  return crc;
}

static bool sendAll(const void *data, size_t count) {
  const uint8_t *p = static_cast<const uint8_t *>(data);
  uint32_t last = millis();
  while (count) {
    size_t n = Serial.write(p, count > 1024 ? 1024 : count);
    if (n) { p += n; count -= n; last = millis(); }
    else if (millis() - last > 15000) return false;
    delay(1);
  }
  return true;
}

static camera_fb_t *captureTimed() {
  const uint32_t started = micros();
  camera_fb_t *fb = captureFresh();
  captureUs = micros() - started;
  return fb;
}

static bool prepareTimed(camera_fb_t *fb) {
  const uint32_t started = micros();
  const bool ok = prepareImage(fb);
  preprocessUs = micros() - started;
  return ok;
}

// The JPEG and input array share one header, sequence number, and CRC.
// This function sends the frame but does not return it to the camera.
static bool sendExample(camera_fb_t *fb) {
  uint32_t crc = crcUpdate(0xffffffff, fb->buf, fb->len);
  crc = crcUpdate(crc, grayInput, sizeof(grayInput)) ^ 0xffffffff;
  uint32_t header[] = {1, ++sequence, RAW_W, RAW_H, INPUT_W, INPUT_H,
      CROP_X, CROP_Y, CROP_SIZE, (uint32_t)fb->len, sizeof(grayInput),
      captureUs, preprocessUs, crc};
  return sendAll("GST7", 4) && sendAll(header, sizeof(header)) &&
      sendAll(fb->buf, fb->len) && sendAll(grayInput, sizeof(grayInput));
}

static void beginCollection() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  while (!Serial) delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  if (!initCameraFrontend()) {
    while (true) {
      Serial.println("# ERROR camera/PSRAM initialization failed; enable OPI PSRAM and check camera cable");
      delay(2000);
    }
  }
  Serial.println("# READY GST7 frontend=gesture_gray_v1");
}

static void pollCollection() {
  if (!Serial.available()) { delay(5); return; }
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command == "CAPTURE") {
    Serial.println("# CAPTURING");
    digitalWrite(LED_BUILTIN, LOW);
    const bool ok = captureExample();
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(ok ? "# DONE" : "# ERROR capture, preprocessing, or send failed; check TODO 2.1 and 2.2");
  } else if (command == "PING") {
    Serial.println("# READY GST7 frontend=gesture_gray_v1");
  } else {
    Serial.println("# ERROR expected CAPTURE or PING");
  }
}
