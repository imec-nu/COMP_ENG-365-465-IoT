// Checkpoint 3: SOS (... --- ...).
// Board: XIAO ESP32S3. The built-in LED is active low.

#include <Arduino.h>

const int LED_PIN = LED_BUILTIN;
const unsigned long DOT_MS = 200;
const unsigned long DASH_MS = 400;
const unsigned long SYMBOL_GAP_MS = DOT_MS;
const unsigned long LETTER_GAP_MS = 3 * DOT_MS;
const unsigned long MESSAGE_GAP_MS = 7 * DOT_MS;

// One pulse lasting durationMs. The caller adds the gap after the pulse.
void blinkPulse(unsigned long durationMs) {
  digitalWrite(LED_PIN, LOW);
  delay(durationMs);
  digitalWrite(LED_PIN, HIGH);
}

void sendS() {
  // TODO 3.1: Send three dots using blinkPulse(DOT_MS).
  // Use delay(SYMBOL_GAP_MS) between dots. Leave the final gap to loop().
  // YOUR CODE HERE
}

void sendO() {
  // TODO 3.2: Send three dashes using blinkPulse(DASH_MS).
  // Use delay(SYMBOL_GAP_MS) between dashes. Leave the final gap to loop().
  // YOUR CODE HERE
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off
}

void loop() {
  // TODO 3.3: Send S, O, S using the functions above.
  // Use delay() with LETTER_GAP_MS between letters and MESSAGE_GAP_MS
  // after the final S. These are full gaps; do not add a symbol gap too.
  // YOUR CODE HERE
}
