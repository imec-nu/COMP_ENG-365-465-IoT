// Checkpoint 1: serial output.
// Open Serial Monitor at 115200 baud. Send r to repeat the output.
#include <Arduino.h>

void printSequence() {
  for (int number = 1; number <= 10; number++) {
    Serial.println(number);
    delay(500);
  }
  Serial.println("complete");
}

void setup() {
  Serial.begin(115200);

  // Wait up to 3 seconds for Serial Monitor to connect.
  unsigned long startTime = millis();
  while (!Serial && millis() - startTime < 3000) {
    delay(10);
  }

  printSequence();
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'r' || command == 'R') {
      printSequence();
    }
  }
}
