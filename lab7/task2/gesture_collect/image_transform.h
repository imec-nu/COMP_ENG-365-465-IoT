#pragma once
#include <stdint.h>
#include "config.h"

// rgb contains RAW_W * RAW_H interleaved three-byte pixels.
// gray has room for INPUT_W * INPUT_H bytes, stored row by row.
inline bool makeModelInput(const uint8_t *rgb, uint8_t *gray) {
  // TODO 2.1: YOUR CODE HERE
  // For every output (x,y), sample the center of its cell in the crop:
  // sx = CROP_X + ((2*x + 1) * CROP_SIZE) / (2*INPUT_W), and likewise for sy.
  // Find that source pixel at rgb + 3*(sy*RAW_W + sx).
  // Store the rounded channel mean (R + G + B + 1)/3 at gray[y*INPUT_W + x].
  // Add the channels as integers so their sum can exceed 255. Return true.
  return false;
}
