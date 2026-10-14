// Lab 3, Task 1: loudness detection with the Sense expansion-board microphone.
// Board: XIAO_ESP32S3; ESP32 package 3.3.12. Serial Monitor: 115200 baud.
// Microphone wiring is on the expansion board: PDM clock GPIO42, data GPIO41.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

const int LED_PIN = LED_BUILTIN;  // GPIO21, active low.
const uint32_t SAMPLE_RATE = 16000;
const size_t FRAME_SAMPLES = 512;  // 32 ms of audio at 16 kHz.
const float RMS_THRESHOLD = 2000.0f;  // Calibrate using quiet and snap RMS values.
const uint32_t LED_HOLD_MS = 800;
const uint32_t COOLDOWN_MS = 350;

bool ledIsOn = false;
bool hasTriggered = false;
uint32_t lastTriggerMs = 0;
int16_t audioFrame[FRAME_SAMPLES];

#include "snap_light_support.h"

float calculateRms(const int16_t *samples, size_t count) {
  // TODO 1.1: Implement RMS for all count samples.
  // Return zero for an empty frame. Use a uint64_t sum of squares;
  // promote each signed sample to int32_t before multiplying it by itself.
  // Divide by count using floating-point arithmetic, then apply sqrtf().
  // RMS is in raw PCM counts, not decibels. Do not average signed samples.
  // YOUR CODE HERE
  return 0.0f;
}

void handleSoundLevel(float rms, uint32_t nowMs) {
  // TODO 1.2: Accept a loud frame only when rms > RMS_THRESHOLD and either
  // no frame has triggered yet or COOLDOWN_MS has elapsed since lastTriggerMs.
  // On acceptance: call setLed(true), update ledIsOn and hasTriggered,
  // and save nowMs in lastTriggerMs. Otherwise leave the state unchanged.
  // Compare elapsed time using uint32_t subtraction, not a future deadline.
  // An accepted trigger while the LED is on starts its hold time again.
  // YOUR CODE HERE
}

void updateLedTimeout(uint32_t nowMs) {
  // TODO 1.3: If the LED is on and LED_HOLD_MS has elapsed since the latest
  // accepted trigger, call setLed(false) and update ledIsOn.
  // Use elapsed-time subtraction. Do not use delay() to hold the LED on:
  // the microphone must keep being read during the hold and cooldown.
  // YOUR CODE HERE
}

void setup() {
  beginHardware();
}

void loop() {
  updateLedTimeout(millis());
  const size_t count = readAudioFrame(audioFrame, FRAME_SAMPLES);
  if (count != 0) {
    const float rms = calculateRms(audioFrame, count);
    handleSoundLevel(rms, millis());
    reportRms(rms);
  }
  updateLedTimeout(millis());
  delay(1);
}
