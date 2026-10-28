// Lab 5 Task 3. Complete TODOs 3.1/3.2 here and 3.3/3.4 in script.h.
#include "camera_support.h"

camera_fb_t *captureJpeg() {
  // TODO 3.1: YOUR CODE HERE
  // Get a frame with esp_camera_fb_get(). If it returns nullptr, send
  // HTTP 500 with http.send() and return nullptr.
  // If frame->format is not PIXFORMAT_JPEG, return the frame to the camera,
  // send HTTP 500, and return nullptr. Otherwise, return the frame pointer.
  // Replace both placeholder lines below.
  http.send(501, "text/plain", "Complete TODO 3.1.");
  return nullptr;
}

void handleCapture() {
  camera_fb_t *frame = captureJpeg();
  if (!frame) return;

  // TODO 3.2: YOUR CODE HERE
  // Use http.sendHeader("Cache-Control", "no-store") and
  // http.setContentLength(frame->len) to describe the response.
  // Use http.send(200, "image/jpeg", "") to send the HTTP headers.
  // Then call writePhotoBytes(frame->buf, frame->len).
  // Return the frame with esp_camera_fb_return() after the write finishes,
  // whether or not all bytes were sent. The buffer belongs to the camera.
  // Replace both placeholder lines below.
  esp_camera_fb_return(frame);
  http.send(501, "text/plain", "Complete TODO 3.2.");
}
