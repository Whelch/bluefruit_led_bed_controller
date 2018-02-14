#include <stdint.h>
#include <math.h>
#include "mathLib.h"

#define EASE_EXP_CONST 1.414213562373
#define EASE_QUARTIC_CONST 1.681792830507

long lerp(double percent, long start, long end) {
  return start + (long)((end-start)*percent);
}

uint8_t distance(uint8_t first, uint8_t second) {
  if (first > second) return first - second;
  else return second - first;
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
    case quartic: return easing_quartic(input);
  }
}
