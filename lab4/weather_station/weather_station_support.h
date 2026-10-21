#pragma once
// Provided support code. Read as needed; no edits are required for this lab.
// Lab 4, Tasks 3 and 4: approximate IP location -> current weather -> ntfy.
// Board: XIAO_ESP32S3; esp32 core 3.3.12; Serial Monitor: 115200 baud.
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Wire.h>
#include <U8x8lib.h>
#include <time.h>
#include "config.h"
#include "weather_logic.h"

// OLED SSD1306 128x64, address 0x3C: VCC=3V3, GND=GND,
// SDA=D4/GPIO5, SCL=D5/GPIO6. U8x8 argument order is reset, clock, data.
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE, 6, 5);
constexpr uint32_t REFRESH_MS = 120000;
constexpr char GEO_URL[] = "http://ip-api.com/json/?fields=status,message,lat,lon,city";
Location location;
Weather lastWeather;
bool haveLocation = false;
bool haveWeather = false;
bool configured = false;
uint32_t lastAttemptMs = 0;
String lastPublishedBody;

// Implemented in weather_station.ino; support code calls these student functions.
bool readGET(HTTPClient& http, String& payload);
bool refreshWeather();
void notifyFreshWeather();

// Provided transport setup. Never print URLs containing your WeatherAPI key.
void prepareHTTP(HTTPClient& http) {
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.addHeader("User-Agent", "XIAO-Lab4/1.0");
}

bool fetchLocation(Location& result) {
  NetworkClient client;
  HTTPClient http;
  if (!http.begin(client, GEO_URL)) return false;
  prepareHTTP(http);
  String payload;
  const bool received = readGET(http, payload);
  http.end();
  return received && parseLocation(payload, result);
}

bool fetchWeather(const String& url, Weather& result) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // Verify the HTTPS server certificate.
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  prepareHTTP(http);
  String payload;
  const bool received = readGET(http, payload);
  http.end();
  return received && parseWeather(payload, result);
}

// Provided OLED layout: 16 columns x 8 rows with this 8x8 font.
// Long words wrap; text beyond the available rows ends with an ellipsis.
void drawWrapped(uint8_t row, uint8_t rows, const String& text) {
  unsigned int offset = 0;
  for (uint8_t r = 0; r < rows && offset < text.length(); ++r) {
    String line = text.substring(offset, offset + 16);
    offset += line.length();
    if (r == rows - 1 && offset < text.length())
      line = line.substring(0, 13) + "...";
    oled.drawString(0, row + r, line.c_str());
  }
}

void showStatus(const char* message) {
  oled.clearLine(7);
  oled.drawString(0, 7, message);  // Status labels are at most 16 characters.
}

void showWeather() {
  oled.clear();
  String place = lastWeather.place.length() ? lastWeather.place : location.city;
  drawWrapped(0, 1, place.length() ? place : String("IP location"));
  char line[17];
  snprintf(line, sizeof(line), "Lat: %.3f", location.latitude);
  oled.drawString(0, 1, line);
  snprintf(line, sizeof(line), "Lon: %.3f", location.longitude);
  oled.drawString(0, 2, line);
  snprintf(line, sizeof(line), "Temp: %.1f C", lastWeather.temperatureC);
  oled.drawString(0, 3, line);
  drawWrapped(4, 3, lastWeather.condition);
  showStatus("Current weather");
}

bool validTopic() {
  const String topic(NTFY_TOPIC);
  if (topic.length() == 0 || topic.length() > 64 || topic.startsWith("YOUR_"))
    return false;
  for (unsigned int i = 0; i < topic.length(); ++i) {
    const char c = topic[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  }
  return true;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000)
    delay(100);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.print("Wi-Fi connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool clockReady() {
  // Certificate validity checks need the real date. Keep certificate checking
  // enabled if NTP is blocked; use a network with NTP access instead.
  if (time(nullptr) > 1704067200) return true;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  const uint32_t started = millis();
  while (time(nullptr) <= 1704067200 && millis() - started < 10000)
    delay(100);
  return time(nullptr) > 1704067200;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  oled.setI2CAddress(0x3C * 2);  // U8x8 expects the shifted I2C address.
  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.clear();
  oled.drawString(0, 0, "Lab 4 weather");
  configured = String(WIFI_SSID).length() > 0 &&
               !String(WIFI_SSID).startsWith("YOUR_") &&
               String(WEATHER_API_KEY).length() > 0 &&
               !String(WEATHER_API_KEY).startsWith("YOUR_") &&
               (!ENABLE_NOTIFICATIONS || validTopic());
  if (!configured) {
    Serial.println("Edit config.h before running the lab.");
    showStatus("Edit config.h");
  }
  lastAttemptMs = millis() - REFRESH_MS;  // Allow the first attempt now.
}

void loop() {
  if (!configured) { delay(50); return; }
  if (millis() - lastAttemptMs < REFRESH_MS) { delay(50); return; }
  // Advance before any request. Failures also wait 120 seconds before retrying.
  lastAttemptMs = millis();
  if (!connectWiFi()) {
    showStatus("Wi-Fi failed");
    Serial.println("Wi-Fi failed; next attempt in 120 seconds.");
    return;
  }
  if (!clockReady()) {
    showStatus("Clock not ready");
    Serial.println("NTP time unavailable; next attempt in 120 seconds.");
    return;
  }
  if (!refreshWeather()) {
    showStatus(haveWeather ? "Stale: API error" : "API/TODO pending");
    Serial.println("Weather unavailable; keeping any previous valid reading.");
    return;
  }
  if (ENABLE_NOTIFICATIONS) notifyFreshWeather();
}
