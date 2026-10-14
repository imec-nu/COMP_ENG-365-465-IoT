// Task 3: record 16 kHz mono audio in consecutive five-second TXT files.
// Complete TODO 3.1 through 3.3. Keep audio_logger_support.h in this folder.
#include "audio_logger_support.h"

void ProducerTask(void *) {
  // TODO 3.1: Implement the producer loop.
  // Receive an int index from qFree with xQueueReceive(..., portMAX_DELAY).
  // Use bufferForIndex(index), then readFullAudioBlock(..., BLOCK_SAMPLES).
  // Send the same index to qFilled with xQueueSend(..., portMAX_DELAY).
  // Use checkQueueResult() to check each queue operation.
  // Repeat forever. Once sent, that buffer belongs to the consumer.
  // YOUR CODE HERE
  waitForTodo("TODO 3.1");
}

bool writeBlockToFiles(const int16_t *samples, size_t count) {
  if (!samples || count == 0 || !audioFile || samplesInFile >= SAMPLES_PER_FILE) {
    return false;
  }
  // TODO 3.2: Write every sample, including a block that crosses a file boundary.
  // Keep an offset into samples. While offset < count:
  // - Find the smaller of the remaining block length and free file capacity.
  // - Call writeTextSamples(samples + offset, amount).
  // - Advance offset and samplesInFile by amount.
  // - At exactly SAMPLES_PER_FILE, finishCurrentFile(), then openNextFile().
  // Return true after the whole block is written.
  // openNextFile() resets samplesInFile to zero.
  // YOUR CODE HERE
  waitForTodo("TODO 3.2");
}

void ConsumerTask(void *) {
  // TODO 3.3: Implement the consumer loop.
  // Open the first file with openNextFile().
  // Receive an int index from qFilled and obtain bufferForIndex(index).
  // Call writeBlockToFiles(..., BLOCK_SAMPLES); haltRecording() if it is false.
  // Only after writing, send the index back to qFree. Repeat forever.
  // Use portMAX_DELAY and checkQueueResult() for both queue operations.
  // YOUR CODE HERE
  waitForTodo("TODO 3.3");
}

