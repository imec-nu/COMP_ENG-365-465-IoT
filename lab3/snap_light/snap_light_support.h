#pragma once
// Supplied microphone setup and diagnostics. Keep this tab with the sketch.
I2SClass I2S;

void setLed(bool on) {
  digitalWrite(LED_PIN, on ? LOW : HIGH);  // XIAO LED is active low.
}

void beginHardware() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  I2S.setPinsPdmRx(42, 41);  // Sense expansion board: clock, microphone data.
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    const char message[] = "Microphone initialization failed\n";
    while (true) {
      if (Serial && Serial.availableForWrite() >= static_cast<int>(sizeof(message) - 1)) {
        Serial.write(reinterpret_cast<const uint8_t *>(message), sizeof(message) - 1);
      }
      delay(1000);
    }
  }
}

size_t readAudioFrame(int16_t *samples, size_t capacity) {
  size_t bytesRead = 0;
  // The native read reports partial data separately from timeout status.
  esp_err_t result = i2s_channel_read(I2S.rxChan(), samples,
      capacity * sizeof(int16_t), &bytesRead, 50);
  if (result != ESP_OK && result != ESP_ERR_TIMEOUT) return 0;
  return bytesRead / sizeof(int16_t);
}

void reportRms(float rms) {
  // Offer every frame so a short snap is visible during calibration.
  char line[48];
  const int length = snprintf(line, sizeof(line), "RMS: %.1f\n", rms);
  if (length > 0 && Serial && Serial.availableForWrite() >= length) {
    Serial.write(reinterpret_cast<const uint8_t *>(line), length);
  }
}
