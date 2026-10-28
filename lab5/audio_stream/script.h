#pragma once
#include <pgmspace.h>

const char SCRIPT_JS[] PROGMEM = R"LAB5JS(
"use strict";

// The supplied support.js defines the state used below and handles the page UI.
// These functions run in the browser. Re-upload and reload the page after editing.

function createAudioSocket() {
  // TODO 2.3: open a WebSocket to this page's host on port 81.
  // Use location.hostname for the address and set binaryType to "arraybuffer".
  // Return the socket. The supplied code attaches the message handlers.
  // YOUR CODE HERE
  return null;
}

function decodeAndStore(buffer) {
  // TODO 2.4: buffer contains signed 16-bit PCM, low byte first.
  // Read sample i with view.getInt16(i * 2, true). Store it in samples[i]
  // and circular[writePos], then advance writePos modulo WINDOW_SAMPLES.
  // Add samples.length to totalReceived and return samples.
  // The supplied receiver has already checked that the byte count is even.
  const view = new DataView(buffer);
  const samples = new Int16Array(buffer.byteLength / 2);
  // YOUR CODE HERE
  return samples;
}

function startRecording() {
  if (!ws || ws.readyState !== WebSocket.OPEN || encoding) return;
  // TODO 2.5a: begin a new recording. Empty recordedChunks, reset
  // recordedCount to zero, and set recording to true.
  // YOUR CODE HERE
  showRecordingStarted();
}

function appendRecording(samples) {
  // TODO 2.5b: when recording is true, push a copy of samples into
  // recordedChunks and add samples.length to recordedCount.
  // Use samples.slice() to give the recording its own array.
  // The caller has already limited this packet to the remaining recording time.
  // YOUR CODE HERE
}

function stopRecording(note = "") {
  if (!recording) return;
  // TODO 2.5c: stop accepting samples by setting recording to false.
  // Save recordedChunks and recordedCount in local variables, then replace
  // recordedChunks with an empty array and reset recordedCount to zero.
  // Pass the saved chunks, saved count, and note to finishRecording().
  // That supplied function creates the MP3 and displays its download link.
  // YOUR CODE HERE
}

startPage();
)LAB5JS";
