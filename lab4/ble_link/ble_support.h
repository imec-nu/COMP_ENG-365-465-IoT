#pragma once
// Supplied BLE event queue, callbacks, and scheduler. No TODOs in this tab.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <atomic>
#include "config.h"
#include "ble_logic.h"

constexpr char SERVICE_UUID[] = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr char TX_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
constexpr char RX_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
constexpr uint32_t PERIOD_MS = 500;
constexpr uint32_t EXCHANGES = 10;
BLECharacteristic *tx = nullptr;
BLE2902 *cccd = nullptr;
QueueHandle_t events = nullptr;
std::atomic<bool> queueOverflow{false};

enum EventKind : uint8_t { CONNECTED, DISCONNECTED, SUBSCRIPTION, ACK_RECEIVED };
struct LinkEvent { EventKind kind; uint32_t value; uint32_t atMs; };

// Supplied plumbing: callbacks run on a different task. A queue transfers
// small events to loop(); callbacks never change the measurement state.
void postEvent(EventKind kind, uint32_t value = 0) {
  LinkEvent event{kind, value, millis()};
  if (xQueueSend(events, &event, 0) != pdTRUE) queueOverflow.store(true);
}

// Supplied strict parser: malformed input cannot accidentally become ACK 0.
bool parseAck(const String &text, uint32_t &sequence) {
  if (!text.startsWith("ACK ") || text.length() < 5 || text.length() > 14) return false;
  uint64_t value = 0;
  for (size_t i = 4; i < text.length(); ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
    if (value > UINT32_MAX) return false;
  }
  sequence = static_cast<uint32_t>(value);
  return true;
}
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { postEvent(CONNECTED); }
  void onDisconnect(BLEServer *) override {
    // Do not inherit the previous client's subscription on reconnect.
    if (cccd) cccd->setNotifications(false);
    postEvent(DISCONNECTED);
  }
};
class SubscriptionCallbacks : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor *) override {
    postEvent(SUBSCRIPTION, cccd->getNotifications() ? 1 : 0);
  }
};
// Supplied compatibility shim: core 3.3.12 reports subscriptions through
// onSubscribe; older Bluedroid cores report a write to descriptor 0x2902.
#if defined(CONFIG_NIMBLE_ENABLED)
class TxCallbacks : public BLECharacteristicCallbacks {
  void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *, uint16_t value) override {
    postEvent(SUBSCRIPTION, (value & 1) != 0);
  }
};
#endif
void configureTxSubscription(BLECharacteristic *characteristic) {
#if defined(CONFIG_NIMBLE_ENABLED)
  characteristic->setCallbacks(new TxCallbacks());
#else
  cccd = new BLE2902();
  cccd->setCallbacks(new SubscriptionCallbacks());
  characteristic->addDescriptor(cccd);
#endif
}

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    uint32_t sequence;
    if (parseAck(characteristic->getValue(), sequence)) postEvent(ACK_RECEIVED, sequence);
  }
};

bool createLink(BLEService *service);
bool sendHello();

RttRun run;
uint32_t nextSequence = 0;  // Keep increasing across reconnects.

bool enabled = false;
bool connected = false;
bool subscribed = false;
bool finished = false;
uint32_t lastAttemptMs = 0;
void setup() {
  Serial.begin(115200);
  delay(250);
  if (String(BOARD_NAME) == "XIAO-L4-yourid" || strlen(BOARD_NAME) > 20) {
    Serial.println("Setup: set a unique BOARD_NAME (at most 20 ASCII characters) in config.h.");
    return;
  }
  events = xQueueCreate(32, sizeof(LinkEvent));
  if (!events) { Serial.println("Cannot allocate BLE event queue."); return; }
  BLEDevice::init(BOARD_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *service = server->createService(SERVICE_UUID);
  if (!createLink(service) || !tx) {
    Serial.println("TODO 1.1 is incomplete; advertising has not started.");
    return;
  }
  service->start();
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  enabled = true;
  Serial.printf("Advertising %s. Run the PC client.\n", BOARD_NAME);
}

void loop() {
  if (!enabled) { delay(20); return; }
  if (queueOverflow.exchange(false)) {
    // Fail visibly rather than publish a result after missing control events.
    enabled = false;
    Serial.println("BLE event queue overflow. Reset the board and reconnect.");
    return;
  }
  LinkEvent event;
  while (xQueueReceive(events, &event, 0) == pdTRUE) {
    if (event.kind == CONNECTED) {
      connected = true; subscribed = false; finished = false; run = RttRun{};
      Serial.println("Connected; waiting for the TX subscription.");
    } else if (event.kind == DISCONNECTED) {
      connected = false; subscribed = false; run = RttRun{}; finished = false;
      BLEDevice::startAdvertising();
      Serial.println("Disconnected; advertising again. Re-run the PC client to begin a new run.");
    } else if (event.kind == SUBSCRIPTION) {
      subscribed = event.value != 0;
      run = RttRun{}; finished = false;
      lastAttemptMs = millis();
      Serial.println(subscribed ? "Subscribed. Starting a fresh 10-exchange run." : "Unsubscribed.");
    } else if (event.kind == ACK_RECEIVED && connected && subscribed) {
      uint32_t rtt;
      if (run.acceptAck(event.value, event.atMs, rtt)) {
        Serial.printf("ACK %lu: RTT=%lu ms (%lu/10)\n", (unsigned long)event.value,
                      (unsigned long)rtt, (unsigned long)run.completed);
      } else {
        Serial.printf("Ignoring unmatched/stale ACK %lu.\n", (unsigned long)event.value);
      }
    }
  }
  if (!connected || !subscribed || finished) { delay(2); return; }
  const uint32_t now = millis();
  if (run.completed == EXCHANGES) {
    Serial.printf("Mean board RTT over 10 matched exchanges: %.2f ms\n", run.totalMs / 10.0);
    tx->setValue("DONE"); tx->notify();
    finished = true;
  } else if (run.pending && uint32_t(now - run.sentAtMs) >= ACK_TIMEOUT_MS) {
    Serial.printf("ACK %lu timed out; check TODO 1.3, RX, and the PC reply; trying a new sequence.\n", (unsigned long)run.pendingSeq);
    run.pending = false;  // Timeout is not an RTT sample.
    lastAttemptMs = now;
  } else if (!run.pending && uint32_t(now - lastAttemptMs) >= PERIOD_MS) {
    lastAttemptMs = now;
    if (!sendHello()) {
      Serial.println("TODO 1.2 is incomplete; no HELLO was sent.");
      enabled = false;
    }
  }
  delay(2);
}
