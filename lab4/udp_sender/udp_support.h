#pragma once
// Supplied Wi-Fi setup and periodic scheduling. No TODOs in this tab.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_system.h>
#include "config.h"

WiFiUDP udp;
IPAddress destination;
constexpr uint32_t SEND_PERIOD_MS = 500;
uint32_t sessionId;
uint32_t sequence = 0;
uint32_t lastAttemptMs = 0;
uint32_t lastReconnectMs = 0;
bool configured = false;
bool socketOpen = false;

bool sendDatagram(uint32_t number);

void setup() {
  Serial.begin(115200);
  delay(250);
  if (String(WIFI_SSID) == "YOUR_WIFI_SSID" || String(WIFI_PASSWORD) == "YOUR_WIFI_PASSWORD" ||
      !destination.fromString(COMPUTER_IPV4) || destination[0] == 127 || destination[0] == 0 ||
      destination[0] >= 224 || destination == IPAddress(255,255,255,255)) {
    Serial.println("Setup: fill Wi-Fi credentials and the computer's LAN IPv4 in config.h.");
    return;
  }
  sessionId = esp_random();  // A restart cannot be mistaken for sequence loss.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastReconnectMs = millis();
  configured = true;
  Serial.println("Joining Wi-Fi. Keep the computer on the same LAN.");
}

void loop() {
  if (!configured) { delay(20); return; }
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (socketOpen) { udp.stop(); socketOpen = false; }
    if (uint32_t(now - lastReconnectMs) >= 10000) {
      Serial.println("Still waiting for Wi-Fi; retrying.");
      WiFi.reconnect(); lastReconnectMs = now;
    }
    delay(10); return;
  }
  if (!socketOpen) {
    if (!udp.begin(4211)) { Serial.println("UDP socket failed."); delay(1000); return; }
    socketOpen = true;
    Serial.printf("Board IPv4: %s; sending to %s:%u; session=%08lx\n",
                  WiFi.localIP().toString().c_str(), COMPUTER_IPV4, COMPUTER_PORT,
                  (unsigned long)sessionId);
    lastAttemptMs = now;
  }
  if (uint32_t(now - lastAttemptMs) >= SEND_PERIOD_MS) {
    lastAttemptMs = now;  // Do not burst missed sends after a pause.
    const uint32_t current = sequence++;
    if (sendDatagram(current)) {
      Serial.printf("Queued L4 %08lx %lu\n", (unsigned long)sessionId, (unsigned long)current);
    } else {
      Serial.println("Datagram not sent. Complete 2.1 or check the socket.");
    }
  }
  delay(2);
}
