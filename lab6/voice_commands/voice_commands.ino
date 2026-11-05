// Lab 6 Task 3. Complete the three functions below.
// Adjust microphone sensitivity and matching settings in config.h.
#include "command_support.h"

bool storeTemplate(uint8_t index, const float *features, uint16_t frames) {
  if (index >= TEMPLATE_COUNT || !features || !frames ||
      frames > MAX_VOICE_FRAMES || !templates[index].features) return false;
  TemplateEntry &entry = templates[index];
  // TODO 3.1: YOUR CODE HERE
  // Copy frames * FEATURES floats from features into entry.features.
  // memcpy() takes a byte count; include sizeof(float) in that count.
  // Save frames in entry.frames and set entry.valid to true, then return true.
  // The supplied code advances to the next example after this returns true.
  return false;
}

Prediction matchCommand(const float *features, uint16_t frames) {
  Prediction p = {};
  p.command = COMMAND_UNKNOWN;
  p.wakeScore = p.sleepScore = INF_COST;
  for (uint8_t i = 0; i < TEMPLATE_COUNT; ++i) p.distances[i] = INF_COST;
  if (enrolled != TEMPLATE_COUNT || !features || frames < MIN_VOICE_FRAMES ||
      frames > MAX_VOICE_FRAMES) return p;
  // TODO 3.2: YOUR CODE HERE
  // classScore(features, frames, label, distances) returns the median DTW score
  // for a class and writes its three distances into the supplied array.
  // Fill p.wakeScore with label 0 and p.distances; fill p.sleepScore with
  // label 1 and p.distances + 3. Set p.candidateClass to the lower-score label.
  // Set p.threshold from acceptThreshold[p.candidateClass]. Accept only when
  // best <= threshold, best < runnerUp, and best <= runnerUp * RUNNER_UP_RATIO.
  // Set p.command to COMMAND_WAKE or COMMAND_SLEEP when accepted.
  return p;
}

const char *executeCommand(Command command) {
  // TODO 3.3: YOUR CODE HERE
  // Return "UNKNOWN" for COMMAND_UNKNOWN without changing the LED or timing.
  // If everAccepted is true and millis() - lastAcceptedMs is less than
  // COMMAND_COOLDOWN_MS, return "COOLDOWN" without changing either one.
  // Otherwise, WAKE turns the active-low LED on; SLEEP turns it off.
  // Update ledOn, lastAcceptedMs, and everAccepted after executing the command.
  // Return "LED_ON" or "LED_OFF" to describe the action.
  return "NOT_IMPLEMENTED";
}
