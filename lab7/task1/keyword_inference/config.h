#pragma once
#include <Arduino.h>

// Keep these audio settings identical in collection and inference.
constexpr int SR = 16000;
constexpr int NSAMPLES = 32000;
constexpr int NFFT = 512;
constexpr int HOP = 256;
constexpr int NMELS = 32;
constexpr int NFRAMES = 1 + (NSAMPLES - NFFT) / HOP;
constexpr int MIC_CLOCK_PIN = 42;
constexpr int MIC_DATA_PIN = 41;
constexpr uint32_t READ_TIMEOUT_MS = 6000;
constexpr size_t ARENA_BYTES = 256 * 1024;
constexpr uint32_t AUTO_GAP_MS = 1500;
