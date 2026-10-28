#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include "config.h"
#include "index.h"
#include "style.h"
#include "script.h"

// Provided: camera setup, network setup, and sending all bytes of a photo.
WebServer http(80);
camera_fb_t *captureJpeg();
void handleCapture();

bool initCamera() {
  if (!psramFound()) {
    Serial.println("Enable Tools > PSRAM > OPI PSRAM, then upload again.");
    return false;
  }
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // 640 x 480 pixels.
  config.jpeg_quality = 12;          // Lower values give higher JPEG quality.
  config.fb_location = CAMERA_FB_IN_PSRAM;
  // Keep recent frames available while the browser waits between captures.
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", error);
    return false;
  }
  Serial.println("Camera ready: VGA JPEG.");
  return true;
}

// A network write may send only part of the buffer. Retry the remaining bytes.
// This function returns after sending the photo or closing a failed transfer.
bool writePhotoBytes(const uint8_t *data, size_t length) {
  NetworkClient client = http.client();
  const uint32_t startedAt = millis();
  size_t sent = 0;
  while (sent < length && client.connected() && millis() - startedAt < 10000) {
    const size_t remaining = length - sent;
    const size_t chunk = remaining < 2048 ? remaining : 2048;
    const size_t count = client.write(data + sent, chunk);
    sent += count;
    delay(1);
  }
  Serial.printf("Photo: %u bytes; sent: %u bytes\n", (unsigned)length, (unsigned)sent);
  if (sent != length) {
    client.stop();
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nLab 5 Task 3: web camera");
  if (!initCamera()) {
    while (true) {
      Serial.println("Check the camera connector and PSRAM setting, then reset.");
      delay(3000);
    }
  }
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    while (true) {
      Serial.println("Wi-Fi AP could not start. Check the name and password in config.h.");
      delay(3000);
    }
  }
  Serial.printf("Wi-Fi: %s\n", AP_SSID);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());

  http.on("/", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  http.on("/style.css", HTTP_GET, []() {
    http.send_P(200, "text/css; charset=utf-8", STYLE_CSS);
  });
  http.on("/script.js", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "application/javascript; charset=utf-8", SCRIPT_JS);
  });
  http.on("/capture", HTTP_GET, handleCapture);
  http.onNotFound([]() { http.send(404, "text/plain", "Not found"); });
  http.begin();
}

void loop() {
  http.handleClient();
}
