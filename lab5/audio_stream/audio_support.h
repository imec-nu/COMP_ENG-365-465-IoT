#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP_I2S.h>
#include "config.h"
#include "index.h"
#include "style.h"
#include "script.h"
#include "audio_support_js.h"
#include "lamejs.h"

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t SAMPLES_PER_PACKET = 512;
constexpr int PDM_CLK_PIN = 42;
constexpr int PDM_DATA_PIN = 41;

WebServer http(80);
WebSocketsServer ws(81);
I2SClass microphone;
uint8_t pcmPacket[SAMPLES_PER_PACKET * sizeof(int16_t)];
bool microphoneReady = false;

bool beginMicrophone();
void streamAudioPacket();

void onWebSocket(uint8_t clientId, WStype_t type, uint8_t *, size_t) {
  if (type == WStype_CONNECTED) Serial.printf("Browser %u connected\n", clientId);
  if (type == WStype_DISCONNECTED) Serial.printf("Browser %u disconnected\n", clientId);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    while (true) {
      Serial.println("AP setup failed. Check the SSID and password in config.h.");
      delay(1000);
    }
  }

  http.on("/", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  http.on("/index.html", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  http.on("/style.css", HTTP_GET, []() { http.send_P(200, "text/css", STYLE_CSS); });
  http.on("/script.js", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "application/javascript", SCRIPT_JS);
  });
  http.on("/support.js", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "application/javascript", AUDIO_SUPPORT_JS);
  });
  http.on("/lame.min.js", HTTP_GET, []() { http.send_P(200, "application/javascript", LAME_JS); });
  http.onNotFound([]() { http.send(404, "text/plain", "Not found"); });
  http.begin();
  ws.begin();
  ws.onEvent(onWebSocket);
  microphoneReady = beginMicrophone();

  Serial.printf("Wi-Fi: %s\n", AP_SSID);
  Serial.printf("Open http://%s/\n", WiFi.softAPIP().toString().c_str());
  if (microphoneReady) Serial.println("Microphone ready: 16000 Hz, signed 16-bit, mono.");
}

void loop() {
  http.handleClient();
  ws.loop();
  if (microphoneReady) {
    streamAudioPacket();
  } else {
    static uint32_t lastMessageMs = 0;
    if (millis() - lastMessageMs >= 2000) {
      Serial.println("Microphone not started. Complete TODO 2.1 and check the Sense expansion board.");
      lastMessageMs = millis();
    }
    delay(1);
  }
}
