// Lab 5 Task 1: control the board's LED from a browser.
#include "led_support.h"

bool startAccessPoint() {
  // TODO 1.1: Create the Wi-Fi network that your phone will join.
  // Select WIFI_AP with WiFi.mode(), then call WiFi.softAP() with
  // AP_SSID and AP_PASSWORD. Return whether softAP() succeeded.
  // YOUR CODE HERE
  return false;
}

void registerLedRoutes() {
  // TODO 1.2: Connect the two webpage buttons to their request handlers.
  // Use server.on(path, HTTP_GET, handler) to register "/on" with
  // handleLEDOn and "/off" with handleLEDOff. Pass the function names
  // without parentheses; the server calls them when a request arrives.
  // YOUR CODE HERE
}

void handleLEDCommand(bool turnOn) {
  // TODO 1.3: Apply the command, then send the browser back to the page.
  // Write LOW to LED_PIN when turnOn is true, or HIGH when it is false.
  // Store turnOn in ledState so the webpage shows the same state.
  // Set the response header "Location" to "/" with server.sendHeader(),
  // then call server.send(303). This tells the browser to reload the page.
  // YOUR CODE HERE
  server.send(501, "text/plain", "Complete TODO 1.3, then upload again.");
}
