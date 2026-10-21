#pragma once
#include <stdint.h>

constexpr uint32_t ACK_TIMEOUT_MS = 2000;

// loop() updates this state when it processes queued BLE events.
struct RttRun {
  bool pending = false;
  uint32_t pendingSeq = 0;
  uint32_t sentAtMs = 0;
  uint32_t completed = 0;
  uint64_t totalMs = 0;

  bool acceptAck(uint32_t sequence, uint32_t receivedAtMs, uint32_t &rttMs) {
    // TODO 1.3: match the ACK to the pending HELLO. Calculate elapsed time
    // as receivedAtMs - sentAtMs using uint32_t arithmetic to handle millis()
    // wrapping. Reject the ACK if pending is false, sequence differs from
    // pendingSeq, or elapsed time is >= ACK_TIMEOUT_MS. Return false without
    // changing any RttRun fields or rttMs when you reject a reply.
    // For a match, put the elapsed time in rttMs, clear pending, increment
    // completed, add the elapsed time to totalMs, and return true.
    (void)sequence; (void)receivedAtMs; (void)rttMs;
    return false;  // Replace this placeholder.
  }
};
