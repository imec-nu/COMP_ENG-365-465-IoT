// Blink example for Checkpoint 2. Board: XIAO ESP32S3.
// The built-in LED is active low: LOW = on, HIGH = off.
#include <Arduino.h>

const int LED_PIN = LED_BUILTIN;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off
}

void loop() {
  digitalWrite(LED_PIN, LOW);   // LED on
  delay(500);
  digitalWrite(LED_PIN, HIGH);  // LED off
  delay(500);
}
