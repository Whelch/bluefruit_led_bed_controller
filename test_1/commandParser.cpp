#include "state.h"
#include "string.h"
#include "mathLib.h"

#define COMMAND_SEPARATOR " -\n:="

void processBrightnessCommand(State *state) {
  boolean failed = true;
  uint16_t newVal;
  uint8_t newBrightness = 255;
  while (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    if (token.equals("f")) {
      if (token = strtok(NULL, COMMAND_SEPARATOR)) {
        if (uint8_t otherBrightness = token.toInt() || token.equals("0")) {
          if (token = strtok(NULL, COMMAND_SEPARATOR)) {
            if (uint16_t duration = token.toInt()) {
              failed = false;
              state->brightFlux.active = true;
              state->brightFlux.start = newBrightness;
              state->brightFlux.end = otherBrightness;
              state->brightFlux.duration = ((uint32_t)duration) * 1000; // Convert to micros and halve to get the start->end duration.
              state->brightFlux.currentTime = 0;
              state->ble.println(F("Brightness fluxuation enabled."));
            }
          }
        }
      }
    } else if(newVal = token.toInt() || token.equals("0")) {
      if (newVal <= UINT8_MAX) {
        failed = false;
        state->brightFlux.active = false;
        newBrightness = newVal;
        for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
          for (uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
            state->stripColors[stripIndex][ledIndex].o = newVal;
            state->stripDirty[stripIndex] = true;
          }
        }
        
        Serial.print(F("Adjusting brightness: ")); Serial.println(newVal);
      }
    }
  }
  if(failed) {
    if(state->brightFlux.active) {
      state->brightFlux.active = false;
      state->ble.println(F("Brightness fluxuation disabled."));
    } else {
      state->ble.println(F("--==[brightness]==--"));
      state->ble.println(F("Available arguments:"));
      state->ble.println(F("\nfluxuation [0-255] [1-65535] <optional>"));
      state->ble.println(F("\n[0-255]"));
      state->ble.println(F("\nExamples:"));
      state->ble.println(F(" b 150"));
      state->ble.println(F(" b 0"));
      state->ble.println(F(" b f 0 5000"));
    }
  }
}

void processPingPongCommand(State *state) {
  boolean failed = true;
  Easing easing = linear;
  boolean dark = false;
  uint8_t spread;
  uint32_t duration;
  while (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    if (token.equals("e")) {
      if (token = strtok(NULL, COMMAND_SEPARATOR)) {
        if (token.equals("l")) {
          easing = linear;
        } else if (token.equals("s")) {
          easing = cosine;
        } else if (token.equals("e")) {
          easing = exponential;
        } else if (token.equals("q")) {
          easing = quartic;
        }
        if (!failed) {
          state->pingpong.easing = easing;
        }
      }
    } else if(token.equals("d")) {
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
          state->brightFlux.active = false;
          state->pingpong.spread = spread;
          state->pingpong.pixel = spread;
          state->pingpong.duration = duration * 1000;
          state->pingpong.directionUp = false;
          state->pingpong.falloff = new uint8_t[spread];
          state->pingpong.easing = easing;
          state->pingpong.dark = dark;
          
          uint8_t o = 255;
          for (uint16_t index = 0; index < state->pingpong.spread; index++) {
            state->pingpong.falloff[index] = o;
            o /= 2;
          }
          
          for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
            for (uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
              if (state->pingpong.dark) {
                state->stripColors[stripIndex][ledIndex].o = 0;
              } else {
                state->stripColors[stripIndex][ledIndex].o = state->pingpong.falloff[state->pingpong.spread-1];
              }
            }
            state->stripDirty[stripIndex];
          }
          
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
      state->ble.println(F("- [1-8]: pixel spread"));
      state->ble.println(F("- [1000-65535]: duration of a cyle in ms"));
      state->ble.println(F("- d: (optional) specifies that all other pixels should be turned off"));
      state->ble.println(F("- e <easing>: (optional) adds an easing"));
      state->ble.println(F("\nExample:"));
      state->ble.println(F(" pingpong 5 5000 d"));
    }
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
      state->ble.println(F("- [1000-65535] (optional) duration of a cycle in ms"));
      state->ble.println(F("\nExamples:"));
      state->ble.println(F(" rainbow 2 2000"));
      state->ble.println(F(" rainbow"));
    }
  }
}

void processColorCommand(State *state) {
  boolean failed = true;
  if (String token = strtok(NULL, COMMAND_SEPARATOR)) {
    Color newColor;
    if (token.equals("r")) {
      failed = false;
      newColor.setColor(255, 0, 0);
    } else if(token.equals("y")) {
      failed = false;
      newColor.setColor(255, 255, 0);
    } else if(token.equals("g")) {
      failed = false;
      newColor.setColor(0, 255, 0);
    } else if(token.equals("c")) {
      failed = false;
      newColor.setColor(0, 255, 255);
    } else if(token.equals("b")) {
      failed = false;
      newColor.setColor(0, 0, 255);
    } else if(token.equals("p")) {
      failed = false;
      newColor.setColor(255, 0, 255);
    } else if(token.equals("w")) {
      failed = false;
      newColor.setColor(255, 255, 255);
    }

    if(!failed) {
      state->rainbow.active = false;
      for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
        for(uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
          state->stripColors[stripIndex][ledIndex].setColor(newColor);
        }
        state->stripDirty[stripIndex] = true;
      }
    }
  }

  if (failed) {
    state->ble.println(F("--==[color]==--"));
    state->ble.println(F("Available colors"));
    state->ble.println(F("Red"));
    state->ble.println(F("Yellow"));
    state->ble.println(F("Green"));
    state->ble.println(F("Cyan"));
    state->ble.println(F("Blue"));
    state->ble.println(F("Purple"));
    state->ble.println(F("White"));
  }
}

void processInfoCommand(State *state) {
  state->ble.println(F("--==[info]==--"));
  state->ble.print(F("Last Micros : ")); state->ble.println(state->lastMicros);
  state->ble.print(F("Current Micros : ")); state->ble.println(micros());
  state->ble.print(F("Delta Micros : ")); state->ble.println(state->deltaMicros);
  state->ble.print(F("Lost Micros : ")); state->ble.println(state->lostMicros);
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
      printColorToBle(state->pingpong.falloff[spreadIndex], &(state->ble));
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
        state->saveState[slot].pingpong.falloff[spread] = state->pingpong.falloff[spread];
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
        state->saveState[slot].pingpong.falloff[spread] = state->pingpong.falloff[spread];
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

