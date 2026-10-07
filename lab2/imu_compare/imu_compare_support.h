#pragma once

const int IMU_CS = D7;    // GPIO44
const int IMU_SCK = D8;   // GPIO7
const int IMU_MISO = D9;  // GPIO8
const int IMU_MOSI = D10; // GPIO9
const int I2C_SDA = D4;   // GPIO5
const int I2C_SCL = D5;   // GPIO6
const uint8_t IMU_ADDR = 0x68;

const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_CONFIG2 = 0x1D;
const uint8_t REG_INT_PIN_CFG = 0x37;
const uint8_t REG_INT_ENABLE = 0x38;
const uint8_t REG_INT_STATUS = 0x3A;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_USER_CTRL = 0x6A;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_PWR_MGMT_2 = 0x6C;
const uint8_t REG_WHO_AM_I = 0x75;

struct RawSample {
  int16_t ax, ay, az, gx, gy, gz;
};

uint8_t spiReadAddress(uint8_t reg);
bool selectI2cRegister(uint8_t reg);
uint8_t sampleDivider(uint16_t rateHz);
void decodeFrame(const uint8_t *raw, RawSample &sample);
float rateFromCount(uint32_t count, uint32_t elapsedMs);

SPISettings imuSpi(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3);
RawSample latest = {};
bool haveSample = false;
uint32_t windowStartMs = 0;
uint32_t completedReads = 0;
uint32_t ioErrors = 0;
uint32_t statusPolls = 0;
uint64_t totalReadUs = 0;
uint32_t maxReadUs = 0;
float configuredRateHz = 0.0f;

int16_t combineBytes(uint8_t hi, uint8_t lo) {
  const uint16_t bits = ((uint16_t)hi << 8) | lo;
  const int32_t value = bits >= 0x8000 ? (int32_t)bits - 65536 : bits;
  return (int16_t)value;
}

bool readRegisters(uint8_t reg, uint8_t *data, size_t length) {
  if (USE_SPI) {
    SPI.beginTransaction(imuSpi);
    digitalWrite(IMU_CS, LOW);
    SPI.transfer(spiReadAddress(reg));
    for (size_t i = 0; i < length; ++i) data[i] = SPI.transfer(0x00);
    digitalWrite(IMU_CS, HIGH);
    SPI.endTransaction();
    return true; // SPI has no acknowledgement or frame CRC here.
  }

  if (!selectI2cRegister(reg)) return false;
  const size_t received = Wire.requestFrom(IMU_ADDR, length, true);
  if (received != length) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < length; ++i) data[i] = (uint8_t)Wire.read();
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  if (USE_SPI) {
    SPI.beginTransaction(imuSpi);
    digitalWrite(IMU_CS, LOW);
    SPI.transfer(reg & 0x7F);
    SPI.transfer(value);
    digitalWrite(IMU_CS, HIGH);
    SPI.endTransaction();
    return true;
  }

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void stopWithMessage(const char *message) {
  while (true) {
    Serial.println(message);
    delay(1000);
  }
}

bool configureImu() {
  if (!writeRegister(REG_PWR_MGMT_1, 0x80)) return false;
  delay(100);
  if (!writeRegister(REG_PWR_MGMT_1, 0x01)) return false;
  if (!writeRegister(REG_PWR_MGMT_2, 0x00)) return false;
  if (!writeRegister(REG_USER_CTRL, USE_SPI ? 0x10 : 0x00)) return false;
  if (!writeRegister(REG_GYRO_CONFIG, 0x00)) return false; // +/-250 deg/s
  if (!writeRegister(REG_ACCEL_CONFIG, 0x00)) return false; // +/-2 g
  if (!writeRegister(REG_CONFIG, 0x03)) return false;
  if (!writeRegister(REG_ACCEL_CONFIG2, 0x03)) return false;
  if (!writeRegister(REG_SMPLRT_DIV, sampleDivider(TARGET_HZ))) return false;
  if (!writeRegister(REG_INT_PIN_CFG, 0x00)) return false;
  if (!writeRegister(REG_INT_ENABLE, 0x01)) return false;
  delay(100);

  const uint8_t regs[] = {REG_PWR_MGMT_1, REG_PWR_MGMT_2, REG_USER_CTRL,
                         REG_GYRO_CONFIG, REG_ACCEL_CONFIG, REG_CONFIG,
                         REG_ACCEL_CONFIG2, REG_SMPLRT_DIV, REG_INT_PIN_CFG,
                         REG_INT_ENABLE};
  const uint8_t expected[] = {0x01, 0x00, (uint8_t)(USE_SPI ? 0x10 : 0x00),
                             0x00, 0x00, 0x03, 0x03, sampleDivider(TARGET_HZ),
                             0x00, 0x01};
  for (size_t i = 0; i < sizeof(regs); ++i) {
    uint8_t actual = 0;
    if (!readRegisters(regs[i], &actual, 1) || actual != expected[i]) {
      Serial.printf("Config check failed at register 0x%02X.\n", regs[i]);
      return false;
    }
  }
  return true;
}

void reportWindow() {
  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = nowMs - windowStartMs;
  if (elapsedMs < 2000) return;

  const float readHz = rateFromCount(completedReads, elapsedMs);
  const double avgUs = completedReads ? (double)totalReadUs / completedReads : 0.0;
  Serial.printf("[%s %s] target=%u Hz configured=%.1f Hz reads/s=%.1f avg_us=%.1f max_us=%lu io_errors=%lu polls=%lu\n",
                USE_SPI ? "SPI" : "I2C", WAIT_FOR_DATA_READY ? "ready" : "continuous",
                TARGET_HZ, configuredRateHz, readHz, avgUs, (unsigned long)maxReadUs,
                (unsigned long)ioErrors, (unsigned long)statusPolls);
  completedReads = ioErrors = statusPolls = maxReadUs = 0;
  totalReadUs = 0;
  windowStartMs = nowMs;
}

void setup() {
  Serial.begin(115200);
  const uint32_t startMs = millis();
  while (!Serial && millis() - startMs < 1500) delay(10);

  if (TARGET_HZ != 100 && TARGET_HZ != 200 &&
      TARGET_HZ != 500 && TARGET_HZ != 1000) {
    stopWithMessage("Use TARGET_HZ = 100, 200, 500, or 1000.");
  }
  if (SPI_CLOCK_HZ > 1000000 || I2C_CLOCK_HZ > 400000) {
    stopWithMessage("Use SPI <= 1 MHz and I2C <= 400 kHz.");
  }

  if (USE_SPI) {
    pinMode(IMU_CS, OUTPUT);
    digitalWrite(IMU_CS, HIGH);
    SPI.begin(IMU_SCK, IMU_MISO, IMU_MOSI, IMU_CS);
  } else {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(I2C_CLOCK_HZ);
    Wire.setTimeOut(20);
  }
  delay(150);

  uint8_t who = 0;
  if (!readRegisters(REG_WHO_AM_I, &who, 1)) {
    stopWithMessage("IMU read failed. Check wiring and TODO 2.1 or 2.2.");
  }
  Serial.printf("WHO_AM_I = 0x%02X (MPU-6500: 0x70)\n", who);
  if (who != 0x70) {
    stopWithMessage("Unexpected device ID. Check the read function and sensor model.");
  }
  if (!configureImu()) stopWithMessage("IMU configuration failed.");

  uint8_t divider = 0;
  if (!readRegisters(REG_SMPLRT_DIV, &divider, 1)) {
    stopWithMessage("Could not read sample divider.");
  }
  configuredRateHz = 1000.0f / (1.0f + divider);
  Serial.printf("Requested=%u Hz, configured=%.1f Hz, DIV=%u\n",
                TARGET_HZ, configuredRateHz, divider);
  Serial.printf("Bus=%s, clock=%lu Hz\n", USE_SPI ? "SPI" : "I2C",
                (unsigned long)(USE_SPI ? SPI_CLOCK_HZ : I2C_CLOCK_HZ));
  windowStartMs = millis();
}

void loop() {
  bool ready = true;
  if (WAIT_FOR_DATA_READY) {
    uint8_t status = 0;
    ++statusPolls;
    if (!readRegisters(REG_INT_STATUS, &status, 1)) {
      ++ioErrors;
      ready = false;
    } else {
      ready = (status & 0x01) != 0;
    }
  }

  if (ready) {
    uint8_t raw[14] = {};
    const uint32_t startUs = micros();
    const bool ok = readRegisters(REG_ACCEL_XOUT_H, raw, sizeof(raw));
    const uint32_t elapsedUs = micros() - startUs;
    if (ok) {
      decodeFrame(raw, latest);
      haveSample = true;
      ++completedReads;
      totalReadUs += elapsedUs;
      if (elapsedUs > maxReadUs) maxReadUs = elapsedUs;
    } else {
      ++ioErrors;
    }
  }

  static uint32_t lastPrintMs = 0;
  if (SHOW_VALUES && haveSample && millis() - lastPrintMs >= 100) {
    lastPrintMs = millis();
    Serial.printf("raw A: %d %d %d | G: %d %d %d\n",
                  latest.ax, latest.ay, latest.az, latest.gx, latest.gy, latest.gz);
  }
  reportWindow();
}
