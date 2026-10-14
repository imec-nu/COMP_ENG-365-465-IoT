#pragma once
// Supplied PDM setup, bounded audio reads, calibration, and USB output.
I2SClass I2S;
uint32_t samplesRead = 0;
uint32_t samplesPrinted = 0;
uint32_t samplesNotPrinted = 0;
uint32_t audioReadErrors = 0;
bool microphoneEnded = false;
bool stopMessagePending = false;

void setLed(bool on) {
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

bool sendTextIfReady(const char *text, size_t length) {
  if (!Serial || Serial.availableForWrite() < static_cast<int>(length)) return false;
  return Serial.write(reinterpret_cast<const uint8_t *>(text), length) == length;
}

void beginHardware() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);  // USB output must not hold up stop handling.
#endif
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  if (TOUCH_CALIBRATION_MODE) return;

  I2S.setPinsPdmRx(42, 41);
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    const char message[] = "Microphone initialization failed\n";
    while (true) {
      sendTextIfReady(message, sizeof(message) - 1);
      delay(1000);
    }
  }
}

void showTouchCalibration() {
  const uint32_t raw = touchRead(TOUCH_PIN);
  char line[48];
  const int length = snprintf(line, sizeof(line), "D0/GPIO1 raw: %lu\n", static_cast<unsigned long>(raw));
  if (length > 0) sendTextIfReady(line, length);
  delay(100);
}

size_t readAudioBlock(int16_t *samples, size_t capacity) {
  size_t bytesRead = 0;
  // Use the native API so valid partial data is not lost on a timeout.
  const esp_err_t result = i2s_channel_read(I2S.rxChan(), samples,
      capacity * sizeof(int16_t), &bytesRead, READ_TIMEOUT_MS);
  if (result != ESP_OK && result != ESP_ERR_TIMEOUT) {
    ++audioReadErrors;
    return 0;
  }
  return bytesRead / sizeof(int16_t);
}

void printRawSample(int16_t sample) {
  char line[12];
  const int length = snprintf(line, sizeof(line), "%d\n", static_cast<int>(sample));
  if (length > 0 && sendTextIfReady(line, length)) {
    ++samplesPrinted;
  } else {
    ++samplesNotPrinted;
  }
}

void queueStopMessage() {
  stopMessagePending = true;
}

void sendStopMessageIfReady() {
  if (!stopMessagePending) return;
  char line[180];
  const int length = snprintf(line, sizeof(line),
      "%s\nRead=%lu Printed=%lu SerialSkipped=%lu ReadErrors=%lu\n",
      microphoneEnded ? "STOPPED BY TOUCH INTERRUPT" : "STOP REQUESTED: I2S.end failed",
      static_cast<unsigned long>(samplesRead), static_cast<unsigned long>(samplesPrinted),
      static_cast<unsigned long>(samplesNotPrinted), static_cast<unsigned long>(audioReadErrors));
  if (length > 0 && sendTextIfReady(line, length)) stopMessagePending = false;
}
