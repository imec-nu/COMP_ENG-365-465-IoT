// Lab 4 Task 2. Complete TODO 2.1 here and 2.2/2.3 in tools/udp_receiver.py.
// Set the Wi-Fi credentials and computer's LAN IPv4 address in config.h.
// udp_support.h provides the Wi-Fi connection, socket, and send schedule.
#include "udp_support.h"

bool sendDatagram(uint32_t number) {
  // TODO 2.1: format "L4 <session> <sequence>" as ASCII text.
  // Write sessionId as eight lowercase hexadecimal digits and number as
  // decimal. Use snprintf with "L4 %08lx %lu" and cast both to unsigned long.
  // Send only the text bytes, without a newline or terminating NUL.
  // Call udp.beginPacket(destination, COMPUTER_PORT), udp.write(...), and
  // udp.endPacket(). Return true only if beginPacket and endPacket succeed
  // and write accepts every byte. Check reception in the Python receiver.
  (void)number;
  return false;  // Replace this placeholder.
}

