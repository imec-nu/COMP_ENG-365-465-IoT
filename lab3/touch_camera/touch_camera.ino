#include <Arduino.h>

// Raise this ratio if an untouched wire triggers; lower it if touches are missed.
const float TOUCH_THRESHOLD_RATIO = 1.20f;
#include "touch_camera_support.h"

bool captureRequested(bool touched, TouchState &state) {
  // TODO 4.1: YOUR CODE HERE
  // Confirm a touch after DEBOUNCE_COUNT consecutive true samples.
  // Return true once, then latch. Require DEBOUNCE_COUNT consecutive false
  // samples to rearm. A held touch or a one-sample release must not retrigger.
  // Use state.touchCount, state.releaseCount, and state.latched.
  return false;
}

bool nextImagePath(char *path, size_t capacity, uint32_t &number) {
  // TODO 4.2: YOUR CODE HERE
  // Starting at number, find an unused /image_NNNN.jpg below IMAGE_NUMBER_LIMIT.
  // Use formatImagePath(path, capacity, number) and SD.exists(path).
  // Leave path and number at the unused candidate; return true when found.
  // Return false if formatting fails or the number limit is reached.
  return false;
}

bool capturePhoto(const char *path) {
  // TODO 4.3: YOUR CODE HERE
  // Refuse an existing path. Acquire a frame with captureFreshFrame().
  // Open SD.open(path, FILE_WRITE), then write frame->buf with length frame->len.
  // Check acquisition, file open, JPEG/XGA format, and the number of bytes written.
  // Close every opened file and return every acquired frame with
  // esp_camera_fb_return(frame), including error paths. Never use it afterward.
  // Remove an incomplete file with SD.remove(path). Return true only for a
  // complete, nonempty JPEG.
  return false;
}
