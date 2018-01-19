#include <Adafruit_NeoPixel.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "config.h"
#include "mathLib.h"

#ifndef STATE_H_
#define STATE_H_

struct BrightnessFlux {
  boolean active;
  uint8_t start; // The "from" brightness.
  uint8_t end; // The  "to" brightness. These switch every half-period
  uint32_t duration; // Duration from start to end.
  uint32_t currentTime;
  Easing easing;
};

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t o;

  Color() {
    r = 0;
    g = 0;
    b = 0;
    o = 0;
  }

  Color(uint8_t red, uint8_t green, uint8_t blue) {
    r = red;
    g = green;
    b = blue;
    o = 0;
  }
  
  Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t opacity) {
    r = red;
    g = green;
    b = blue;
    o = opacity;
  }

  void setColor(Color other) {
    r = other.r;
    g = other.g;
    b = other.b;
  }

  void setColor(uint8_t red, uint8_t green, uint8_t blue) {
    r = red;
    g = green;
    b = blue;
  }

  uint32_t getConverted() {
    return ((((uint32_t)r * (o + 1) << 8) & 0xFF0000) |
            (((uint32_t)g * (o + 1)) & 0xFF00) |
            (((uint32_t)b * (o + 1) >> 8) & 0xFF));
  }
};

struct PingPong {
  boolean active;
  uint8_t pixel; // The "from" brightness.
  uint8_t spread; // How far from the center pixel to light up
  uint32_t duration;
  uint32_t currentTime;
  boolean directionUp;
  uint8_t *falloff;
  boolean dark;
  Easing easing;
};

struct Rainbow {
  boolean active;
  uint8_t repeat; // how many times the rainbow repeats on a side
  uint32_t duration; // how long for the rainbow to repeat.
  uint32_t currentTime;
};

struct SaveState {
  BrightnessFlux brightFlux;
  PingPong pingpong;
  Rainbow rainbow;
};

struct State {
  Adafruit_BluefruitLE_SPI ble = Adafruit_BluefruitLE_SPI(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

  boolean rfState[NUM_STRIPS] = {false, false, false, false };
  boolean bleState[NUM_STRIPS] = {false, false, false, false };
  boolean stripDirty[NUM_STRIPS] = {false, false, false, false };
  boolean buttonStateChanged[NUM_STRIPS] = {false, false, false, false };

  Adafruit_NeoPixel strips[NUM_STRIPS] = {
    Adafruit_NeoPixel(LEFT_POST_LEDS, LEFT_POST_PIN, NEO_GRBW + NEO_KHZ800),
    Adafruit_NeoPixel(LEFT_RAIL_LEDS, LEFT_RAIL_PIN, NEO_GRBW + NEO_KHZ800),
    Adafruit_NeoPixel(RIGHT_POST_LEDS, RIGHT_POST_PIN, NEO_GRBW + NEO_KHZ800),
    Adafruit_NeoPixel(RIGHT_RAIL_LEDS, RIGHT_RAIL_PIN, NEO_GRBW + NEO_KHZ800),
  };
  
  Color *stripColors[NUM_STRIPS];
  
  unsigned long lastMicros;
  uint16_t deltaMicros = 0;
  unsigned long lostMicros = 0;
  unsigned long numCalls = 0;
  unsigned long fpsMicros = 0;
  
  BrightnessFlux brightFlux;
  PingPong pingpong;
  Rainbow rainbow;

  SaveState saveState[5];
};

#endif
