// Copy your completed image_transform.h from gesture_collect into this folder.
// Train the model, then copy model_data.h and model_config.h here as well.
#include "inference_support.h"

bool fillModelInput() {
  // TODO 2.5: YOUR CODE HERE
  // Convert all INPUT_W*INPUT_H grayscale bytes into inputTensor->data.int8.
  // Training uses pixel/255.0. Quantize that value with INPUT_SCALE and
  // INPUT_ZERO_POINT: q = lrintf((pixel/255.0f)/INPUT_SCALE) + INPUT_ZERO_POINT.
  // Clamp q to [-128,127] before casting to int8_t. Return true when filled.
  return false;
}

int decodePrediction(float *scores, const char *&prediction) {
  // TODO 2.6: YOUR CODE HERE
  // Fill scores[i] from outputTensor->data.int8[i] for each class:
  // score = (stored value - outputTensor->params.zero_point)
  //         * outputTensor->params.scale.
  // Find the index of the largest score. If it is below ACCEPT_THRESHOLD,
  // set prediction to "UNKNOWN". Otherwise use "NO_GESTURE" for
  // BACKGROUND_INDEX, or CLASS_LABELS[best] for a gesture. Return best.
  return -1;
}

void setup() {
  beginInference();
}

void loop() {
  pollInference();
}
