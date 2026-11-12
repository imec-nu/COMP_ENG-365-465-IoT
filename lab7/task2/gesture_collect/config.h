#pragma once
#include <stddef.h>

// Keep these settings the same during collection and inference.
constexpr int RAW_W = 320;
constexpr int RAW_H = 240;
constexpr int INPUT_W = 64;
constexpr int INPUT_H = 64;
constexpr int CROP_X = 40;
constexpr int CROP_Y = 0;
constexpr int CROP_SIZE = 240;
constexpr size_t ARENA_BYTES = 256 * 1024;
constexpr unsigned long AUTO_INTERVAL_MS = 500;
