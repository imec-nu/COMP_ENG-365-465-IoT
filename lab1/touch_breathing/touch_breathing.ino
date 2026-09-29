// Checkpoint 4: one breathing cycle per touch.
// Board: XIAO_ESP32S3, Arduino-ESP32 3.x.
// Complete TODO 4.1 through 4.5.

#include <Arduino.h>

const int LED_PIN = LED_BUILTIN;  // GPIO21, active low
const int T1_PIN = D0;            // GPIO1
const int T2_PIN = D3;            // GPIO4
const int T3_PIN = D8;            // GPIO7
const uint32_t MAX_DUTY = 1023;   // 10-bit PWM
const uint32_t POLL_MS = 20;

// TODO 4.1: Set each threshold from the released and touched readings
// in Serial Monitor. The values below are starting values.
const uint32_t THRESHOLD_1 = 200000;
const uint32_t THRESHOLD_2 = 200000;
const uint32_t THRESHOLD_3 = 200000;

// PWM output is inverted in setup(): 0 = off, MAX_DUTY = full brightness.
void fadeTo(uint32_t targetDuty, uint32_t durationMs);

uint8_t selectMode(bool t1, bool t2, bool t3) {
  // TODO 4.2: Return the input number (1, 2, or 3) if exactly one is touched.
  // Return 0 otherwise. Adding the booleans gives the number of touches.
  // YOUR CODE HERE
  return 0;
}

uint32_t periodForMode(uint8_t mode) {
  // TODO 4.3: Return the total cycle time in ms:
  // mode 1: 1000, mode 2: 2500, mode 3: 5000; otherwise 0.
  // Use if/else or switch. Replace the return statement below.
  // YOUR CODE HERE
  return 0;
}

void runBreathingCycle(uint32_t periodMs) {
  // TODO 4.4: Call fadeTo(targetDuty, durationMs) twice: first to MAX_DUTY,
  // then to 0. Each call takes half of periodMs and waits for the fade
  // to finish, so no extra delay is needed.
  // YOUR CODE HERE
}

bool anyTouch(bool t1, bool t2, bool t3) {
  // TODO 4.5: Return true while any input is touched, false when all are
  // released. Use logical OR (||). Replace the return statement below.
  // YOUR CODE HERE
  return false;
}

// Support functions and main loop. Read through these; no edits are needed.

void readTouches(bool &t1, bool &t2, bool &t3) {
  const uint32_t raw1 = touchRead(T1_PIN);
  const uint32_t raw2 = touchRead(T2_PIN);
  const uint32_t raw3 = touchRead(T3_PIN);
  // Touch readings normally increase on the ESP32-S3.
  t1 = raw1 > THRESHOLD_1;
  t2 = raw2 > THRESHOLD_2;
  t3 = raw3 > THRESHOLD_3;

  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 100) {
    lastPrintMs = millis();
    Serial.printf("raw: %lu %lu %lu | touched: %d %d %d\n",
                  (unsigned long)raw1, (unsigned long)raw2,
                  (unsigned long)raw3, t1, t2, t3);
  }
}

// Fade from the current duty to targetDuty over about durationMs milliseconds.
// This function returns when the fade is complete.
void fadeTo(uint32_t targetDuty, uint32_t durationMs) {
  static uint32_t currentDuty = 0;
  const int32_t startDuty = (int32_t)currentDuty;
  const int32_t difference = (int32_t)targetDuty - startDuty;
  const uint32_t startMs = millis();

  while (durationMs > 0) {
    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs >= durationMs) break;
    const int32_t duty = startDuty +
        (int32_t)((int64_t)difference * elapsedMs / durationMs);
    ledcWrite(LED_PIN, (uint32_t)duty);
    delay(5);
  }
  ledcWrite(LED_PIN, targetDuty);
  currentDuty = targetDuty;
}

void waitForRelease() {
  // Wait for three consecutive released samples (about 60 ms) to filter noise.
  uint8_t releasedSamples = 0;
  while (releasedSamples < 3) {
    bool t1, t2, t3;
    readTouches(t1, t2, t3);
    if (anyTouch(t1, t2, t3)) {
      releasedSamples = 0;
    } else {
      ++releasedSamples;
    }
    delay(POLL_MS);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off
  const bool attached = ledcAttach(LED_PIN, 5000, 10);
  const bool inverted = attached && ledcOutputInvert(LED_PIN, true);
  if (!attached || !inverted) {
    // Repeat the message in case Serial Monitor is opened later.
    while (true) {
      Serial.println("PWM setup failed. Check the board and ESP32 core version.");
      delay(1000);
    }
  }
  ledcWrite(LED_PIN, 0);  // Zero duty means off after output inversion.
}

void loop() {
  bool t1, t2, t3;
  readTouches(t1, t2, t3);
  const uint8_t mode = selectMode(t1, t2, t3);
  if (mode == 0) {
    ledcWrite(LED_PIN, 0);
    delay(POLL_MS);
    return;
  }

  const uint32_t periodMs = periodForMode(mode);
  if (periodMs == 0) {  // Skip unset or invalid periods.
    delay(POLL_MS);
    return;
  }

  // Finish the cycle before checking for release. Touches during the cycle
  // are ignored and do not trigger another cycle.
  runBreathingCycle(periodMs);
  ledcWrite(LED_PIN, 0);
  // All three inputs must be released before the next trigger.
  waitForRelease();
}
