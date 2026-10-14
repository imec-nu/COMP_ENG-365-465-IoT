#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// XIAO ESP32S3 Sense SD slot. This sketch only reads the card.
const int SD_CS_PIN = 21;
const size_t CHUNK_BYTES = 1024;
const size_t MAX_COMMAND_BYTES = 1024;
const size_t MAX_PATH_BYTES = 240;
const uint8_t MAX_DIRECTORY_DEPTH = 8;
const uint32_t ACK_TIMEOUT_MS = 10000;
const uint32_t WRITE_TIMEOUT_MS = 10000;

bool sdMounted = false;
uint8_t chunk[CHUNK_BYTES];
char command[MAX_COMMAND_BYTES + 1];
size_t commandLength = 0;
bool discardCommand = false;

enum LineStatus { LINE_PENDING, LINE_READY, LINE_TOO_LONG, LINE_BAD_CHARACTER };
bool badCommandCharacter = false;

// Never wait indefinitely if the receiving program closes its serial port.
bool writeBytes(const uint8_t *bytes, size_t count) {
  size_t sent = 0;
  uint32_t lastProgressMs = millis();
  while (sent < count) {
    const int room = Serial.availableForWrite();
    if (room > 0) {
      const size_t amount = min(count - sent, (size_t)room);
      const size_t written = Serial.write(bytes + sent, amount);
      if (written > 0) {
        sent += written;
        lastProgressMs = millis();
      }
    }
    if (millis() - lastProgressMs >= WRITE_TIMEOUT_MS) return false;
    if (sent < count) delay(1);
  }
  return true;
}

bool writeLine(const char *text) {
  return writeBytes((const uint8_t *)text, strlen(text)) &&
         writeBytes((const uint8_t *)"\n", 1);
}

bool sendError(const char *code) {
  char line[80];
  snprintf(line, sizeof(line), "SDX1 ERR %s", code);
  return writeLine(line);
}

void resetCommandReader() {
  commandLength = 0;
  discardCommand = false;
  badCommandCharacter = false;
}

// A malformed or overlong line is discarded through its terminating LF.
LineStatus pollCommand() {
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) break;
    if (value == '\n') {
      command[commandLength] = '\0';
      const LineStatus status = discardCommand ? LINE_TOO_LONG :
          (badCommandCharacter ? LINE_BAD_CHARACTER : LINE_READY);
      resetCommandReader();
      return status;
    }
    if (value < 0x20 || value > 0x7e) badCommandCharacter = true;
    if (!discardCommand) {
      if (commandLength == MAX_COMMAND_BYTES) {
        discardCommand = true;
      } else {
        command[commandLength++] = (char)value;
      }
    }
  }
  return LINE_PENDING;
}

uint32_t updateCrc32(uint32_t crc, const uint8_t *bytes, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    }
  }
  return crc;
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool validUtf8(const uint8_t *bytes, size_t count) {
  size_t i = 0;
  while (i < count) {
    const uint8_t first = bytes[i++];
    if (first < 0x80) continue;
    uint32_t codepoint;
    uint32_t minimum;
    uint8_t following;
    if (first >= 0xc2 && first <= 0xdf) {
      codepoint = first & 0x1f; minimum = 0x80; following = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
      codepoint = first & 0x0f; minimum = 0x800; following = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
      codepoint = first & 0x07; minimum = 0x10000; following = 3;
    } else {
      return false;
    }
    if (i + following > count) return false;
    while (following-- > 0) {
      const uint8_t next = bytes[i++];
      if ((next & 0xc0) != 0x80) return false;
      codepoint = (codepoint << 6) | (next & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
  }
  return true;
}

bool validPath(const char *path, size_t length) {
  if (length < 2 || length > MAX_PATH_BYTES || path[0] != '/') return false;
  if (!validUtf8((const uint8_t *)path, length)) return false;
  size_t segmentStart = 1;
  for (size_t i = 1; i <= length; ++i) {
    const uint8_t c = (uint8_t)path[i];
    if (i < length && (c < 0x20 || c == 0x7f || c == '\\')) return false;
    if (i == length || c == '/') {
      const size_t segmentLength = i - segmentStart;
      if (segmentLength == 0 ||
          (segmentLength == 1 && path[segmentStart] == '.') ||
          (segmentLength == 2 && path[segmentStart] == '.' &&
           path[segmentStart + 1] == '.')) return false;
      segmentStart = i + 1;
    }
  }
  return true;
}

bool decodePath(const char *hex, char *path) {
  const size_t length = strlen(hex);
  if (length == 0 || length % 2 != 0 || length / 2 > MAX_PATH_BYTES) return false;
  for (size_t i = 0; i < length / 2; ++i) {
    const int high = hexDigit(hex[2 * i]);
    const int low = hexDigit(hex[2 * i + 1]);
    if (high < 0 || low < 0) return false;
    path[i] = (char)((high << 4) | low);
  }
  path[length / 2] = '\0';
  return validPath(path, length / 2);
}

void encodePath(const char *path, char *hex) {
  const char digits[] = "0123456789abcdef";
  const size_t length = strlen(path);
  for (size_t i = 0; i < length; ++i) {
    const uint8_t c = (uint8_t)path[i];
    hex[2 * i] = digits[c >> 4];
    hex[2 * i + 1] = digits[c & 0x0f];
  }
  hex[2 * length] = '\0';
}

bool ensureSdMounted() {
  if (sdMounted && SD.cardType() != CARD_NONE) return true;
  SD.end();
  // Recursion can hold a parent directory open at each level.
  sdMounted = SD.begin(SD_CS_PIN, SPI, 4000000, "/sd", 16, false) &&
              SD.cardType() != CARD_NONE;
  return sdMounted;
}

// An error aborts the listing; END is sent only for a complete listing.
bool listDirectory(const char *sdPath, uint8_t depth) {
  char mountedPath[MAX_PATH_BYTES + 4];
  snprintf(mountedPath, sizeof(mountedPath), "/sd%s", sdPath);
  DIR *directory = opendir(mountedPath);
  if (directory == nullptr) {
    sdMounted = false;
    sendError("DIRECTORY_OPEN_FAILED");
    return false;
  }
  while (true) {
    errno = 0;
    const struct dirent *entry = readdir(directory);
    if (entry == nullptr) {
      const int directoryError = errno;
      closedir(directory);
      if (directoryError != 0) {
        sdMounted = false;
        sendError("DIRECTORY_READ_FAILED");
        return false;
      }
      return true;
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    char path[MAX_PATH_BYTES + 1];
    const int length = snprintf(path, sizeof(path), "%s%s%s", sdPath,
                                strcmp(sdPath, "/") == 0 ? "" : "/", entry->d_name);
    if (length < 0 || (size_t)length >= sizeof(path)) {
      closedir(directory);
      sendError("PATH_TOO_LONG");
      return false;
    }
    if (!validPath(path, (size_t)length)) {
      closedir(directory);
      sendError("UNSUPPORTED_PATH");
      return false;
    }
    snprintf(mountedPath, sizeof(mountedPath), "/sd%s", path);
    struct stat info;
    if (stat(mountedPath, &info) != 0) {
      closedir(directory);
      sdMounted = false;
      sendError("FILE_STAT_FAILED");
      return false;
    }
    if (S_ISDIR(info.st_mode)) {
      if (depth >= MAX_DIRECTORY_DEPTH) {
        closedir(directory);
        sendError("DIRECTORY_TOO_DEEP");
        return false;
      }
      if (!listDirectory(path, depth + 1)) {
        closedir(directory);
        return false;
      }
    } else if (S_ISREG(info.st_mode)) {
      // Shared only while a file line is emitted, never across recursion.
      static char hex[2 * MAX_PATH_BYTES + 1];
      static char line[2 * MAX_PATH_BYTES + 48];
      encodePath(path, hex);
      snprintf(line, sizeof(line), "SDX1 FILE %lu %s",
               (unsigned long)info.st_size, hex);
      if (!writeLine(line)) {
        closedir(directory);
        return false;
      }
    }
  }
}

void listFiles() {
  if (listDirectory("/", 0)) writeLine("SDX1 END");
}

const char *waitForAck() {
  const uint32_t startedMs = millis();
  while (millis() - startedMs < ACK_TIMEOUT_MS) {
    const LineStatus status = pollCommand();
    if (status == LINE_TOO_LONG) return "COMMAND_TOO_LONG";
    if (status == LINE_BAD_CHARACTER) return "BAD_COMMAND";
    if (status == LINE_READY) {
      if (strcmp(command, "SDX1 ACK") == 0) return nullptr;
      if (strcmp(command, "SDX1 CANCEL") == 0) return "CANCELLED";
      return "EXPECTED_ACK";
    }
    delay(1);
  }
  resetCommandReader();
  return "ACK_TIMEOUT";
}

void sendFile(const char *hexPath) {
  char path[MAX_PATH_BYTES + 1];
  if (!decodePath(hexPath, path)) {
    sendError("BAD_PATH");
    return;
  }
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    file.close();
    sendError("FILE_OPEN_FAILED");
    return;
  }
  const size_t total = file.size();
  size_t acknowledged = 0;
  uint32_t wholeCrc = 0xffffffffUL;
  char line[80];
  snprintf(line, sizeof(line), "SDX1 DATA %lu", (unsigned long)total);
  if (!writeLine(line)) {
    file.close();
    return;
  }
  while (acknowledged < total) {
    const size_t wanted = min(CHUNK_BYTES, total - acknowledged);
    size_t received = 0;
    while (received < wanted) {
      const size_t amount = file.read(chunk + received, wanted - received);
      if (amount == 0 || amount > wanted - received) {
        file.close();
        sdMounted = false;
        sendError("SD_READ_FAILED");
        return;
      }
      received += amount;
    }
    const uint32_t chunkCrc = updateCrc32(0xffffffffUL, chunk, received) ^ 0xffffffffUL;
    snprintf(line, sizeof(line), "SDX1 CHUNK %lu %08lx",
             (unsigned long)received, (unsigned long)chunkCrc);
    if (!writeLine(line) || !writeBytes(chunk, received)) {
      file.close();
      return;
    }
    const char *error = waitForAck();
    if (error != nullptr) {
      file.close();
      sendError(error);
      return;
    }
    wholeCrc = updateCrc32(wholeCrc, chunk, received);
    acknowledged += received;
  }
  file.close();
  snprintf(line, sizeof(line), "SDX1 DONE %lu %08lx",
           (unsigned long)acknowledged, (unsigned long)(wholeCrc ^ 0xffffffffUL));
  writeLine(line);
}

void handleCommand() {
  if (strcmp(command, "SDX1 CANCEL") == 0) {
    sendError("CANCELLED");
    return;
  }
  const bool ping = strcmp(command, "SDX1 PING") == 0;
  const bool list = strcmp(command, "SDX1 LIST") == 0;
  const bool get = strncmp(command, "SDX1 GET ", 9) == 0;
  if (!ping && !list && !get) {
    sendError("BAD_COMMAND");
    return;
  }
  if (!ensureSdMounted()) {
    sendError("SD_UNAVAILABLE");
    return;
  }
  if (ping) writeLine("SDX1 READY 1");
  else if (list) listFiles();
  else sendFile(command + 9);
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  SPI.begin(D8, D9, D10, SD_CS_PIN);
  ensureSdMounted();
}

void loop() {
  const LineStatus status = pollCommand();
  if (status == LINE_READY) handleCommand();
  else if (status == LINE_TOO_LONG) sendError("COMMAND_TOO_LONG");
  else if (status == LINE_BAD_CHARACTER) sendError("BAD_COMMAND");
  delay(1);
}
