#pragma once
#include "config.h"
#include <ESP_I2S.h>
#include <esp32-hal-psram.h>
#include <math.h>
#include <string.h>

// Provided microphone, buffers, and USB transport. Complete the .ino functions.
I2SClass microphone;

static int16_t currentFrame[FRAME_SAMPLES];
static int16_t preRoll[PRE_ROLL_FRAMES][FRAME_SAMPLES];
static uint8_t preWriteIndex = 0;
static uint8_t preFramesValid = 0;

// Allocated in PSRAM in setup() (~160 KB).
static int16_t *clipSamples = nullptr;
static size_t clipCount = 0;
static uint32_t clipNumber = 0;
static bool receiverReady = false;

static float noiseRms = 0.0f;
static float startThreshold = MIN_SPEECH_RMS;
static uint8_t activeRun = 0;
static uint8_t quietRun = 0;
static uint32_t lastStatusMs = 0;

enum VadState : uint8_t { WAITING_FOR_SPEECH, RECORDING_SPEECH };
static VadState vadState = WAITING_FOR_SPEECH;


void updateSpeechThreshold();
void adaptBackgroundNoise(float rms);
void appendPreRollInOrder();
void processWaitingFrame(const int16_t *frame, float rms);
void processRecordingFrame(const int16_t *frame, float rms);

bool readAudioFrame(int16_t *destination) {
  // readBytes() can return fewer bytes than requested. Complete the frame
  // before using it, so every frame is exactly 32 ms of audio.
  uint8_t *destBytes = reinterpret_cast<uint8_t *>(destination);
  size_t received = 0;
  while (received < FRAME_BYTES) {
    size_t count = microphone.readBytes(
        reinterpret_cast<char *>(destBytes + received),
        FRAME_BYTES - received);
    if (count == 0) {
      Serial.println("# ERROR: microphone read timed out");
      return false;
    }
    received += count;
  }
  return true;
}

float frameRms(const int16_t *samples, size_t count) {
  if (count == 0) return 0.0f;
  uint64_t squaredSum = 0;
  for (size_t i = 0; i < count; ++i) {
    // Cast before multiplication; 512 * 32768^2 exceeds 32-bit range.
    int64_t sample = static_cast<int64_t>(samples[i]);
    squaredSum += static_cast<uint64_t>(sample * sample);
  }
  return sqrtf(static_cast<float>(
      static_cast<double>(squaredSum) / static_cast<double>(count)));
}

void calibrateBackgroundNoise() {
  Serial.println("# CALIBRATION: stay quiet for approximately 2 seconds");
  double rmsSum = 0.0;
  uint32_t frames = 0;
  while (frames < NOISE_CALIBRATION_FRAMES) {
    if (!readAudioFrame(currentFrame)) continue;
    rmsSum += frameRms(currentFrame, FRAME_SAMPLES);
    ++frames;
  }
  noiseRms = static_cast<float>(rmsSum / frames);
  updateSpeechThreshold();
  Serial.printf("# CALIBRATED noise_rms=%.1f start_threshold=%.1f end_threshold=%.1f\n",
                noiseRms, startThreshold, startThreshold * END_THRESHOLD_RATIO);
}

void pushPreRoll(const int16_t *frame) {
  memcpy(preRoll[preWriteIndex], frame, FRAME_BYTES);
  preWriteIndex = (preWriteIndex + 1) % PRE_ROLL_FRAMES;
  if (preFramesValid < PRE_ROLL_FRAMES) ++preFramesValid;
}

void appendClip(const int16_t *samples, size_t count) {
  const size_t space = MAX_SAMPLES - clipCount;
  const size_t take = count < space ? count : space;
  if (take > 0) {
    memcpy(clipSamples + clipCount, samples, take * sizeof(int16_t));
    clipCount += take;
  }
}

void writeUInt32LE(uint8_t *dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
  dst[2] = static_cast<uint8_t>(value >> 16);
  dst[3] = static_cast<uint8_t>(value >> 24);
}

bool writeAll(const uint8_t *data, size_t length) {
  size_t offset = 0;
  uint32_t lastProgressMs = millis();
  while (offset < length) {
    // Avoid trying to queue an entire five-second recording at once.
    size_t request = length - offset;
    if (request > 1024) request = 1024;
    size_t written = Serial.write(data + offset, request);
    if (written > 0) {
      offset += written;
      lastProgressMs = millis();
    } else {
      if (millis() - lastProgressMs > 5000) return false;
      delay(1);
    }
  }
  return true;
}

void sendClip(uint32_t sequenceNumber) {
  if (!receiverReady) {
    Serial.println("# NOT_SENT: run tools/receive_audio.py to enable WAV export");
    return;
  }

  uint8_t header[16] = {'V', 'A', 'D', '1'};
  writeUInt32LE(header + 4, SAMPLE_RATE);
  writeUInt32LE(header + 8, static_cast<uint32_t>(clipCount));
  writeUInt32LE(header + 12, sequenceNumber);

  // No textual logging between the header and the PCM data.
  bool ok = writeAll(header, sizeof(header)) &&
            writeAll(reinterpret_cast<const uint8_t *>(clipSamples),
                     clipCount * sizeof(int16_t));
  if (ok) {
    Serial.printf("# SENT clip=%lu samples=%lu\n",
                  static_cast<unsigned long>(sequenceNumber),
                  static_cast<unsigned long>(clipCount));
  } else {
    receiverReady = false;
    Serial.println("# ERROR: USB write stalled; reconnect receiver");
  }
}

void pollReceiverCommand() {
  static char command[16];
  static uint8_t index = 0;
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      command[index] = '\0';
      if (strcmp(command, "READY") == 0) {
        receiverReady = true;
        Serial.println("# RECEIVER_READY");
      } else if (strcmp(command, "STOP") == 0) {
        receiverReady = false;
        Serial.println("# RECEIVER_STOPPED");
      }
      index = 0;
    } else if (index < sizeof(command) - 1) {
      command[index++] = ch;
    } else {
      index = 0;
    }
  }
}

void beginSpeechClip() {
  clipCount = 0;
  quietRun = 0;
  appendPreRollInOrder();
  vadState = RECORDING_SPEECH;
  Serial.printf("# START threshold=%.1f preroll_ms=%lu\n", startThreshold,
                static_cast<unsigned long>(PRE_ROLL_FRAMES * FRAME_SAMPLES * 1000UL / SAMPLE_RATE));
}

void finishSpeechClip(const char *reason) {
  ++clipNumber;
  const uint32_t durationMs =
      static_cast<uint32_t>(clipCount * 1000UL / SAMPLE_RATE);
  Serial.printf("# END clip=%lu reason=%s duration_ms=%lu samples=%lu\n",
                static_cast<unsigned long>(clipNumber), reason,
                static_cast<unsigned long>(durationMs),
                static_cast<unsigned long>(clipCount));
  sendClip(clipNumber);

  // USB transmission pauses microphone reads; don't join stale pre-roll
  // from the previous segment to the next new utterance.
  preFramesValid = 0;
  preWriteIndex = 0;
  activeRun = 0;
  quietRun = 0;
  clipCount = 0;
  vadState = WAITING_FOR_SPEECH;
}

void processFrame(const int16_t *frame) {
  const float rms = frameRms(frame, FRAME_SAMPLES);
  if (vadState == WAITING_FOR_SPEECH) {
    processWaitingFrame(frame, rms);
  } else {
    processRecordingFrame(frame, rms);
  }
  const uint32_t now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    Serial.printf("# RMS=%.1f noise=%.1f threshold=%.1f state=%s\n",
                  rms, noiseRms, startThreshold,
                  vadState == WAITING_FOR_SPEECH ? "IDLE" : "RECORDING");
    lastStatusMs = now;
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 1500) delay(10);
  Serial.println("# Lab 6 Task 1: VAD starting");

  if (!psramFound()) {
    Serial.println("# FATAL: PSRAM unavailable. Set Tools > PSRAM > OPI PSRAM.");
    while (true) delay(1000);
  }
  clipSamples = static_cast<int16_t *>(ps_malloc(MAX_SAMPLES * sizeof(int16_t)));
  if (!clipSamples) {
    Serial.println("# FATAL: 160 KB PSRAM audio buffer allocation failed");
    while (true) delay(1000);
  }

  microphone.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if (!microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("# FATAL: PDM microphone initialization failed");
    while (true) delay(1000);
  }
  microphone.setTimeout(250);
  calibrateBackgroundNoise();
  Serial.println("# Listening. Start tools/receive_audio.py to save WAV clips.");
  Serial.println("# Voice should exceed the printed threshold for 3 frames.");
}

void loop() {
  pollReceiverCommand();
  if (readAudioFrame(currentFrame)) processFrame(currentFrame);
}
