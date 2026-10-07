// Task 3: share one I2C bus between an MPU-6500 and an SSD1306 OLED.
// Board: XIAO ESP32S3. Serial Monitor: 115200 baud.
// Install the U8g2 library. Supplied hardware/display code is in the .h tab.
// Display all six axes. Tilt sets the scrolling speed; text wraps at the edges.
//
// Connect the hardware before completing TODO 3.1 through 3.4:
// Disconnect power and remove the IMU's Task 2 SPI connections first.
// OLED:     VCC -> 3V3, GND -> GND, SDA -> D4, SCL -> D5.
// MPU-6500: VCC -> 3V3, GND -> GND, SDA/SDI -> D4, SCL/SCLK -> D5.
//           NCS/CS -> 3V3, SDO/AD0 -> GND. Leave INT unconnected.
// Join both SDA wires to D4 (GPIO5) and both SCL wires to D5 (GPIO6).
// Both devices share power and ground. D5 is the SCL pin, not D6.
// Reconnect power after rewiring. This sketch uses I2C only; no bus switch
// or touch inputs are needed. Addresses: IMU 0x68, OLED 0x3C.
#include "tilt_display_support.h"

// Each frame reads the IMU once, updates motion, and sends the OLED image.
// I2C transfer time sets the actual frame rate; 33 ms is the minimum interval.
const uint32_t FRAME_INTERVAL_MS = 33;

const float DEGREES_PER_RADIAN = 180.0f / PI;
const float FILTER_ALPHA = 0.2f;
const float FILTER_REFERENCE_SECONDS = 0.1f;
const float DEADZONE_DEG = 2.0f;
const float PX_PER_DEG_PER_SEC = 0.8f;
// Check motion with your actual mounting. Flip the relevant sign if needed.
const float X_SIGN = 1.0f;
const float Y_SIGN = -1.0f;

Sample latest;
float pitchFiltered = 0, rollFiltered = 0;
bool filterReady = false;
float velX = 0, velY = 0;        // pixels/second
float scrollX = 0, scrollY = 0;  // pixels
uint32_t lastRenderMs = 0;

RawSample decodeSample(const uint8_t bytes[14]) {
  RawSample raw;
  // TODO 3.1: Assign raw.ax/ay/az and raw.gx/gy/gz using combineBytes().
  // Each signed value is high byte first. Byte pairs:
  // ax: 0,1; ay: 2,3; az: 4,5; temperature: 6,7 (skip);
  // gx: 8,9; gy: 10,11; gz: 12,13.
  // YOUR CODE HERE
  return raw;
}

void sampleAndUpdateVelocity(float dt) {
  // Supplied: one coherent 14-byte burst, from ACCEL_XOUT_H through GYRO_ZOUT_L.
  uint8_t bytes[14];
  if (!readRegisters(0x3B, bytes, sizeof(bytes))) {
    velX = 0;
    velY = 0;
    filterReady = false;
    cacheText(latest, "READ ERROR: data held");
    return;
  }

  latest = scaleSample(decodeSample(bytes));
  float ax = latest.ax;
  float ay = latest.ay;
  float az = latest.az;

  // TODO 3.2: Replace the zeros with the pitch and roll formulas in the handout.
  // Use atan2f(), sqrtf(), ax/ay/az, and DEGREES_PER_RADIAN.
  float pitch = 0;  // YOUR CODE HERE
  float roll = 0;   // YOUR CODE HERE

  // Supplied: smooth the angles using actual elapsed time.
  if (!filterReady) {
    pitchFiltered = pitch;
    rollFiltered = roll;
    filterReady = true;
  } else {
    const float weight = 1.0f - powf(1.0f - FILTER_ALPHA,
                                    dt / FILTER_REFERENCE_SECONDS);
    pitchFiltered = weight * pitch + (1 - weight) * pitchFiltered;
    rollFiltered = weight * roll + (1 - weight) * rollFiltered;
  }

  velX = 0;
  velY = 0;
  // TODO 3.3: Set horizontal velocity from rollFiltered and vertical velocity
  // from pitchFiltered. Keep an axis stopped when |angle| <= DEADZONE_DEG.
  // Use fabsf() to check the deadzone, PX_PER_DEG_PER_SEC as the gain,
  // and X_SIGN/Y_SIGN to select the direction.
  // YOUR CODE HERE

  // Supplied: show the sample read for this frame.
  cacheText(latest, "Tilt to scroll");
}

void setup() {
  Serial.begin(115200);

  initializeHardware();  // Stops and reports if either device fails to initialize.
  cacheText(latest, "Waiting for sample");
  renderWrapped(0, 0);
  lastRenderMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastRenderMs < FRAME_INTERVAL_MS) {
    delay(1);
    return;
  }
  const float dt = (now - lastRenderMs) * 0.001f;
  lastRenderMs = now;
  sampleAndUpdateVelocity(dt);

  // TODO 3.4: Advance each scroll position using its velocity and dt.
  // Position change = velocity (pixels/second) * elapsed time (seconds).
  // YOUR CODE HERE

  // Supplied: wrap and display this frame before reading the next sample.
  scrollX = wrapPosition(scrollX, SCREEN_W);
  scrollY = wrapPosition(scrollY, SCREEN_H);
  renderWrapped(scrollX, scrollY);
  delay(1);
}
