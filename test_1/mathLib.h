#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#ifndef MATH_LIB_H_
#define MATH_LIB_H_

#define PI 3.14159265

enum Easing: uint8_t {
  linear = 1,
  cosine = 2,
  exponential = 3,
  quartic = 4
};

long lerp(double percent, long start, long end);

uint32_t Color(uint8_t r, uint8_t g, uint8_t b);

void printColorToBle(uint32_t color, Adafruit_BluefruitLE_SPI* ble);

double easing_cosine(double input);

double easing_exponential(double input);

double easing_quartic(double input);

double easing_linear(double input);

double ease(Easing easing, double input);

#endif
