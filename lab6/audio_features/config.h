#pragma once
#include <Arduino.h>

// Microphone, frame size, and VAD settings. Use your Task 1 tuning here.
static constexpr int PDM_CLK_PIN = 42;
static constexpr int PDM_DATA_PIN = 41;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t FRAME_SAMPLES = 512;
static constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
static constexpr uint32_t MAX_CLIP_SECONDS = 5;
static constexpr size_t MAX_CLIP_SAMPLES = SAMPLE_RATE * MAX_CLIP_SECONDS;
static constexpr uint16_t MAX_TEST_FRAMES = 400;  // enough for 10s WAV (~313)
static constexpr size_t TEST_SERIAL_CHUNK = 128; // request/response chunk, avoid CDC RX overruns
static constexpr uint16_t MAX_FEATURE_FRAMES = MAX_TEST_FRAMES;
static constexpr uint8_t FEATURES_PER_FRAME = 8;
static constexpr uint16_t FFT_BINS = FRAME_SAMPLES / 2 + 1; // 257 bins: 0..8000Hz
static constexpr uint16_t SPECTRUM_BINS = 129;  // exported bins 0..128 => 0..4000Hz
static constexpr uint8_t PRE_ROLL_FRAMES = 16;  // 512ms pre-roll, includes onset frames
static constexpr uint8_t START_ACTIVE_FRAMES = 3;
static constexpr uint8_t END_QUIET_FRAMES = 16; // 512ms
static constexpr uint8_t NOISE_CALIBRATION_FRAMES = 63; // ~2 seconds

// Tune these after observing real microphone readings (PCM RMS counts).
static constexpr float NOISE_MULTIPLIER = 1.8f;
static constexpr float MIN_SPEECH_RMS = 150.0f;
static constexpr float END_THRESHOLD_RATIO = 0.75f;
static constexpr float NOISE_UPDATE_ALPHA = 0.015f;

// Bands are [left,right), except the final band includes 4000Hz.
static constexpr uint16_t BAND_EDGES_HZ[FEATURES_PER_FRAME + 1] = {
    0, 250, 500, 750, 1000, 1500, 2000, 3000, 4000};

