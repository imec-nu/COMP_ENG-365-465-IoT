#pragma once
// Provided microphone capture, VAD, USB transfer, and plot packet support.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <arduinoFFT.h>
#include <esp32-hal-psram.h>
#include <math.h>
#include <string.h>

#include "config.h"

// Functions completed in audio_features.ino.
void applyHannWindow(const int16_t *input);
void computeMagnitudeSpectrum();
float peakFrequencyHz(const float *spectrum);
void extractLogBandFeatures(float out[FEATURES_PER_FRAME]);

// -------------------------- Hardware/memory ---------------------------
I2SClass microphone;
static bool micRunning = false;
static bool receiverReady = false;
static int16_t currentFrame[FRAME_SAMPLES];
static int16_t preRoll[PRE_ROLL_FRAMES][FRAME_SAMPLES];
static uint8_t preWriteIndex = 0;
static uint8_t preFramesValid = 0;
static int16_t *clipSamples = nullptr; // 160 KB in PSRAM
static size_t clipCount = 0;
static uint32_t clipNumber = 0;

static float fftReal[FRAME_SAMPLES];
static float fftImag[FRAME_SAMPLES];
static float hann[FRAME_SAMPLES];
static float magnitude[FFT_BINS];
static float spectrumSum[SPECTRUM_BINS];
static float *featureMatrix = nullptr; // MAX_TEST_FRAMES x 8 floats in PSRAM
static uint16_t featureFrames = 0;
static ArduinoFFT<float> fft(fftReal, fftImag, FRAME_SAMPLES, (float)SAMPLE_RATE);

static float noiseRms = 0;
static float startThreshold = MIN_SPEECH_RMS;
static uint8_t activeRun = 0;
static uint8_t quietRun = 0;
static uint32_t lastStatusMs = 0;
enum VadState : uint8_t { WAITING_FOR_SPEECH, RECORDING_SPEECH };
static VadState vadState = WAITING_FOR_SPEECH;

// ----------------------------- DSP ------------------------------------
void prepareWindow() {
  // Compute the Hann weights once at startup.
  for (size_t n = 0; n < FRAME_SAMPLES; ++n) {
    hann[n] = 0.5f * (1.0f - cosf(2.0f * PI * (float)n /
                                   (float)(FRAME_SAMPLES - 1)));
  }
}

void startAnalysis() {
  featureFrames = 0;
  memset(spectrumSum, 0, sizeof(spectrumSum));
}

bool analyzeFrame(const int16_t *samples) {
  if (featureFrames >= MAX_FEATURE_FRAMES) return false;
  applyHannWindow(samples);
  computeMagnitudeSpectrum();
  float *features = featureMatrix + (size_t)featureFrames * FEATURES_PER_FRAME;
  extractLogBandFeatures(features);
  for (uint16_t k = 0; k < SPECTRUM_BINS; ++k) {
    spectrumSum[k] += magnitude[k];
  }
  ++featureFrames;
  return true;
}

float meanSpectrumPeakHz() {
  // A constant scale does not change the peak bin, so searching the sum
  // gives the same result as searching the mean spectrum.
  return peakFrequencyHz(spectrumSum);
}

// ---------------------------- USB transport ---------------------------
void putU32LE(uint8_t *dest, uint32_t v) {
  dest[0] = (uint8_t)v;
  dest[1] = (uint8_t)(v >> 8);
  dest[2] = (uint8_t)(v >> 16);
  dest[3] = (uint8_t)(v >> 24);
}

bool writeAll(const uint8_t *bytes, size_t length) {
  size_t offset = 0;
  uint32_t progressedAt = millis();
  while (offset < length) {
    size_t chunk = length - offset;
    if (chunk > 1024) chunk = 1024;
    size_t n = Serial.write(bytes + offset, chunk);
    if (n > 0) {
      offset += n;
      progressedAt = millis();
    } else {
      if (millis() - progressedAt > 5000) return false;
      delay(1);
    }
  }
  return true;
}

bool sendPcmPacket(uint32_t seq) {
  uint8_t header[16] = {'V', 'A', 'D', '1'};
  putU32LE(header + 4, SAMPLE_RATE);
  putU32LE(header + 8, (uint32_t)clipCount);
  putU32LE(header + 12, seq);
  return writeAll(header, sizeof(header)) &&
         writeAll((const uint8_t *)clipSamples, clipCount * sizeof(int16_t));
}

bool sendFeaturePacket(uint32_t seq) {
  if (featureFrames == 0) return false;
  // FEA2 + rate(u32) + seq(u32) + frames(u32) + bins(u32) + peak(f32)
  //     + 129 x f32 (mean magnitude spectrum)
  //     + frames x 8 x f32 (log band energies)
  uint8_t header[24] = {'F', 'E', 'A', '2'};
  putU32LE(header + 4, SAMPLE_RATE);
  putU32LE(header + 8, seq);
  putU32LE(header + 12, featureFrames);
  putU32LE(header + 16, SPECTRUM_BINS);
  const float peakHz = meanSpectrumPeakHz();
  static_assert(sizeof(float) == 4, "ESP32 IEEE-754 float32 expected");
  memcpy(header + 20, &peakHz, 4); // ESP32-S3 is little endian
  if (!writeAll(header, sizeof(header))) return false;

  float meanSpectrum[SPECTRUM_BINS];
  for (uint16_t k = 0; k < SPECTRUM_BINS; ++k) {
    meanSpectrum[k] = spectrumSum[k] / (float)featureFrames;
  }
  return writeAll((const uint8_t *)meanSpectrum, sizeof(meanSpectrum)) &&
         writeAll((const uint8_t *)featureMatrix,
                  (size_t)featureFrames * FEATURES_PER_FRAME * sizeof(float));
}

// ---------------------------- Microphone ------------------------------
bool beginMicrophone() {
  microphone.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  micRunning = microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  if (micRunning) microphone.setTimeout(250);
  return micRunning;
}

bool readAudioFrame(int16_t *dst) {
  uint8_t *bytes = (uint8_t *)dst;
  size_t received = 0;
  while (received < FRAME_BYTES) {
    size_t n = microphone.readBytes((char *)(bytes + received),
                                    FRAME_BYTES - received);
    if (n == 0) return false;
    received += n;
  }
  return true;
}

float frameRms(const int16_t *samples) {
  uint64_t acc = 0;
  for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
    int64_t s = samples[i];
    acc += (uint64_t)(s * s);
  }
  return sqrtf((float)((double)acc / (double)FRAME_SAMPLES));
}

void updateThreshold() {
  startThreshold = fmaxf(MIN_SPEECH_RMS, noiseRms * NOISE_MULTIPLIER);
}

void calibrateNoise() {
  Serial.println("# CALIBRATING: remain quiet for about 2 seconds");
  double sum = 0;
  uint32_t n = 0;
  while (n < NOISE_CALIBRATION_FRAMES) {
    if (!readAudioFrame(currentFrame)) continue;
    sum += frameRms(currentFrame);
    ++n;
  }
  noiseRms = (float)(sum / (double)n);
  updateThreshold();
  Serial.printf("# CALIBRATED noise=%.1f threshold=%.1f\n", noiseRms, startThreshold);
}

void resetVad() {
  preWriteIndex = 0;
  preFramesValid = 0;
  activeRun = 0;
  quietRun = 0;
  clipCount = 0;
  vadState = WAITING_FOR_SPEECH;
}

void pushPreRoll(const int16_t *samples) {
  memcpy(preRoll[preWriteIndex], samples, FRAME_BYTES);
  preWriteIndex = (preWriteIndex + 1) % PRE_ROLL_FRAMES;
  if (preFramesValid < PRE_ROLL_FRAMES) ++preFramesValid;
}

void appendClip(const int16_t *samples, size_t count) {
  size_t room = MAX_CLIP_SAMPLES - clipCount;
  if (count > room) count = room;
  if (count > 0) {
    memcpy(clipSamples + clipCount, samples, count * sizeof(int16_t));
    clipCount += count;
  }
}

void appendPreRoll() {
  uint8_t first = preFramesValid == PRE_ROLL_FRAMES ? preWriteIndex : 0;
  for (uint8_t i = 0; i < preFramesValid; ++i) {
    uint8_t index = (first + i) % PRE_ROLL_FRAMES;
    appendClip(preRoll[index], FRAME_SAMPLES);
  }
}

void beginSpeech() {
  clipCount = 0;
  quietRun = 0;
  appendPreRoll();  // includes the consecutive frames that confirmed speech
  vadState = RECORDING_SPEECH;
  Serial.printf("# VOICE_BEGIN threshold=%.1f preroll_ms=%u\n",
                startThreshold, (unsigned)(PRE_ROLL_FRAMES * 32));
}

void finishSpeech(const char *why) {
  ++clipNumber;
  Serial.printf("# VOICE_END id=%lu cause=%s samples=%lu\n",
                (unsigned long)clipNumber, why, (unsigned long)clipCount);
  if (receiverReady) {
    startAnalysis();
    const size_t completeFrames = clipCount / FRAME_SAMPLES;
    for (size_t i = 0; i < completeFrames; ++i) {
      if (!analyzeFrame(clipSamples + i * FRAME_SAMPLES)) break;
    }
    bool ok = sendPcmPacket(clipNumber) && sendFeaturePacket(clipNumber);
    if (!ok) {
      receiverReady = false;
      Serial.println("# ERROR: USB send failed; restart PC receiver");
    } else {
      Serial.printf("# SENT id=%lu feature_frames=%u\n",
                    (unsigned long)clipNumber, (unsigned)featureFrames);
    }
  } else {
    Serial.println("# SKIPPED: start Python --listen to save WAV and figures");
  }
  // Processing/transmission pauses microphone reading; clear old pre-roll.
  resetVad();
}

void handleMicFrame(const int16_t *frame) {
  const float rms = frameRms(frame);
  if (vadState == WAITING_FOR_SPEECH) {
    pushPreRoll(frame);
    if (rms >= startThreshold) {
      if (activeRun < START_ACTIVE_FRAMES) ++activeRun;
    } else {
      activeRun = 0;
    }
    if (activeRun >= START_ACTIVE_FRAMES) {
      beginSpeech();
    } else if (activeRun == 0 && rms < 0.70f * startThreshold) {
      noiseRms = (1.0f - NOISE_UPDATE_ALPHA) * noiseRms +
                 NOISE_UPDATE_ALPHA * rms;
      updateThreshold();
    }
  } else {
    appendClip(frame, FRAME_SAMPLES);
    if (rms < startThreshold * END_THRESHOLD_RATIO) {
      if (quietRun < END_QUIET_FRAMES) ++quietRun;
    } else {
      quietRun = 0;
    }
    if (quietRun >= END_QUIET_FRAMES) finishSpeech("silence");
    else if (clipCount >= MAX_CLIP_SAMPLES) finishSpeech("max_5_seconds");
  }
  uint32_t now = millis();
  if (now - lastStatusMs > 2000) {
    Serial.printf("# LEVEL rms=%.1f noise=%.1f threshold=%.1f state=%s\n",
                  rms, noiseRms, startThreshold,
                  vadState == WAITING_FOR_SPEECH ? "IDLE" : "RECORDING");
    lastStatusMs = now;
  }
}

// ---------------------- PC provided tone test -------------------------
// Request one 128-byte chunk at a time so a full frame fits through USB CDC
// without overrunning the receive buffer.
bool readExactSerial(uint8_t *dst, size_t n, size_t &got) {
  got = 0;
  uint32_t lastProgressMs = millis();
  while (got < n) {
    int one = Serial.read(); // -1 when no data, otherwise one byte
    if (one >= 0) {
      dst[got++] = (uint8_t)one;
      lastProgressMs = millis();
    } else {
      if (millis() - lastProgressMs > 5000) return false;
      delay(1);
    }
  }
  return true;
}

void processTest(uint16_t frameCount) {
  Serial.printf("# TEST_BEGIN frames=%u protocol=chunk128\n", (unsigned)frameCount);
  Serial.flush();
  if (micRunning) {
    microphone.end(); // prevents old I2S samples accumulating during test
    micRunning = false;
  }
  resetVad();
  startAnalysis();
  bool okay = true;
  for (uint16_t i = 0; i < frameCount && okay; ++i) {
    // A full 512-sample frame is assembled from eight 128-byte transfers.
    for (size_t offset = 0; offset < FRAME_BYTES; offset += TEST_SERIAL_CHUNK) {
      size_t requested = FRAME_BYTES - offset;
      if (requested > TEST_SERIAL_CHUNK) requested = TEST_SERIAL_CHUNK;
      Serial.printf("# CHUNK %u %u %u\n", (unsigned)i, (unsigned)offset,
                    (unsigned)requested);
      Serial.flush(); // Ensure PC sees the request before we await its reply.
      size_t received = 0;
      if (!readExactSerial(((uint8_t *)currentFrame) + offset, requested, received)) {
        Serial.printf("# ERROR: missing test frame %u offset=%u received=%u/%u\n",
                      (unsigned)i, (unsigned)offset, (unsigned)received,
                      (unsigned)requested);
        okay = false;
        break;
      }
    }
    if (okay && !analyzeFrame(currentFrame)) {
      Serial.printf("# ERROR: FFT analysis failed on frame %u\n", (unsigned)i);
      okay = false;
    }
  }
  if (okay) {
    float peak = meanSpectrumPeakHz();
    Serial.printf("# TEST_RESULT frames=%u peak_hz=%.2f\n",
                  (unsigned)featureFrames, peak);
    if (!sendFeaturePacket(0)) {
      Serial.println("# ERROR: test feature transfer stalled");
    }
  }
  Serial.println("# TEST_DONE: restarting microphone");
  if (!beginMicrophone()) {
    Serial.println("# FATAL: could not restart PDM microphone");
    while (true) delay(1000);
  }
  calibrateNoise();
  resetVad();
  Serial.println("# LISTENING: microphone ready");
}

void pollCommands() {
  static char line[40] = {0};
  static uint8_t pos = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (pos < sizeof(line) - 1) line[pos++] = c;
      else pos = 0;
      continue;
    }
    line[pos] = '\0';
    if (strcmp(line, "READY") == 0) {
      receiverReady = true;
      Serial.println("# RECEIVER_READY");
    } else if (strcmp(line, "STOP") == 0) {
      receiverReady = false;
      Serial.println("# RECEIVER_STOPPED");
    } else if (strncmp(line, "TEST ", 5) == 0) {
      long frames = strtol(line + 5, nullptr, 10);
      if (vadState != WAITING_FOR_SPEECH) {
        Serial.println("# ERROR: cannot run TEST while recording");
      } else if (frames < 1 || frames > MAX_TEST_FRAMES) {
        Serial.println("# ERROR: TEST frame count must be 1..400");
      } else {
        pos = 0;
        processTest((uint16_t)frames);
        return;
      }
    } else if (pos) {
      Serial.println("# ERROR: unknown command");
    }
    pos = 0;
  }
}

// ----------------------------- Arduino --------------------------------
void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  Serial.println("# LAB6_TASK2 starting");
  if (!psramFound()) {
    Serial.println("# FATAL: enable Tools > PSRAM > OPI PSRAM");
    while (true) delay(1000);
  }
  clipSamples = (int16_t *)ps_malloc(MAX_CLIP_SAMPLES * sizeof(int16_t));
  featureMatrix = (float *)ps_malloc((size_t)MAX_FEATURE_FRAMES *
                                    FEATURES_PER_FRAME * sizeof(float));
  if (!clipSamples || !featureMatrix) {
    Serial.println("# FATAL: PSRAM allocation failed");
    while (true) delay(1000);
  }
  prepareWindow();
  if (!beginMicrophone()) {
    Serial.println("# FATAL: PDM microphone initialization failed");
    while (true) delay(1000);
  }
  calibrateNoise();
  Serial.println("# LISTENING: microphone ready");
  Serial.println("# Launch tools/feature_viewer.py --listen or --tone ...");
}

void loop() {
  pollCommands();
  if (micRunning && readAudioFrame(currentFrame)) handleMicFrame(currentFrame);
}
