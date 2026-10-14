#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

const uint32_t SAMPLE_RATE = 16000;
const uint32_t SECONDS_PER_FILE = 5;
const uint32_t SAMPLES_PER_FILE = SAMPLE_RATE * SECONDS_PER_FILE; // 80,000
const size_t BLOCK_SAMPLES = 1024; // 64 ms of audio
const int BUFFER_COUNT = 2;
const int PDM_CLK_PIN = 42;
const int PDM_DATA_PIN = 41;
const int SD_CS_PIN = 21;
const char *RAW_DIRECTORY = "/rawtxt";

I2SClass I2S;
alignas(4) int16_t audioBuffers[BUFFER_COUNT][BLOCK_SAMPLES];
char textBuffer[BLOCK_SAMPLES * 8];
QueueHandle_t qFree = nullptr;
QueueHandle_t qFilled = nullptr;
std::atomic<bool> recordingFailed{false};

File audioFile;
char currentPath[64];
uint32_t nextFileIndex = 0;
uint32_t samplesInFile = 0;
uint32_t fileOpenedMs = 0;

void ProducerTask(void *);
bool writeBlockToFiles(const int16_t *samples, size_t count);
void ConsumerTask(void *);

[[noreturn]] void haltRecording(const char *message) {
  if (!recordingFailed.exchange(true)) {
    Serial.printf("RECORDING HALTED: %s\n", message);
    Serial.println("The active file may be incomplete. Reset after fixing the problem.");
  }
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void requireHealthyRecording() {
  if (recordingFailed.load()) {
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

[[noreturn]] void waitForTodo(const char *todo) {
  Serial.printf("Waiting: complete %s and upload again.\n", todo);
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

int16_t *bufferForIndex(int index) {
  if (index < 0 || index >= BUFFER_COUNT) haltRecording("Invalid buffer index.");
  return audioBuffers[index];
}

void checkQueueResult(BaseType_t result) {
  if (result != pdTRUE) haltRecording("Queue operation failed.");
}

// Read bytes into int16_t storage; never interpret a byte as a whole sample.
void readFullAudioBlock(int16_t *destination, size_t count) {
  if (!destination || count == 0 || count > BLOCK_SAMPLES) {
    haltRecording("Invalid audio-read size.");
  }
  const size_t needed = count * sizeof(int16_t);
  size_t received = 0;
  while (received < needed) {
    requireHealthyRecording();
    const size_t amount = I2S.readBytes(
        reinterpret_cast<char *>(destination) + received, needed - received);
    if (amount == 0 || amount > needed - received || amount % sizeof(int16_t) != 0) {
      haltRecording("Incomplete or invalid I2S read.");
    }
    received += amount;
  }
}

// Only the consumer uses the File object and text buffer.
void openNextFile() {
  requireHealthyRecording();
  if (audioFile) haltRecording("Close the current file before opening another.");
  do {
    snprintf(currentPath, sizeof(currentPath), "%s/rec_%06lu.txt",
             RAW_DIRECTORY, (unsigned long)nextFileIndex);
    if (++nextFileIndex == 0) haltRecording("Recording filename counter overflow.");
  } while (SD.exists(currentPath));

  audioFile = SD.open(currentPath, FILE_WRITE);
  if (!audioFile) haltRecording("Could not create an SD file.");
  samplesInFile = 0;
  fileOpenedMs = millis();
}

void writeTextSamples(const int16_t *samples, size_t count) {
  requireHealthyRecording();
  if (!audioFile || !samples || count == 0) {
    haltRecording("Invalid text-write request.");
  }
  size_t offset = 0;
  while (offset < count) {
    requireHealthyRecording();
    const size_t chunk = min(count - offset, BLOCK_SAMPLES);
    size_t used = 0;
    for (size_t i = 0; i < chunk; ++i) {
      const int chars = snprintf(textBuffer + used, sizeof(textBuffer) - used,
                                 "%d\n", (int)samples[offset + i]);
      if (chars <= 0 || (size_t)chars >= sizeof(textBuffer) - used) {
        haltRecording("Text buffer overflow.");
      }
      used += (size_t)chars;
    }
    const size_t written = audioFile.write(
        reinterpret_cast<const uint8_t *>(textBuffer), used);
    if (written != used) {
      audioFile.close();
      haltRecording("SD write was incomplete; no automatic retry was attempted.");
    }
    offset += chunk;
  }
}

void finishCurrentFile() {
  requireHealthyRecording();
  if (!audioFile || samplesInFile != SAMPLES_PER_FILE) {
    haltRecording("A completed file must contain exactly 80,000 samples.");
  }
  audioFile.flush();
  audioFile.close();
  Serial.printf("SAVED %s | samples=%lu audio_s=%.3f open_ms=%lu\n",
                currentPath, (unsigned long)samplesInFile,
                (double)samplesInFile / SAMPLE_RATE,
                (unsigned long)(millis() - fileOpenedMs));
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  const uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 1500) delay(10);

  SPI.begin(D8, D9, D10, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI)) haltRecording("Could not mount the SD card.");
  if (!SD.exists(RAW_DIRECTORY) && !SD.mkdir(RAW_DIRECTORY)) {
    haltRecording("Could not create /rawtxt.");
  }

  qFree = xQueueCreate(BUFFER_COUNT, sizeof(int));
  qFilled = xQueueCreate(BUFFER_COUNT, sizeof(int));
  if (!qFree || !qFilled) haltRecording("Could not create the audio queues.");
  for (int index = 0; index < BUFFER_COUNT; ++index) {
    checkQueueResult(xQueueSend(qFree, &index, 0));
  }

  I2S.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  I2S.setTimeout(1000);
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    haltRecording("Could not initialize the microphone.");
  }

  Serial.println("16 kHz mono PCM; 80,000 signed samples per completed TXT file.");
  Serial.println("Check files announced as SAVED. The active file is not yet complete.");
  if (xTaskCreatePinnedToCore(ProducerTask, "capture", 4096, nullptr, 3,
                              nullptr, 1) != pdPASS ||
      xTaskCreatePinnedToCore(ConsumerTask, "write_sd", 4096, nullptr, 3,
                              nullptr, 0) != pdPASS) {
    haltRecording("Could not create both audio tasks.");
  }
}

void loop() {
  delay(1000); // Recording runs in the two tasks.
}
