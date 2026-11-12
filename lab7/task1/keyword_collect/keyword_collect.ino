#include "collect_support.h"

// TODO 1.1
// Read exactly bytes bytes into destination. The complete clip requests
// NSAMPLES * sizeof(int16_t): 32,000 samples, or 64,000 bytes.
// mic.readBytes(buffer, count) may return fewer bytes than requested.
// Advance the destination by the number received, then request the remainder.
// Stop on a zero-byte read or after READ_TIMEOUT_MS; return true only if full.
bool readExact(void *destination, size_t bytes) {
  // YOUR CODE HERE
  return false;
}
