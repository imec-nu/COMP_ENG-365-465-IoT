// Lab 4 Tasks 3 and 4. Edit this sketch and weather_logic.h.
// Fill in config.h locally. Keep ENABLE_NOTIFICATIONS=false during Task 3.
// Wi-Fi/TLS/OLED/timing support is in weather_station_support.h.
#include "weather_station_support.h"

bool readGET(HTTPClient& http, String& payload) {
  // TODO 3.1: Read the body of a successful GET response.
  // Call http.GET() and print the returned status. Negative values indicate
  // transport errors. Return false unless the status is HTTP_CODE_OK (200).
  // Read the body into payload with http.getString(). Return true if the body
  // is nonempty, otherwise return false. The caller handles http.end().
  // Keep the API key out of Serial output.
  return false;
}

bool publishNtfy(const String& body) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  if (!http.begin(client, String("https://ntfy.sh/") + NTFY_TOPIC)) return false;
  prepareHTTP(http);
  bool accepted = false;
  // TODO 4.1: Publish body as a text message using HTTP POST.
  // Use http.addHeader(name, value) to set Content-Type to
  // "text/plain; charset=utf-8" and Title to a short ASCII title.
  // Call http.POST(body) and print the returned status. Set accepted to true
  // for status codes 200 through 299 and leave it false for all other results.
  http.end();
  return accepted;
}

bool refreshWeather() {
  // TODO 3.4: Request weather for the coordinates from the location lookup.
  // If haveLocation is false, pass a local Location variable to fetchLocation().
  // Return false if the lookup fails. Otherwise copy the result into location
  // and set haveLocation to true. Reuse this location on later refreshes.
  // Build https://api.weatherapi.com/v1/current.json with query parameters
  // key=WEATHER_API_KEY, q=<latitude>,<longitude>, and aqi=no. String(value, 6)
  // formats each coordinate from location.
  // Pass the URL and a separate local Weather variable to fetchWeather().
  // On failure, return false and leave lastWeather unchanged. On success,
  // copy the weather result into lastWeather, set haveWeather to true, and
  // call showWeather(). Print the coordinates, temperature, and condition
  // in Serial Monitor, then return true.
  return false;
}

void notifyFreshWeather() {
  // TODO 4.2: Publish when the message differs from the last successful post.
  // loop() calls this only after a fresh weather fetch with notifications enabled.
  // Build a String body with lastWeather.temperatureC, lastWeather.condition,
  // location.latitude, and location.longitude. Label the coordinates as
  // approximate. For the place name, use lastWeather.place; if it is empty,
  // use location.city. String(number, decimals) formats values for the message.
  // If body matches lastPublishedBody, return. Otherwise call publishNtfy(body)
  // and print whether it succeeded. Update lastPublishedBody only if the call
  // returns true. Use loop()'s 120-second schedule for later attempts.
}
