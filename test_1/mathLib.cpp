#include <stdint.h>
#include <math.h>
#include "mathLib.h"

#define EASE_EXP_CONST 1.414213562373
#define EASE_QUARTIC_CONST 1.681792830507

long lerp(double percent, long start, long end) {
  return start + (long)((end-start)*percent);
}

uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

void printColorToBle(uint32_t color, Adafruit_BluefruitLE_SPI* ble) {
  uint8_t r = color >> 16;
  uint8_t g = color >> 8;
  uint8_t b = color;
  ble->print(F("#"));
  if (r < 0x10) ble->print(F("0")); ble->print(r, HEX);
  if (g < 0x10) ble->print(F("0")); ble->print(g, HEX);
  if (b < 0x10) ble->print(F("0")); ble->print(b, HEX);
}

/**
 * Input is a value between 0 and 1.
 * Output is a value eased between 0 and 1.
 */
double easing_cosine(double input) {
  return ((cos(input * PI) * -1) + 1) / 2;
}

double easing_exponential(double input) {
  if (input <= .5) {
    input *= EASE_EXP_CONST;
    return input * input;
  } else {
    input = (1 - input) * EASE_EXP_CONST;
    return 1 - (input * input);
  }
}

double easing_quartic(double input) {
  if (input <= .5) {
    input *= EASE_QUARTIC_CONST;
    return input * input * input * input;
  } else {
    input = (1 - input) * EASE_QUARTIC_CONST;
    return 1 - (input * input * input * input);
  }
}

double easing_linear(double input) {
  return input;
}

double ease(Easing easing, double input) {
  switch(easing) {
    case linear: return easing_linear(input);
    case cosine: return easing_cosine(input);
    case exponential: return easing_exponential(input);
  }
}
