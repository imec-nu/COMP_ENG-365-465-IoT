// Task 2: stream microphone audio to the browser.
// Complete TODOs 2.1 and 2.2 here, then 2.3 through 2.5 in script.h.
#include "audio_support.h"

bool beginMicrophone() {
  // TODO 2.1: set the PDM clock/data pins with microphone.setPinsPdmRx().
  // Then call microphone.begin() for PDM RX, SAMPLE_RATE, 16-bit, mono.
  // Return the result of begin().
  // YOUR CODE HERE
  return false;
}

void streamAudioPacket() {
  // TODO 2.2: read up to sizeof(pcmPacket) bytes into pcmPacket with
  // microphone.readBytes((char *)pcmPacket, sizeof(pcmPacket)).
  // Keep reading even when no browser is connected, so old audio does not queue up.
  // Use ws.connectedClients() to check whether a browser is connected.
  // If the read returns a nonzero, even byte count and a client is connected,
  // send exactly those bytes with ws.broadcastBIN(pcmPacket, bytesRead).
  // Each signed 16-bit sample occupies two bytes.
  // YOUR CODE HERE
}
