#pragma once
#include <Arduino.h>

// Microphone on the XIAO ESP32S3 Sense expansion board.
static constexpr int PDM_CLK_PIN = 42;
static constexpr int PDM_DATA_PIN = 41;
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t FRAME_SAMPLES = 512;      // 32 ms
static constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
static constexpr uint8_t PRE_ROLL_FRAMES = 16;    // 512 ms, including onset frames
static constexpr uint8_t START_ACTIVE_FRAMES = 3; // 96 ms
static constexpr uint8_t END_QUIET_FRAMES = 16;   // 512 ms
static constexpr uint32_t MAX_RECORD_SECONDS = 5;
static constexpr size_t MAX_SAMPLES = SAMPLE_RATE * MAX_RECORD_SECONDS;
static constexpr uint32_t NOISE_CALIBRATION_FRAMES = 63; // about 2 seconds

// Tune these from the RMS readings printed in your room.
// RMS values are PCM sample counts, not a calibrated sound level in dB.
static constexpr float NOISE_MULTIPLIER = 1.8f;
static constexpr float MIN_SPEECH_RMS = 350.0f;
static constexpr float END_THRESHOLD_RATIO = 0.75f;
static constexpr float NOISE_UPDATE_ALPHA = 0.015f;
static constexpr uint32_t STATUS_INTERVAL_MS = 2000;
