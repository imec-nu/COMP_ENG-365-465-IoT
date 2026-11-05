#pragma once

// Audio capture, feature extraction, DTW, and the serial console are provided.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <arduinoFFT.h>
#include <esp32-hal-psram.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "voice_command_types.h"

#include "config.h"

bool storeTemplate(uint8_t index, const float *features, uint16_t frames);
Prediction matchCommand(const float *features, uint16_t frames);
const char *executeCommand(Command command);

// ---------------------------- Buffers ---------------------------------
I2SClass microphone;
static int16_t audioFrame[FRAME_SAMPLES];
static int16_t preRoll[PRE_ROLL_FRAMES][FRAME_SAMPLES];
static uint8_t preWriteIndex = 0, preFramesValid = 0;
static int16_t *clipSamples = nullptr;           // 160 KB PSRAM
static size_t clipCount = 0;
static size_t speechStartSample = 0;
static uint32_t clipNumber = 0;

static float fftReal[FRAME_SAMPLES];
static float fftImag[FRAME_SAMPLES];
static float hann[FRAME_SAMPLES];
static float magnitudes[FFT_BINS];
static float *queryFeatures = nullptr;  // MAX_VOICE_FRAMES x 8, PSRAM
static uint16_t queryFrames = 0;
static ArduinoFFT<float> fft(fftReal, fftImag, FRAME_SAMPLES, (float)SAMPLE_RATE);

struct TemplateEntry {
  float *features;                      // MAX_VOICE_FRAMES x 8, PSRAM
  uint16_t frames;
  bool valid;
};
static TemplateEntry templates[TEMPLATE_COUNT];
static float acceptThreshold[CLASS_COUNT] = {0, 0};
static uint8_t enrolled = 0;
static bool ledOn = false;
static bool everAccepted = false;
static uint32_t lastAcceptedMs = 0;

static float noiseRms = 0.0f;
static float startThreshold = MIN_SPEECH_RMS;
static uint8_t activeRun = 0, quietRun = 0;
static uint32_t lastStatusMs = 0;

enum VadState : uint8_t { WAITING_FOR_SPEECH, RECORDING_SPEECH };
static VadState vadState = WAITING_FOR_SPEECH;
enum OperatingMode : uint8_t { MODE_IDLE, MODE_ENROLL, MODE_RECOGNIZE };
static OperatingMode mode = MODE_IDLE;

const char *className(uint8_t label) { return label == 0 ? "WAKE" : "SLEEP"; }
uint8_t templateClass(uint8_t index) { return index / TEMPLATES_PER_CLASS; }

// Feature extraction from Task 2
void prepareWindow() {
  for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
    hann[i] = 0.5f * (1.0f - cosf(2.0f * PI * (float)i /
                                (float)(FRAME_SAMPLES - 1)));
  }
}

void applyHannWindow(const int16_t *input) {
  float avg = 0.0f;
  for (size_t i = 0; i < FRAME_SAMPLES; ++i) avg += input[i];
  avg /= (float)FRAME_SAMPLES;
  for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
    fftReal[i] = ((float)input[i] - avg) * hann[i];
    fftImag[i] = 0.0f;
  }
}

void computeSpectrum() {
  fft.compute(FFTDirection::Forward);
  for (uint16_t k = 0; k < FFT_BINS; ++k) {
    const float re = fftReal[k], im = fftImag[k];
    magnitudes[k] = sqrtf(re * re + im * im);
  }
}

void extractLogBandFeatures(float output[FEATURES]) {
  const float binHz = (float)SAMPLE_RATE / (float)FRAME_SAMPLES;
  for (uint8_t b = 0; b < FEATURES; ++b) {
    float power = 0.0f;
    for (uint16_t k = 0; k < SPECTRUM_BINS; ++k) {
      float hz = k * binHz;
      bool inBand = hz >= BAND_EDGES_HZ[b] &&
          (hz < BAND_EDGES_HZ[b + 1] ||
           (b == FEATURES - 1 && hz <= BAND_EDGES_HZ[b + 1]));
      if (inBand) power += magnitudes[k] * magnitudes[k];
    }
    output[b] = log1pf(power);
  }
}

// Frame-wise normalization reduces sensitivity to loudness / mic distance.
// Every feature vector is centered and scaled to unit Euclidean length.
// We still use the 8 log-band energies from Task 2 as the input features.
void normalizeFeatureFrame(float f[FEATURES]) {
  float mean = 0.0f;
  for (uint8_t b = 0; b < FEATURES; ++b) mean += f[b];
  mean /= (float)FEATURES;
  float power = 0.0f;
  for (uint8_t b = 0; b < FEATURES; ++b) {
    f[b] -= mean;
    power += f[b] * f[b];
  }
  float scale = sqrtf(power);
  if (scale < 1.0e-5f) scale = 1.0f;
  for (uint8_t b = 0; b < FEATURES; ++b) f[b] /= scale;
}

// Retain a short lead-in (224 ms) and 64 ms after speech end.
// Task 1's 512-ms pre-roll and 512-ms trailing silence are still captured,
// but most of those long quiet intervals are excluded from DTW.
uint16_t buildSpeechFeatures(size_t beginSample, size_t endSample) {
  if (endSample <= beginSample || endSample > clipCount) return 0;
  queryFrames = 0;
  size_t n = endSample - beginSample;
  for (size_t at = 0; at + FRAME_SAMPLES <= n; at += FRAME_SAMPLES) {
    if (queryFrames >= MAX_VOICE_FRAMES) break;
    applyHannWindow(clipSamples + beginSample + at);
    computeSpectrum();
    float *target = queryFeatures + (size_t)queryFrames * FEATURES;
    extractLogBandFeatures(target);
    normalizeFeatureFrame(target);
    ++queryFrames;
  }
  return queryFrames;
}

// ---------------- Task 3 matching: classic constrained DTW ------------
// Per-frame distance: Euclidean distance between two 8-D unit vectors.
float featureDistance(const float *a, const float *b) {
  float squared = 0.0f;
  for (uint8_t i = 0; i < FEATURES; ++i) {
    float delta = a[i] - b[i];
    squared += delta * delta;
  }
  return sqrtf(squared);
}

// Two rolling DP rows: O(n*m) time, O(m) memory. Also track DTW path length
// so distances from unequal word durations are approximately comparable.
// The global cost-minimizing path is normalized by its chosen path length.
float computeDTWDistance(const float *a, uint16_t aCount,
                         const float *b, uint16_t bCount) {
  if (!a || !b || !aCount || !bCount ||
      aCount > MAX_VOICE_FRAMES || bCount > MAX_VOICE_FRAMES) return INF_COST;
  float longer = (float)(aCount > bCount ? aCount : bCount);
  float shorter = (float)(aCount < bCount ? aCount : bCount);
  if (longer > shorter * DTW_MAX_LENGTH_RATIO) return INF_COST;

  // Keeping this buffer on the stack saves PSRAM accesses in the DP loop.
  float prev[MAX_VOICE_FRAMES + 1], curr[MAX_VOICE_FRAMES + 1];
  uint16_t prevLen[MAX_VOICE_FRAMES + 1], currLen[MAX_VOICE_FRAMES + 1];
  for (uint16_t j = 0; j <= bCount; ++j) {
    prev[j] = INF_COST;
    prevLen[j] = 0;
  }
  prev[0] = 0.0f;

  for (uint16_t i = 1; i <= aCount; ++i) {
    curr[0] = INF_COST;
    currLen[0] = 0;
    for (uint16_t j = 1; j <= bCount; ++j) {
      // Relative-time alignment window permits moderate speed variation.
      float normI = (float)i / (float)aCount;
      float normJ = (float)j / (float)bCount;
      if (fabsf(normI - normJ) > DTW_WARP_FRACTION) {
        curr[j] = INF_COST;
        currLen[j] = 0;
        continue;
      }
      float best = prev[j - 1];
      uint16_t len = prevLen[j - 1];
      if (prev[j] < best) {
        best = prev[j];
        len = prevLen[j];
      }
      if (curr[j - 1] < best) {
        best = curr[j - 1];
        len = currLen[j - 1];
      }
      if (best >= INF_COST * 0.5f) {
        curr[j] = INF_COST;
        currLen[j] = 0;
      } else {
        curr[j] = best + featureDistance(
            a + (size_t)(i - 1) * FEATURES,
            b + (size_t)(j - 1) * FEATURES);
        currLen[j] = len + 1;
      }
    }
    for (uint16_t j = 0; j <= bCount; ++j) {
      prev[j] = curr[j];
      prevLen[j] = currLen[j];
    }
  }
  if (!prevLen[bCount] || prev[bCount] >= INF_COST * 0.5f) return INF_COST;
  return prev[bCount] / (float)prevLen[bCount];
}

// Median of 3: using more than one exemplar reduces accidental false matches.
float medianThree(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;
}

float classScore(const float *features, uint16_t frames,
                 uint8_t label, float d[3]) {
  for (uint8_t i = 0; i < TEMPLATES_PER_CLASS; ++i) {
    const TemplateEntry &t = templates[label * TEMPLATES_PER_CLASS + i];
    d[i] = t.valid ? computeDTWDistance(features, frames, t.features, t.frames)
                   : INF_COST;
  }
  return medianThree(d[0], d[1], d[2]);
}

// Estimate each command's acceptance threshold from its enrollment examples.
void calibrateAcceptanceThresholds() {
  for (uint8_t c = 0; c < CLASS_COUNT; ++c) {
    float withinMax = 0.0f;
    float betweenMin = INF_COST;
    for (uint8_t i = 0; i < TEMPLATES_PER_CLASS; ++i) {
      uint8_t srcIndex = c * TEMPLATES_PER_CLASS + i;
      const TemplateEntry &src = templates[srcIndex];
      float nearestSame = INF_COST;
      for (uint8_t j = 0; j < TEMPLATES_PER_CLASS; ++j) {
        if (i == j) continue;
        const TemplateEntry &other = templates[c * TEMPLATES_PER_CLASS + j];
        float d = computeDTWDistance(src.features, src.frames, other.features, other.frames);
        if (d < nearestSame) nearestSame = d;
      }
      if (nearestSame > withinMax) withinMax = nearestSame;
      uint8_t otherClass = (uint8_t)(1 - c);
      float between[3];
      float oppositeMedian = classScore(src.features, src.frames, otherClass, between);
      if (oppositeMedian < betweenMin) betweenMin = oppositeMedian;
    }
    float limit = withinMax * ENROLL_THRESHOLD_MULTIPLIER + ENROLL_THRESHOLD_OFFSET;
    if (limit < ACCEPT_THRESHOLD_FLOOR) limit = ACCEPT_THRESHOLD_FLOOR;
    if (limit > ACCEPT_THRESHOLD_CEILING) limit = ACCEPT_THRESHOLD_CEILING;
    if (betweenMin < INF_COST * 0.5f &&
        limit > betweenMin * CROSS_CLASS_LIMIT_RATIO) {
      limit = betweenMin * CROSS_CLASS_LIMIT_RATIO;
    }
    acceptThreshold[c] = limit;
    Serial.printf("# CALIBRATION class=%s within=%.3f opposite=%.3f threshold=%.3f\n",
                  className(c), withinMax, betweenMin, limit);
    if (withinMax >= limit || betweenMin <= withinMax) {
      Serial.printf("# WARNING class=%s templates overlap; retry ENROLL or use different words\n",
                    className(c));
    }
  }
}

// ------------------------- Audio sampling -----------------------------
bool readAudioFrame(int16_t *dest) {
  uint8_t *bytes = (uint8_t *)dest;
  size_t got = 0;
  while (got < FRAME_BYTES) {
    size_t n = microphone.readBytes((char *)(bytes + got), FRAME_BYTES - got);
    if (n == 0) return false;
    got += n;
  }
  return true;
}

float frameRms(const int16_t *s) {
  uint64_t acc = 0;
  for (size_t n = 0; n < FRAME_SAMPLES; ++n) {
    int64_t v = s[n];
    acc += (uint64_t)(v * v);
  }
  return sqrtf((float)((double)acc / (double)FRAME_SAMPLES));
}

void updateThreshold() {
  startThreshold = fmaxf(MIN_SPEECH_RMS, noiseRms * NOISE_MULTIPLIER);
}

void calibrateNoise() {
  Serial.println("# CALIBRATING: remain quiet for approximately 2 seconds");
  double sum = 0.0;
  uint16_t count = 0;
  while (count < NOISE_CALIBRATION_FRAMES) {
    if (!readAudioFrame(audioFrame)) continue;
    sum += frameRms(audioFrame);
    ++count;
  }
  noiseRms = (float)(sum / count);
  updateThreshold();
  Serial.printf("# CALIBRATED noise=%.1f threshold=%.1f\n", noiseRms, startThreshold);
}

void resetVad() {
  preFramesValid = 0;
  preWriteIndex = 0;
  activeRun = 0;
  quietRun = 0;
  clipCount = 0;
  speechStartSample = 0;
  vadState = WAITING_FOR_SPEECH;
}

void pushPreRoll(const int16_t *frame) {
  memcpy(preRoll[preWriteIndex], frame, FRAME_BYTES);
  preWriteIndex = (uint8_t)((preWriteIndex + 1) % PRE_ROLL_FRAMES);
  if (preFramesValid < PRE_ROLL_FRAMES) ++preFramesValid;
}

void appendClip(const int16_t *samples, size_t count) {
  size_t capacity = MAX_CLIP_SAMPLES - clipCount;
  if (count > capacity) count = capacity;
  if (count) {
    memcpy(clipSamples + clipCount, samples, count * sizeof(int16_t));
    clipCount += count;
  }
}

void appendPreRoll() {
  uint8_t first = preFramesValid == PRE_ROLL_FRAMES ? preWriteIndex : 0;
  for (uint8_t n = 0; n < preFramesValid; ++n) {
    uint8_t index = (uint8_t)((first + n) % PRE_ROLL_FRAMES);
    appendClip(preRoll[index], FRAME_SAMPLES);
  }
}

void beginSpeech() {
  clipCount = 0;
  quietRun = 0;
  // The pre-roll includes the frames that confirmed the start of speech.
  speechStartSample = (size_t)(preFramesValid >= START_ACTIVE_FRAMES
                             ? preFramesValid - START_ACTIVE_FRAMES : 0) * FRAME_SAMPLES;
  appendPreRoll();
  vadState = RECORDING_SPEECH;
  Serial.printf("# VOICE_BEGIN threshold=%.1f preroll_ms=%u\n",
                startThreshold, (unsigned)(PRE_ROLL_FRAMES * 32));
}

void emitEnrollmentPrompt() {
  if (mode != MODE_ENROLL || enrolled >= TEMPLATE_COUNT) return;
  uint8_t label = templateClass(enrolled);
  uint8_t ordinal = (uint8_t)(enrolled % TEMPLATES_PER_CLASS + 1);
  Serial.printf("# PROMPT label=%s sample=%u/3 -- say '%s', then pause\n",
                className(label), ordinal, className(label));
}

void finishSpeech(const char *reason) {
  ++clipNumber;
  uint32_t startMs = millis();
  // Reduce the 512-ms pre/post silence; leave a short lead-in for weak /s/.
  size_t first = speechStartSample;
  size_t lead = (size_t)LEAD_CONTEXT_FRAMES * FRAME_SAMPLES;
  if (first > lead) first -= lead;
  else first = 0;
  size_t last = clipCount;
  if (strcmp(reason, "silence") == 0) {
    size_t quietSamples = (size_t)END_QUIET_FRAMES * FRAME_SAMPLES;
    if (last > quietSamples) last -= quietSamples;
    last += (size_t)TAIL_CONTEXT_FRAMES * FRAME_SAMPLES;
    if (last > clipCount) last = clipCount;
  }
  // The two boundaries are frame aligned, and most trailing silence is dropped.
  queryFrames = buildSpeechFeatures(first, last);
  Serial.printf("# VOICE_END seq=%lu cause=%s raw_samples=%lu speech_frames=%u\n",
                (unsigned long)clipNumber, reason, (unsigned long)clipCount,
                (unsigned)queryFrames);
  if (queryFrames < MIN_VOICE_FRAMES) {
    Serial.println("# SKIP reason=too_short -- try speaking the word more clearly");
    if (mode == MODE_ENROLL) emitEnrollmentPrompt();
  } else if (mode == MODE_ENROLL && enrolled < TEMPLATE_COUNT) {
    uint8_t label = templateClass(enrolled);
    if (!storeTemplate(enrolled, queryFeatures, queryFrames)) {
      Serial.println("# TEMPLATE_NOT_SAVED: complete storeTemplate() in TODO 3.1");
      resetVad();
      emitEnrollmentPrompt();
      return;
    }
    ++enrolled;
    Serial.printf("# TEMPLATE label=%s saved=%u/3 frames=%u\n",
                  className(label), (unsigned)((enrolled - 1) % TEMPLATES_PER_CLASS + 1),
                  (unsigned)queryFrames);
    if (enrolled == TEMPLATE_COUNT) {
      calibrateAcceptanceThresholds();
      mode = MODE_RECOGNIZE;
      Serial.println("# READY mode=RECOGNIZE: say WAKE or SLEEP");
    } else {
      emitEnrollmentPrompt();
    }
  } else if (mode == MODE_RECOGNIZE) {
    Prediction p = matchCommand(queryFeatures, queryFrames);
    float best = p.candidateClass == 0 ? p.wakeScore : p.sleepScore;
    float runner = p.candidateClass == 0 ? p.sleepScore : p.wakeScore;
    Serial.printf("# SCORES seq=%lu wake=%.3f,%.3f,%.3f sleep=%.3f,%.3f,%.3f\n",
                  (unsigned long)clipNumber, p.distances[0], p.distances[1],
                  p.distances[2], p.distances[3], p.distances[4], p.distances[5]);
    const char *action = executeCommand(p.command);
    const char *predicted = p.command == COMMAND_UNKNOWN ? "UNKNOWN" :
                            className((uint8_t)p.command);
    Serial.printf("# RESULT seq=%lu predicted=%s candidate=%s score=%.3f runner=%.3f "
                  "threshold=%.3f action=%s led=%s processing_ms=%lu\n",
                  (unsigned long)clipNumber, predicted, className(p.candidateClass),
                  best, runner, p.threshold, action, ledOn ? "ON" : "OFF",
                  (unsigned long)(millis() - startMs));
  }
  // The next phrase starts after this clip has been processed.
  resetVad();
}

void handleFrame(const int16_t *samples) {
  if (mode == MODE_IDLE) return;
  float rms = frameRms(samples);
  if (vadState == WAITING_FOR_SPEECH) {
    pushPreRoll(samples);
    if (rms >= startThreshold) {
      if (activeRun < START_ACTIVE_FRAMES) ++activeRun;
    } else activeRun = 0;
    if (activeRun >= START_ACTIVE_FRAMES) {
      beginSpeech();
    } else if (activeRun == 0 && rms < 0.70f * startThreshold) {
      noiseRms = (1.0f - NOISE_UPDATE_ALPHA) * noiseRms +
                 NOISE_UPDATE_ALPHA * rms;
      updateThreshold();
    }
  } else {
    appendClip(samples, FRAME_SAMPLES);
    if (rms < startThreshold * END_THRESHOLD_RATIO) {
      if (quietRun < END_QUIET_FRAMES) ++quietRun;
    } else quietRun = 0;
    if (quietRun >= END_QUIET_FRAMES) finishSpeech("silence");
    else if (clipCount >= MAX_CLIP_SAMPLES) finishSpeech("max_5_seconds");
  }
  uint32_t now = millis();
  if ((uint32_t)(now - lastStatusMs) >= 2500) {
    Serial.printf("# LEVEL rms=%.1f noise=%.1f threshold=%.1f mode=%s\n",
                  rms, noiseRms, startThreshold,
                  mode == MODE_ENROLL ? "ENROLL" :
                  (mode == MODE_RECOGNIZE ? "RECOGNIZE" : "IDLE"));
    lastStatusMs = now;
  }
}

// ---------------------- Serial control protocol -----------------------
void beginEnrollment() {
  enrolled = 0;
  for (uint8_t i = 0; i < TEMPLATE_COUNT; ++i) {
    templates[i].valid = false;
    templates[i].frames = 0;
  }
  acceptThreshold[0] = acceptThreshold[1] = 0.0f;
  everAccepted = false;
  ledOn = false;
  digitalWrite(LED_PIN, HIGH);
  resetVad();
  mode = MODE_ENROLL;
  Serial.println("# ENROLL_BEGIN total=6: templates are lost when board resets");
  emitEnrollmentPrompt();
}

void pollCommands() {
  static char line[32];
  static uint8_t pos = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (pos < sizeof(line) - 1) line[pos++] = c;
      else pos = 0;
      continue;
    }
    line[pos] = '\0';
    if (strcmp(line, "ENROLL") == 0) {
      if (vadState != RECORDING_SPEECH) beginEnrollment();
      else Serial.println("# ERROR: wait for current speech to finish");
    } else if (strcmp(line, "STATUS") == 0) {
      Serial.printf("# STATUS mode=%s templates=%u/6 LED=%s noise=%.1f threshold=%.1f\n",
                    mode == MODE_RECOGNIZE ? "RECOGNIZE" :
                    (mode == MODE_ENROLL ? "ENROLL" : "IDLE"),
                    (unsigned)enrolled, ledOn ? "ON" : "OFF", noiseRms, startThreshold);
    } else if (strcmp(line, "STOP") == 0) {
      mode = MODE_IDLE;
      resetVad();
      Serial.println("# STOPPED");
    } else if (strcmp(line, "RECOGNIZE") == 0) {
      if (enrolled == TEMPLATE_COUNT) {
        mode = MODE_RECOGNIZE;
        resetVad();
        Serial.println("# READY mode=RECOGNIZE: say WAKE or SLEEP");
      } else Serial.println("# ERROR: first enroll all six templates");
    } else if (pos) {
      Serial.println("# ERROR: command must be ENROLL, STATUS, RECOGNIZE or STOP");
    }
    pos = 0;
  }
}

// ------------------------- Arduino entrypoints ------------------------
void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (uint32_t)(millis() - t0) < 1500) delay(10);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // active-low LED OFF
  Serial.println("# LAB6_TASK3 starting");
  if (!psramFound()) {
    Serial.println("# FATAL: Tools > PSRAM > OPI PSRAM required");
    while (true) delay(1000);
  }
  clipSamples = (int16_t *)ps_malloc(MAX_CLIP_SAMPLES * sizeof(int16_t));
  queryFeatures = (float *)ps_malloc((size_t)MAX_VOICE_FRAMES * FEATURES * sizeof(float));
  for (uint8_t i = 0; i < TEMPLATE_COUNT; ++i) {
    templates[i].features =
        (float *)ps_malloc((size_t)MAX_VOICE_FRAMES * FEATURES * sizeof(float));
    templates[i].frames = 0;
    templates[i].valid = false;
  }
  if (!clipSamples || !queryFeatures) {
    Serial.println("# FATAL: PSRAM allocation for audio/features failed");
    while (true) delay(1000);
  }
  for (uint8_t i = 0; i < TEMPLATE_COUNT; ++i) {
    if (!templates[i].features) {
      Serial.println("# FATAL: PSRAM template allocation failed");
      while (true) delay(1000);
    }
  }
  prepareWindow();
  microphone.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if (!microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("# FATAL: PDM microphone initialization failed");
    while (true) delay(1000);
  }
  microphone.setTimeout(250);
  calibrateNoise();
  Serial.println("# SEND ENROLL from voice_console.py to begin six examples");
}

void loop() {
  pollCommands();
  if (readAudioFrame(audioFrame)) handleFrame(audioFrame);
}
