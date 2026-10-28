#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "index.h"

// Provided: startup, webpage rendering, and request processing.
constexpr int LED_PIN = 21;
WebServer server(80);
bool ledState = false;
bool serverReady = false;

bool startAccessPoint();
void registerLedRoutes();
void handleLEDCommand(bool turnOn);

void handleLEDOn() { handleLEDCommand(true); }
void handleLEDOff() { handleLEDCommand(false); }

void handleRoot() {
  String page = INDEX_HTML;
  page.replace("%LED_STATUS%", ledState ? "ON" : "OFF");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", page);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  if (!startAccessPoint()) {
    Serial.println("AP did not start. Complete TODO 1.1 and check config.h.");
    return;
  }
  server.on("/", HTTP_GET, handleRoot);
  registerLedRoutes();
  server.onNotFound([]() { server.send(404, "text/plain", "Page not found"); });
  server.begin();
  serverReady = true;
  Serial.printf("Wi-Fi: %s\n", AP_SSID);
  Serial.printf("Open http://%s/\n", WiFi.softAPIP().toString().c_str());
}

void loop() {
  if (serverReady) server.handleClient();
  delay(1);
}
