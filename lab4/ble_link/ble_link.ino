// Lab 4 Task 1. Edit TODO 1.1/1.2 here, 1.3 in ble_logic.h, and 1.4/1.5 in tools/ble_client.py.
// Set your board name in config.h. ble_support.h handles connections and scheduling.
#include "ble_support.h"

bool createLink(BLEService *service) {
  // TODO 1.1: use service->createCharacteristic() to create TX_UUID with
  // PROPERTY_NOTIFY and RX_UUID with PROPERTY_WRITE. Assign TX to global tx.
  // Call configureTxSubscription(tx), then attach a new RxCallbacks object
  // to RX with setCallbacks(). Return true when both characteristics and
  // their callbacks are ready.
  (void)service;
  return false;  // Replace this placeholder.
}

bool sendHello() {
  // TODO 1.2: format "HELLO <sequence>" using nextSequence.
  // Pass the text and its length to tx->setValue(), without a newline or NUL.
  // Before tx->notify(), save the sequence in run.pendingSeq, save millis()
  // in run.sentAtMs, and set run.pending to true.
  // Increment nextSequence once per new HELLO. Return true after notifying.
  return false;  // Replace this placeholder.
}

