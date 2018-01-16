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

struct PingPong {
  boolean active;
  uint8_t pixel; // The "from" brightness.
  uint8_t spread; // How far from the center pixel to light up
  uint32_t duration;
  uint32_t currentTime;
  boolean directionUp;
  uint32_t *colorFalloff;
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
  uint32_t color;
  uint8_t brightness = 150;
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
  
  uint32_t *stripColors[NUM_STRIPS];
  
  unsigned long lastMicros;
  uint16_t deltaMicros = 0;
  unsigned long lostMicros = 0;
  unsigned long numCalls = 0;
  unsigned long fpsMicros = 0;

  uint32_t color; // Last assigned color from the controller.
  
  uint8_t brightness = 150;
  
  BrightnessFlux brightFlux;
  PingPong pingpong;
  Rainbow rainbow;

  SaveState saveState[5];
};

#endif
