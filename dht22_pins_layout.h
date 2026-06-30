// dht22_pins_layout.h
// Maps a sensor `location` string to the GPIO pin its DHT22 data line is wired to.
// ESP32 dev boards from different manufacturers break out different GPIOs, so
// each installed sensor location may use a different pin even though the
// sketch logic is identical.
//
// To add a new location: add an `if (strcmp(loc, "...") == 0) return N;` line.
// Unknown locations fall back to 14 (the original kitchen wiring).
//
// C/C++ can't switch on strings, so this is an if/strcmp ladder that resolves
// at runtime. The lookup is called once at global-construction time when the
// `DHT dht(dht22_pin_for_location(location), DHT22)` instance is built.

#pragma once

#include <Arduino.h>

inline uint8_t dht22_pin_for_location(const char* loc) {
  if (strcmp(loc, "basement")    == 0) return 4;
  if (strcmp(loc, "dining_room") == 0) return 4;
  if (strcmp(loc, "kitchen")     == 0) return 14;
  return 14;  // fallback for unmapped locations
}

