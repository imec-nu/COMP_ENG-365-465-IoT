// Lab 6 Task 1. Complete TODOs 1.1 through 1.4. Tune sensitivity in config.h.
#include "vad_support.h"

void updateSpeechThreshold() {
  // TODO 1.1: Set startThreshold to the larger of MIN_SPEECH_RMS and
  // noiseRms * NOISE_MULTIPLIER. fmaxf(a, b) returns the larger float.
  // YOUR CODE HERE
}

void adaptBackgroundNoise(float rms) {
  // TODO 1.1 (continued): Move noiseRms a little toward this quiet frame.
  // New estimate = (1 - alpha) * old estimate + alpha * rms,
  // using NOISE_UPDATE_ALPHA for alpha. Then call updateSpeechThreshold().
  // YOUR CODE HERE
}

void appendPreRollInOrder() {
  // TODO 1.2: Append all preFramesValid frames from oldest to newest.
  // preWriteIndex is the NEXT slot to overwrite. If the ring is full,
  // that slot is the oldest; otherwise the oldest slot is 0.
  // Wrap each index with % PRE_ROLL_FRAMES and call
  // appendClip(preRoll[index], FRAME_SAMPLES) for each valid frame.
  // YOUR CODE HERE
}

void processWaitingFrame(const int16_t *frame, float rms) {
  // TODO 1.3: Call pushPreRoll(frame) before testing the onset.
  // Count consecutive frames with rms >= startThreshold in activeRun;
  // reset activeRun to 0 when a frame is below the threshold.
  // Call beginSpeechClip() once activeRun reaches START_ACTIVE_FRAMES.
  // Otherwise, adapt the background only when activeRun == 0 and
  // rms < startThreshold * 0.70f. Call adaptBackgroundNoise(rms) then.
  // beginSpeechClip() copies the ring, including the current frame.
  // YOUR CODE HERE
}

void processRecordingFrame(const int16_t *frame, float rms) {
  // TODO 1.4: Append this frame with appendClip(frame, FRAME_SAMPLES).
  // Count consecutive frames with rms below
  // startThreshold * END_THRESHOLD_RATIO in quietRun; reset it otherwise.
  // If quietRun reaches END_QUIET_FRAMES, call finishSpeechClip("silence").
  // Otherwise, if clipCount reaches MAX_SAMPLES, finish with "max_5_seconds".
  // appendClip() limits the copy to the remaining space in the clip buffer.
  // YOUR CODE HERE
}

