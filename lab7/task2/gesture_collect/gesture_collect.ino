// Complete TODO 2.1 in image_transform.h, then TODO 2.2 below.
#include "collection_support.h"

bool captureExample() {
  // TODO 2.2: YOUR CODE HERE
  // Use captureTimed() to get one camera_fb_t*. Return false if it is null.
  // Call prepareTimed(fb); if it succeeds, call sendExample(fb).
  // Both functions return bool. Save the result, then always return the frame
  // with esp_camera_fb_return(fb), including when processing or sending fails.
  // Return true only when preparation and transmission both succeeded.
  return false;
}

void setup() {
  beginCollection();
}

void loop() {
  pollCollection();
}
