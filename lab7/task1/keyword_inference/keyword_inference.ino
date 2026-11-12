// Train the model before compiling this sketch. See README.md in task1.
#include "inference_support.h"

bool prepareModelInput() {
  // TODO 1.6: Fill inputTensor->data.int8 from features, keeping their order.
  // For each of NFRAMES * NMELS values, normalize with INPUT_MEAN and INPUT_STD.
  // Then q = round(normalized / INPUT_SCALE) + INPUT_ZERO_POINT.
  // Use lrintf(), clamp q to [-128, 127], and store it as int8_t.
  // Return false for non-finite features; return true after filling the tensor.
  // YOUR CODE HERE
  return false;
}

const char *interpretOutput(float *scores, int &best) {
  // TODO 1.7: Convert each INT8 output into a float score:
  // score = (q - outputTensor->params.zero_point) * outputTensor->params.scale.
  // Store all CLASS_COUNT scores and set best to the largest score's index.
  // Return "UNKNOWN" when best is UNKNOWN_INDEX or its score is below
  // ACCEPT_THRESHOLD. Otherwise return CLASS_LABELS[best].
  // YOUR CODE HERE
  return nullptr;
}
