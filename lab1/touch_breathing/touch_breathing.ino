// Checkpoint 4: one breathing cycle per single-touch trigger.
// Board: XIAO ESP32S3. Serial Monitor: 115200 baud.
// Multiple touches stop the cycle; releasing the wire lets it finish.
#include <Arduino.h>

const int LED_PIN = LED_BUILTIN;
const int T1_PIN = D0;  // GPIO1
const int T2_PIN = D3;  // GPIO4
const int T3_PIN = D8;  // GPIO7
const uint32_t MAX_DUTY = 1023;
const uint32_t POLL_MS = 20;
const uint32_t TRIGGER_STABLE_MS = 100;

// TODO 4.1: Set each threshold from the released and touched readings.
const uint32_t THRESHOLD_1 = 200000;
const uint32_t THRESHOLD_2 = 200000;
const uint32_t THRESHOLD_3 = 200000;

// After PWM inversion: 0 = off, MAX_DUTY = full brightness.
void fadeTo(uint32_t targetDuty, uint32_t durationMs);

uint8_t selectMode(bool t1, bool t2, bool t3) {
  // TODO 4.2: Return 1, 2, or 3 if exactly one input is touched.
  // Return 0 otherwise. Adding the booleans gives the number of touches.
  // YOUR CODE HERE
  return 0;
}

uint32_t periodForMode(uint8_t mode) {
  // TODO 4.3: Return the total cycle time in ms:
  // mode 1: 1000, mode 2: 2500, mode 3: 5000; otherwise 0.
  // Use if/else or switch.
  // YOUR CODE HERE
  return 0;
}

void runBreathingCycle(uint32_t periodMs) {
  // TODO 4.4: Call fadeTo() twice, first to MAX_DUTY and then to 0.
  // Each call takes half of periodMs. No extra delay is needed.
  // fadeTo() handles cancellation if multiple inputs are touched.
  // YOUR CODE HERE
}

bool anyTouch(bool t1, bool t2, bool t3) {
  // TODO 4.5: Return true if any input is touched.
  // Return false only when all are released. Use logical OR (||).
  // YOUR CODE HERE
  return false;
}

// Support code.

uint32_t currentDuty = 0;
bool cycleCancelled = false;

void readTouches(bool &t1, bool &t2, bool &t3) {
  const uint32_t raw1 = touchRead(T1_PIN);
  const uint32_t raw2 = touchRead(T2_PIN);
  const uint32_t raw3 = touchRead(T3_PIN);
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

// Releasing a wire allows the fade to continue. Multiple touches cancel it.
void fadeTo(uint32_t targetDuty, uint32_t durationMs) {
  if (cycleCancelled) return;

  const int32_t startDuty = (int32_t)currentDuty;
  const int32_t difference = (int32_t)targetDuty - startDuty;
  const uint32_t startMs = millis();

  while (true) {
    bool t1, t2, t3;
    readTouches(t1, t2, t3);
    if (selectMode(t1, t2, t3) == 0 && anyTouch(t1, t2, t3)) {
      cycleCancelled = true;
      ledcWrite(LED_PIN, 0);
      currentDuty = 0;
      Serial.println("Cycle stopped: multiple touches.");
      return;
    }

    const uint32_t elapsedMs = millis() - startMs;
    if (durationMs == 0 || elapsedMs >= durationMs) {
      ledcWrite(LED_PIN, targetDuty);
      currentDuty = targetDuty;
      return;
    }

    const int32_t duty = startDuty +
        (int32_t)((int64_t)difference * elapsedMs / durationMs);
    ledcWrite(LED_PIN, (uint32_t)duty);
    currentDuty = (uint32_t)duty;
    delay(5);
  }
}

void waitForRelease() {
  // Require three consecutive all-released readings to filter noise.
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
  digitalWrite(LED_PIN, HIGH);
  const bool attached = ledcAttach(LED_PIN, 5000, 10);
  const bool inverted = attached && ledcOutputInvert(LED_PIN, true);
  if (!attached || !inverted) {
    while (true) {
      Serial.println("PWM setup failed. Check the board and ESP32 core version.");
      delay(1000);
    }
  }
  ledcWrite(LED_PIN, 0);
}

void loop() {
  static uint8_t pendingMode = 0;
  static uint32_t pendingSinceMs = 0;

  bool t1, t2, t3;
  readTouches(t1, t2, t3);
  const uint8_t mode = selectMode(t1, t2, t3);
  if (mode == 0) {
    pendingMode = 0;
    ledcWrite(LED_PIN, 0);
    currentDuty = 0;
    delay(POLL_MS);
    return;
  }

  // Allow nearly simultaneous contacts to settle before starting.
  if (mode != pendingMode) {
    pendingMode = mode;
    pendingSinceMs = millis();
  }
  if (millis() - pendingSinceMs < TRIGGER_STABLE_MS) {
    delay(POLL_MS);
    return;
  }

  const uint32_t periodMs = periodForMode(mode);
  if (periodMs == 0) {
    delay(POLL_MS);
    return;
  }

  pendingMode = 0;
  cycleCancelled = false;
  runBreathingCycle(periodMs);
  ledcWrite(LED_PIN, 0);
  currentDuty = 0;
  // Wait after both a completed cycle and a cancelled cycle.
  waitForRelease();
}

