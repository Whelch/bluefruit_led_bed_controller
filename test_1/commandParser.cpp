#include "state.h"
#include "string.h"
#include "mathLib.h"

#define COMMAND_SEPARATOR " -\n:="

void processBrightnessCommand(State *state) {
  boolean failed = true;
  boolean fluxSet = false; // Used for argument ordering.
  while (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    if (token.equals("f") || token.equals("flux") || token.equals("fluxuation")) {
      uint8_t targetBrightness;
      if (token = strtok(NULL, COMMAND_SEPARATOR)) {
        Serial.println("token Found 1");
        if (uint16_t otherBrightness = token.toInt()) {
          Serial.println("otherBrightness");
          if (otherBrightness > UINT8_MAX) {
            state->ble.println(F("Invalid first parameter to the [fluxuation] argument."));
            state->ble.println(F("Expected a value in the range [0-255]."));
            break;
          } else {
            Serial.println("targetBrightness");
            targetBrightness = otherBrightness;
            if (token = strtok(NULL, COMMAND_SEPARATOR)) {
              Serial.println("token 2");
              if (uint32_t period = token.toInt()) {
                if (period > UINT16_MAX) {
                  state->ble.println(F("Invalid second parameter to the [fluxuation] argument."));
                  state->ble.println(F("Expected a value in the range [0-65535]."));
                  break;
                } else {
                  fluxSet = true;
                  failed = false;
                  state->brightFlux.active = true;
                  state->brightFlux.start = state->brightness;
                  state->brightFlux.end = targetBrightness;
                  state->brightFlux.duration = period*500; // Convert to micros and halve to get the start->end duration.
                  state->brightFlux.currentTime = 0;
                  state->ble.println(F("Brightness fluxuation enabled."));
                }
              }
            }
          }
        }
      }
    } else if(uint16_t newVal = token.toInt()) {
      failed = false;
      if (newVal > UINT8_MAX) {
        state->ble.println(F("The number argument must be in the range [0-255]"));
        state->ble.println(F("Example: \"b 150\""));
      } else {
        for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
          state->strips[stripIndex].setBrightness(newVal);
          state->stripDirty[stripIndex] = true;
        }
        if(fluxSet) {
          state->brightFlux.start = newVal;
        } else {
          state->brightFlux.active = false;
        }
        
        Serial.print(F("Adjusting brightness: ")); Serial.println(newVal);
      }
    }  else {
      state->ble.print(F("Unrecognized argument: [")); state->ble.print(token); state->ble.println(F("]"));
    }
  }
  if(failed) {
    if(state->brightFlux.active) {
      state->brightFlux.active = false;
      state->ble.println(F("Brightness fluxuation disabled."));
    } else {
      state->ble.println(F("--==[brightness]==--"));
      state->ble.println(F("Available arguments:"));
      state->ble.println(F("\nfluxuation [0-255] [1000-65535] <optional>"));
      state->ble.println(F(" + Oscilates between two brightness levels, over a given period."));
      state->ble.println(F("\n[0-255] <required>"));
      state->ble.println(F(" + The new brightness value."));
      state->ble.println(F("\nExamples:"));
      state->ble.println(F(" b 150"));
      state->ble.println(F(" bright 0"));
      state->ble.println(F(" brightness fluxuation 0 5000"));
    }
  }
}

void initializePingPongMode(State *state) {
  uint8_t r = state->color >> 16;
  uint8_t g = state->color >> 8;
  uint8_t b = state->color;
  for (uint16_t index = 0; index < state->pingpong.spread; index++) {
    state->pingpong.colorFalloff[index] = Color(r, g, b);
    r /= 2;
    g /= 2;
    b /= 2;
  }
  
  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    for (uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
      if (state->pingpong.dark) {
        state->stripColors[stripIndex][ledIndex] = Color(0, 0, 0);
      } else {
        state->stripColors[stripIndex][ledIndex] = state->pingpong.colorFalloff[state->pingpong.spread-1];
      }
      state->strips[stripIndex].setPixelColor(ledIndex, state->stripColors[stripIndex][ledIndex]);
    }
    state->stripDirty[stripIndex];
  }
}

void processPingPongCommand(State *state) {
  boolean failed = true;
  Easing easing = linear;
  boolean dark = false;
  uint8_t spread;
  uint32_t duration;
  while (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    if (token.equals("e") || token.equals(F("ease")) || token.equals(F("easing"))) {
      if (token = strtok(NULL, COMMAND_SEPARATOR)) {
        if (token.equals("l") || token.equals(F("linear"))) {
          easing = linear;
        } else if (token.equals("s") || token.equals(F("sine"))) {
          easing = cosine;
        } else if (token.equals("e") || token.equals(F("exp")) || token.equals(F("exponential"))) {
          easing = exponential;
        } else if (token.equals("q") || token.equals(F("quart")) || token.equals(F("quartic"))) {
          easing = quartic;
        }
        if (!failed) {
          state->pingpong.easing = easing;
        }
      }
    } else if(token.equals("d") || token.equals("dark")) {
      if(!failed) {
        state->pingpong.dark = true;
      }
      dark = true;
    } else {
      uint32_t number = token.toInt();
      if (number) {
        if (number <= 8) {
          spread = number;
        } else if (number >= 1000 && number <= UINT16_MAX) {
          duration = number;
        }
  
        if (spread && duration) {
          failed = false;
          state->pingpong.active = true;
          state->pingpong.spread = spread;
          state->pingpong.pixel = spread;
          state->pingpong.duration = duration * 1000;
          state->pingpong.directionUp = false;
          state->pingpong.colorFalloff = new uint32_t[spread];
          state->pingpong.easing = easing;
          state->pingpong.dark = dark;
          
          state->ble.println(F("PingPong mode enabled."));
        }
      }
    }
  }
  if(failed) {
    if (state->pingpong.active) {
      state->pingpong.active = false;
      state->ble.println(F("PingPong mode disabled."));
    } else {
      state->ble.println(F("--==[pingpong]==--"));
      state->ble.println(F("Takes 2 arguments"));
      state->ble.println(F("- [1-8] pixel spread"));
      state->ble.println(F("- [1000-65535] duration of a cyle in ms"));
      state->ble.println(F("- ['dark'] (optional) specifies that all other pixels should be turned off"));
      state->ble.println(F("\nExample:"));
      state->ble.println(F(" pingpong 5 5000 d"));
    }
  } else { 
    initializePingPongMode(state);
  }
}

void processRainbowCommand(State *state) {
  boolean firstCommand = true;
  state->rainbow.repeat = 1;
  state->rainbow.currentTime = 0;
  state->rainbow.duration = 0;
  while (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    firstCommand = false;
    state->rainbow.active = true;
    uint32_t number = token.toInt();
    if (number) {
      if (number <= 4) {
        state->rainbow.repeat = number;
      } else if (number >= 1000 && number <= UINT16_MAX) {
        state->rainbow.duration = number * 1000;
      }

      state->ble.println(F("Rainbow mode enabled."));
    }
  }
  if(firstCommand) {
    if (state->rainbow.active) {
      state->rainbow.active = false;
      state->ble.println(F("Rainbow mode disabled."));
    } else {
      state->rainbow.active = true;
      state->ble.println(F("Rainbow mode enabled."));
      state->ble.println(F("--==[rainbow]==--"));
      state->ble.println(F("Takes 2 arguments"));
      state->ble.println(F("- [1-4] (optional) repeat"));
      state->ble.println(F("- [1000-65535] (optional) duration of a cyle in ms"));
      state->ble.println(F("\nExamples:"));
      state->ble.println(F(" rainbow 2 2000"));
      state->ble.println(F(" rainbow"));
    }
  }
}

void processStatusCommand(State *state) {
  state->ble.println(F("--==[status]==--"));
  state->ble.print(F("Color : ")); printColorToBle(state->color, &(state->ble)); state->ble.println();
  state->ble.print(F("Last Micros : ")); state->ble.println(state->lastMicros);
  state->ble.print(F("Current Micros : ")); state->ble.println(micros());
  state->ble.print(F("Delta Micros : ")); state->ble.println(state->deltaMicros);
  state->ble.print(F("Lost Micros : ")); state->ble.println(state->lostMicros);
  state->ble.print(F("Brightness: ")); state->ble.println(state->brightness);
  state->ble.print(F("Rainbow: ")); state->ble.println(state->rainbow.active);
  if (state->rainbow.active) {
    state->ble.print(F("- Repeat: ")); state->ble.println(state->rainbow.repeat);
    state->ble.print(F("- Duration: ")); state->ble.print(state->rainbow.currentTime); state->ble.print("/"); state->ble.println(state->rainbow.duration);
  }
  state->ble.print(F("Brightness Flux: ")); state->ble.println(state->brightFlux.active);
  if (state->brightFlux.active) {
    state->ble.print(F("- Start: ")); state->ble.println(state->brightFlux.start);
    state->ble.print(F("- End: ")); state->ble.println(state->brightFlux.end);
    state->ble.print(F("- Duration: ")); state->ble.print(state->brightFlux.currentTime); state->ble.print("/"); state->ble.println(state->brightFlux.duration);
    state->ble.print(F("- Easing: ")); state->ble.println(state->brightFlux.easing);
  }
  state->ble.print(F("PingPong: ")); state->ble.println(state->pingpong.active);
  if (state->pingpong.active) {
    state->ble.print(F("- pixel: ")); state->ble.println(state->pingpong.pixel);
    state->ble.print(F("- Spread: ")); state->ble.println(state->pingpong.spread);
    state->ble.print(F("- Duration: ")); state->ble.print(state->pingpong.currentTime); state->ble.print("/"); state->ble.println(state->pingpong.duration);
    state->ble.print(F("- Dark: ")); state->ble.println(state->pingpong.dark);
    state->ble.print(F("- Falloff: "));
    for(uint8_t spreadIndex = 0; spreadIndex < state->pingpong.spread; spreadIndex++) {
      printColorToBle(state->pingpong.colorFalloff[spreadIndex], &(state->ble));
      if (spreadIndex + 1 < state->pingpong.spread) {
        state->ble.print(F(", "));
      } else {
        state->ble.println();
      }
    }
    state->ble.print(F("Left Post: ")); state->ble.print(state->strips[LEFT_POST_ID].numPixels()); state->ble.println(F(" pixels"));
    state->ble.print(F("- Radio Button: ")); state->ble.println(state->rfState[LEFT_POST_ID]);
    state->ble.print(F("- Bluetooth Button: ")); state->ble.println(state->bleState[LEFT_POST_ID]);
    
    state->ble.print(F("Left Rail: ")); state->ble.print(state->strips[LEFT_RAIL_ID].numPixels()); state->ble.println(F(" pixels"));
    state->ble.print(F("- Radio Button: ")); state->ble.println(state->rfState[LEFT_RAIL_ID]);
    state->ble.print(F("- Bluetooth Button: ")); state->ble.println(state->bleState[LEFT_RAIL_ID]);
    
    state->ble.print(F("Right Post: ")); state->ble.print(state->strips[RIGHT_POST_ID].numPixels()); state->ble.println(F(" pixels"));
    state->ble.print(F("- Radio Button: ")); state->ble.println(state->rfState[RIGHT_POST_ID]);
    state->ble.print(F("- Bluetooth Button: ")); state->ble.println(state->bleState[RIGHT_POST_ID]);
    
    state->ble.print(F("Right Rail: ")); state->ble.print(state->strips[RIGHT_RAIL_ID].numPixels()); state->ble.println(F(" pixels"));
    state->ble.print(F("- Radio Button: ")); state->ble.println(state->rfState[RIGHT_RAIL_ID]);
    state->ble.print(F("- Bluetooth Button: ")); state->ble.println(state->bleState[RIGHT_RAIL_ID]);
  }
}

void processSaveCommand(State *state) {
  boolean failed = true;
  state->pingpong.easing = linear;
  if (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    uint8_t slot = token.toInt();
    if (slot < 5) {
      failed = false;
      state->ble.print(F("State saved to slot ")); state->ble.println(slot);

      state->saveState[slot].brightness = state->brightness;
      state->saveState[slot].color = state->color;
      
      state->saveState[slot].brightFlux.active = state->brightFlux.active;
      state->saveState[slot].brightFlux.start = state->brightFlux.start;
      state->saveState[slot].brightFlux.end = state->brightFlux.end;
      state->saveState[slot].brightFlux.duration = state->brightFlux.duration;
      state->saveState[slot].brightFlux.easing = state->brightFlux.easing;
      
      state->saveState[slot].rainbow.active = state->rainbow.active;
      state->saveState[slot].rainbow.repeat = state->rainbow.repeat;
      state->saveState[slot].rainbow.duration = state->rainbow.duration;
      
      state->saveState[slot].pingpong.active = state->pingpong.active;
      state->saveState[slot].pingpong.spread = state->pingpong.spread;
      state->saveState[slot].pingpong.duration = state->pingpong.duration;
      state->saveState[slot].pingpong.dark = state->pingpong.dark;
      state->saveState[slot].pingpong.easing = state->pingpong.easing;

      for (uint8_t spread = 0; spread < state->pingpong.spread; spread++) {
        state->saveState[slot].pingpong.colorFalloff[spread] = state->pingpong.colorFalloff[spread];
      }
    }
  }
  if(failed) {
    state->ble.println(F("--==[save]==--"));
    state->ble.println(F("Takes 1 argument"));
    state->ble.println(F("- [0-4] slot"));
    state->ble.println(F("\nExample:"));
    state->ble.println(F(" save 0"));
  }
}

void processLoadCommand(State *state) {
  boolean failed = true;
  state->pingpong.easing = linear;
  if (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    uint8_t slot = token.toInt();
    if (slot < 5) {
      failed = false;
      state->ble.print(F("Slot ")); state->ble.print(slot); state->ble.println(F(" loaded"));

      state->brightness = state->saveState[slot].brightness;
      state->color = state->saveState[slot].color;
      
      state->brightFlux.active = state->saveState[slot].brightFlux.active;
      state->brightFlux.start = state->saveState[slot].brightFlux.start;
      state->brightFlux.end = state->saveState[slot].brightFlux.end;
      state->brightFlux.duration = state->saveState[slot].brightFlux.duration;
      state->brightFlux.currentTime = 0;
      state->brightFlux.easing = state->saveState[slot].brightFlux.easing;
      
      state->rainbow.active = state->saveState[slot].rainbow.active;
      state->rainbow.repeat = state->saveState[slot].rainbow.repeat;
      state->rainbow.duration = state->saveState[slot].rainbow.duration;
      state->rainbow.currentTime = 0;
      
      state->pingpong.active = state->saveState[slot].pingpong.active;
      state->pingpong.pixel = state->saveState[slot].pingpong.spread;
      state->pingpong.spread = state->saveState[slot].pingpong.spread;
      state->pingpong.duration = state->saveState[slot].pingpong.duration;
      state->pingpong.currentTime = 0;
      state->pingpong.directionUp = false;
      state->pingpong.dark = state->saveState[slot].pingpong.dark;
      state->pingpong.easing = state->saveState[slot].pingpong.easing;

      for (uint8_t spread = 0; spread < state->pingpong.spread; spread++) {
        state->saveState[slot].pingpong.colorFalloff[spread] = state->pingpong.colorFalloff[spread];
      }
    }
  }
  if(failed) {
    state->ble.println(F("--==[load]==--"));
    state->ble.println(F("Takes 1 argument"));
    state->ble.println(F("- [0-4] slot"));
    state->ble.println(F("\nExample:"));
    state->ble.println(F(" load 0"));
  }
}

void processEasingCommand(State *state) {
  state->ble.println(F("--==[easing]==--"));
  state->ble.println(F("List of available Easings:"));
  state->ble.println(F("- Linear (default)"));
  state->ble.println(F("- Sine"));
  state->ble.println(F("- Exponential"));
  state->ble.println(F("- Quartic"));
}

