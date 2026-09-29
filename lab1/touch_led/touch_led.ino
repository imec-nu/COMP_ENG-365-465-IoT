// Checkpoint 2: turn the LED on while the wire is touched.
// Board: XIAO ESP32S3. Serial Monitor: 115200 baud.
// D0 = GPIO1. Touch readings normally increase on the ESP32-S3.
#include <Arduino.h>

const int TOUCH_PIN = D0;
const int LED_PIN = LED_BUILTIN;

// TODO 2.1: Measure the released and touched readings. Set the threshold
// between the two ranges; 200000 is a starting value.
const uint32_t TOUCH_THRESHOLD = 200000;

unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Active-low LED: HIGH = off.
}

void loop() {
  // TODO 2.2: Read TOUCH_PIN with touchRead(). Replace 0 below.
  uint32_t raw = 0;  // YOUR CODE HERE

  // TODO 2.3: Compare raw with TOUCH_THRESHOLD. Replace false below.
  bool touched = false;  // YOUR CODE HERE

  // TODO 2.4: Use if/else and digitalWrite() to set LED_PIN.
  // LOW = on when touched; HIGH = off when released.
  // YOUR CODE HERE

  // Print readings every 100 ms for calibration.
  if (millis() - lastPrintTime >= 100) {
    lastPrintTime = millis();
    Serial.print("raw=");
    Serial.print(raw);
    Serial.print(" threshold=");
    Serial.print(TOUCH_THRESHOLD);
    Serial.print(" touched=");
    Serial.println(touched);
  }

  delay(20);
}
