#include "state.h"
#include "mathLib.h"
#include "commandParser.h"

void readPacket(State *state) 
{
  memset(state->packetBuffer, 0, READ_BUFFER_SIZE);
  
  for(uint8_t i = 0; state->ble.available(); i++) {
    char c = state->ble.read();
    state->packetBuffer[i] = c;
  }
}

void updateCharacteristic(State *state, int32_t charId, byte* newVal) {
  String s = GATTCHAR;
  s += charId;
  s += ',';
  s += String((char*)newVal);
  char command[s.length() + 1];
  s.toCharArray(command, s.length()+1);

  state->ble.setMode(BLUEFRUIT_MODE_COMMAND);
  state->ble.println(command);
  state->ble.setMode(BLUEFRUIT_MODE_DATA);
//  state->ble.sendCommandCheckOK(command);
}

/**
 * b+<startBrightness><endBrightness><duration><easing>
 * <startBrightness> 1 byte
 * <endBrightness> 1 byte
 * <duration> 1 byte
 * <easing> 1 byte
 */
void processBreathingCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->breathing.active = true;
    state->breathing.start = state->packetBuffer[2];
    state->breathing.end = state->packetBuffer[3];
    state->breathing.duration = state->packetBuffer[4] * 1000L * 1000L;
    state->breathing.easing = state->packetBuffer[5];

    // Setting Breathing overrides PingPong command
    if (state->pingpong.active) {
      state->pingpong.active = false;
      updatePingPongCharacteristic(state);
    }
  } else {
    state->breathing.active = false;
  }

  updateBreathingCharacteristic(state);
}

/**
 * p+<spread><duration><easing><options>
 * <spread> 1 byte
 * <duration> 1 byte
 * <easing> 1 byte
 * <options> 1 char each
 * - d {dark}
 */
void processPingPongCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->pingpong.active = true;
    state->pingpong.spread = state->packetBuffer[2];
    state->pingpong.duration = state->packetBuffer[3] * 1000L * 1000L;
    state->pingpong.easing = state->packetBuffer[4];
    state->pingpong.directionUp = false;

    state->pingpong.dark = false;
    uint8_t optionsPointer = 5;
    while (state->packetBuffer[optionsPointer]) {
      switch(state->packetBuffer[optionsPointer]) {
        case 'd': state->pingpong.dark = true; break;
      }
      optionsPointer++;
    }

    uint8_t o = 255;
    for (uint8_t index = 0; index < state->pingpong.spread; index++) {
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
      state->stripDirty.set(stripIndex, true);
    }
    
    // Setting PingPong overrides Breathing command
    if (state->breathing.active) {
      state->breathing.active = false;
      updateBreathingCharacteristic(state);
    }
  } else {
    state->pingpong.active = false;
  }
  
  updatePingPongCharacteristic(state);
}

/**
 * r+<repeat><duration>
 * <repeat> 1 byte
 * <duration> 1 bytes
 */
void processRainbowCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->rainbow.active = true;
    state->rainbow.repeat = state->packetBuffer[2];
    state->rainbow.duration = state->packetBuffer[3] * 1000L * 1000L;
  } else {
    state->rainbow.active = false;
  }
  
  updateRainbowCharacteristic(state);
}

/**
 * c<red><green><blue>
 * <red> 1 byte
 * <green> 1 byte
 * <blue> 1 byte
 */
void processColorCommand(State *state) {
  byte r = state->packetBuffer[1];
  byte g = state->packetBuffer[2];
  byte b = state->packetBuffer[3];
  
  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    for (uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
      state->stripColors[stripIndex][ledIndex].setColor(r, g, b);
    }
    state->stripDirty.set(stripIndex, true);
  }
  
  // Setting Color overrides Rainbow command
  if (state->rainbow.active) {
    state->rainbow.active = false;
    updateRainbowCharacteristic(state);
  }
}

/**
 * i<intensity>
 * <intensity> 1 byte
 */
void processIntensityCommand(State *state) {
  byte newIntensity = state->packetBuffer[1];
  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    for (uint16_t ledIndex = 0; ledIndex < state->strips[stripIndex].numPixels(); ledIndex++) {
      state->stripColors[stripIndex][ledIndex].o = newIntensity;
    }
    state->stripDirty.set(stripIndex, true);
  }

  // Setting Intensity overrides PingPong and Breathing command
  if (state->pingpong.active) {
    state->pingpong.active = false;
    updatePingPongCharacteristic(state);
    
  }
  if (state->breathing.active) {
    state->breathing.active = false;
    updateBreathingCharacteristic(state);
  }
}

/**
 * t[0-3][+-]
 */
void processToggleCommand(State *state) {
  byte strip = state->packetBuffer[1];
  boolean newState = state->packetBuffer[2] == '+';
  boolean currentState = state->rfState.get(strip) ^ state->bleState.get(strip);
  
  if (currentState != newState) {
    state->bleState.toggle(strip);
      state->buttonStateChanged.set(strip, true);
  }
}

/**
 * a[+-]
 */
void processAllOnOffCommand(State *state) {
  boolean newState = state->packetBuffer[1] == '+';
  
  for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    boolean currentState = state->rfState.get(stripIndex) ^ state->bleState.get(stripIndex);
    
    if (currentState != newState) {
      state->bleState.toggle(stripIndex);
      state->buttonStateChanged.set(stripIndex, true);
    }
  }
}

/**
 * k
 */
void processKillCommand(State *state) {
  asm volatile ("  jmp 0"); 
}

/**
 * State: [5]
 * [0] active
 * [1] spread
 * [2] duration
 * [3] dark
 * [4] easing
 */
void updatePingPongCharacteristic(State *state) {
  byte command[5];
  command[0] = state->pingpong.active;
  command[1] = state->pingpong.spread;
  command[2] = (byte) (state->pingpong.duration / (1000 * 1000));
  command[3] = state->pingpong.dark;
  command[4] = state->pingpong.easing;
  updateCharacteristic(state, state->monitor.pingPongStateCharId, command);
}

/**
 * State: [5]
 * [0] active
 * [1] startIntensity
 * [2] endIntensity
 * [3] duration
 * [4] easing
 */
void updateBreathingCharacteristic(State *state) {
  byte command[5];
  command[0] = state->breathing.active;
  command[1] = state->breathing.start;
  command[2] = state->breathing.end;
  command[3] = (byte) (state->breathing.duration / (1000 * 1000));
  command[4] = state->breathing.easing;
  updateCharacteristic(state, state->monitor.breathingStateCharId, command);
}

/**
 * State: [3]
 * [0] active
 * [1] repeat
 * [2] duration
 */
void updateRainbowCharacteristic(State *state) {
  byte command[5];
  command[0] = state->rainbow.active;
  command[1] = state->rainbow.repeat;
  command[2] = (byte) (state->rainbow.duration / (1000 * 1000));
  updateCharacteristic(state, state->monitor.rainbowStateCharId, command);
}

