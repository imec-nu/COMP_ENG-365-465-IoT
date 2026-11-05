#pragma once

#include <Arduino.h>

// ------------------------ Hardware / audio ----------------------------
static constexpr int PDM_CLK_PIN = 42;
static constexpr int PDM_DATA_PIN = 41;
static constexpr int LED_PIN = 21;          // active-low
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t FRAME_SAMPLES = 512; // 32 ms
static constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
static constexpr uint32_t MAX_CLIP_SECONDS = 5;
static constexpr size_t MAX_CLIP_SAMPLES = SAMPLE_RATE * MAX_CLIP_SECONDS;
static constexpr uint8_t PRE_ROLL_FRAMES = 16;   // 512 ms including trigger frames
static constexpr uint8_t START_ACTIVE_FRAMES = 2; // 64 ms
static constexpr uint8_t END_QUIET_FRAMES = 16;   // 512 ms
static constexpr uint8_t NOISE_CALIBRATION_FRAMES = 63;

// Tune these from the LEVEL readings in your room.
static constexpr float NOISE_MULTIPLIER = 1.12f;
static constexpr float MIN_SPEECH_RMS = 100.0f;
static constexpr float END_THRESHOLD_RATIO = 0.97f;
static constexpr float NOISE_UPDATE_ALPHA = 0.015f;

// -------------------------- Speech features ---------------------------
static constexpr uint8_t FEATURES = 8;
static constexpr uint16_t FFT_BINS = FRAME_SAMPLES / 2 + 1;
static constexpr uint16_t SPECTRUM_BINS = 129;  // 0 through 4000 Hz
static constexpr uint16_t BAND_EDGES_HZ[FEATURES + 1] = {
    0, 250, 500, 750, 1000, 1500, 2000, 3000, 4000};
static constexpr uint16_t MAX_VOICE_FRAMES = 156; // <= 5s / 32ms
static constexpr uint8_t LEAD_CONTEXT_FRAMES = 7; // 224 ms; preserve weak initial /s/
static constexpr uint8_t TAIL_CONTEXT_FRAMES = 2; // 64 ms after speech end
static constexpr uint8_t MIN_VOICE_FRAMES = 6;    // skip clicks / very short clips

// --------------------- Template matching parameters -------------------
static constexpr uint8_t TEMPLATES_PER_CLASS = 3;
static constexpr uint8_t CLASS_COUNT = 2;
static constexpr uint8_t TEMPLATE_COUNT = CLASS_COUNT * TEMPLATES_PER_CLASS;
// These parameters apply to the *normalized* 8-band feature vectors.
// The board estimates a separate acceptance threshold for each command.
static constexpr float ENROLL_THRESHOLD_MULTIPLIER = 1.80f;
static constexpr float ENROLL_THRESHOLD_OFFSET = 0.035f;
static constexpr float ACCEPT_THRESHOLD_FLOOR = 0.18f;
static constexpr float ACCEPT_THRESHOLD_CEILING = 0.90f;
static constexpr float CROSS_CLASS_LIMIT_RATIO = 0.85f;
static constexpr float RUNNER_UP_RATIO = 0.98f;
static constexpr uint32_t COMMAND_COOLDOWN_MS = 1000;
static constexpr float DTW_MAX_LENGTH_RATIO = 2.5f;
static constexpr float DTW_WARP_FRACTION = 0.30f;
static constexpr float INF_COST = 1.0e20f;

