// Lab 3, Task 2: a hardware touch interrupt requests a microphone stop.
// Board: XIAO_ESP32S3; ESP32 package 3.3.12. Serial Monitor: 115200 baud.
// Use the Sense expansion-board mic and a free-ended touch wire on D0/GPIO1.
#include <Arduino.h>
#include <ESP_I2S.h>

const int LED_PIN = LED_BUILTIN;
const int TOUCH_PIN = D0;  // D0 is GPIO1; D1 is a different pin.
const uint32_t SAMPLE_RATE = 16000;
const size_t READ_BLOCK_SAMPLES = 64;
const uint32_t READ_TIMEOUT_MS = 20;

// First upload with calibration mode true and observe released/touched values.
// Then set the delta below, change this back to false, and upload with the
// contact released. Calibration mode does not start the microphone or IRQ.
const bool TOUCH_CALIBRATION_MODE = false;
// ESP32-S3 interrupt threshold: INCREASE above the hardware baseline.
// Start near half of (touched raw reading - released raw reading).
// This is not the absolute touchRead threshold used in Lab 1.
const uint32_t TOUCH_DELTA_THRESHOLD = 100000;

volatile bool stopRequested = false;  // ISR writes true; foreground reads it.
enum RecorderState { RUNNING, STOPPED };
RecorderState recorderState = RUNNING;
int16_t audioBlock[READ_BLOCK_SAMPLES];

#include "touch_stop_support.h"

void ARDUINO_ISR_ATTR onTouch() {
  // TODO 2.1 (callback): This callback runs for touch AND release on ESP32-S3.
  // Check touchInterruptGetLastStatus(TOUCH_PIN). Only an active touch should
  // latch stopRequested=true. Release must never clear a pending request.
  // Do not read audio, stop I2S, print, or delay inside this ISR.
  // YOUR CODE HERE
}

void attachTouchStop() {
  // TODO 2.1 (registration): Connect TOUCH_PIN to onTouch with
  // touchAttachInterrupt(pin, callback, threshold). Use the calibrated
  // TOUCH_DELTA_THRESHOLD. The main loop must not poll touchRead to stop.
  // YOUR CODE HERE
}

bool stopIfRequested() {
  // TODO 2.2: Implement the one-time RUNNING -> STOPPED transition.
  // If already STOPPED, return true without repeating cleanup.
  // If no request is pending, return false.
  // Otherwise detach the touch callback with touchDetachInterrupt(TOUCH_PIN),
  // store I2S.end() in microphoneEnded, turn on the LED with setLed(true),
  // change recorderState to STOPPED, call queueStopMessage(), and return true.
  // Leave the request latched; only reset/restart should resume acquisition.
  // These operations belong in foreground code, not the ISR.
  // YOUR CODE HERE
  return false;
}

void acquireAndStream() {
  // TODO 2.3: Read and stream only while running; make stop requests responsive.
  // Call stopIfRequested() before starting a read, returning if it says stop.
  // Read one block using readAudioBlock(audioBlock, READ_BLOCK_SAMPLES), and
  // add its returned sample count to samplesRead. A zero count is valid.
  // Check stopIfRequested() again after the read and before each sample output.
  // Send each valid sample through printRawSample(audioBlock[i]).
  // The supplied output helper skips/counts a sample if Serial cannot accept
  // it immediately; it never waits for an entire audio stream to print.
  // YOUR CODE HERE
}

void setup() {
  beginHardware();
  if (!TOUCH_CALIBRATION_MODE) attachTouchStop();
}

void loop() {
  if (TOUCH_CALIBRATION_MODE) {
    showTouchCalibration();
    return;
  }
  acquireAndStream();
  sendStopMessageIfReady();
  delay(1);
}
