#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>

struct Location {
  double latitude = NAN;
  double longitude = NAN;
  String city;
};

struct Weather {
  float temperatureC = NAN;
  String condition;
  String place;
};

inline bool parseLocation(const String& payload, Location& output) {
  JsonDocument doc;
  if (deserializeJson(doc, payload.c_str())) return false;
  // TODO 3.2: Read and check the approximate IP location.
  // Require doc["status"] to be "success" and doc["lat"], doc["lon"] to be
  // numbers. Use is<double>() to check each number before reading it with
  // as<double>(). A missing coordinate is an error; zero is valid.
  // Fill a local Location variable: latitude from doc["lat"], longitude from
  // doc["lon"], and city from doc["city"] (default "" if absent).
  // Use isfinite() to check both coordinates. Latitude must be within
  // [-90, 90] and longitude within [-180, 180]. After all checks pass, copy
  // the result to output and return true. On failure, return false and leave
  // output unchanged.
  return false;
}

inline bool parseWeather(const String& payload, Weather& output) {
  JsonDocument doc;
  if (deserializeJson(doc, payload.c_str())) return false;
  // TODO 3.3: Read and check the current weather from WeatherAPI.
  // Reject a response containing an "error" object. In a local Weather variable,
  // read numeric doc["current"]["temp_c"] into temperatureC and string
  // doc["current"]["condition"]["text"] into condition. Read the optional
  // doc["location"]["name"] into place, using "" if absent.
  // Use is<float>() and is<const char*>() to check types before as<T>().
  // Require a finite temperature and a nonempty condition after trim().
  // Zero C is valid. Return false for missing or invalid required fields and
  // leave output unchanged. After all checks pass, copy the local result to
  // output and return true.
  return false;
}
