// Types used by Task 3 functions.
// Kept in a header so Arduino's auto-generated function prototypes see them.
#pragma once
#include <stdint.h>

enum Command : uint8_t { COMMAND_WAKE, COMMAND_SLEEP, COMMAND_UNKNOWN };
struct Prediction {
  Command command;
  uint8_t candidateClass;
  float wakeScore;
  float sleepScore;
  float threshold;
  float distances[6];
};
