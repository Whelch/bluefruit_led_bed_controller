#include "state.h"
#include "string.h"
#include "mathLib.h"

void readPacket(State *state) 
{
  memset(state->packetBuffer, 0, READ_BUFFER_SIZE);
  
  for(uint8_t i = 0; state->ble.available(); i++) {
    char c = state->ble.read();
    state->packetBuffer[i] = c;
  }
}

uint16_t convert2BytesToInt(byte* bytes) {
  return (bytes[0] << 8) | bytes[1];
}

uint32_t convert4BytesToInt(byte* bytes) {
  return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

/**
 * b+<startBrightness><endBrightness><duration><easing>
 * <startBrightness> 1 byte
 * <endBrightness> 1 byte
 * <duration> 2 bytes
 * <easing> 1 byte
 */
void processBreathingCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->breathing.active = true;
    state->breathing.start = state->packetBuffer[2];
    state->breathing.end = state->packetBuffer[3];
    state->breathing.duration = convert2BytesToInt(&(state->packetBuffer[4])) * 1000;
    state->breathing.easing = state->packetBuffer[6];

    // Setting Breathing overrides PingPong command
    state->pingpong.active = false;
  } else {
    state->breathing.active = false;
  }
}

/**
 * p+<spread><duration><easing><options>
 * <spread> 1 byte
 * <duration> 2 bytes
 * <easing> 1 byte
 * <options> 1 char each
 * - d {dark}
 */
void processPingPongCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->pingpong.active = true;
    state->pingpong.spread = state->packetBuffer[2];
    state->pingpong.duration = convert2BytesToInt(&(state->packetBuffer[3])) * 1000;
    state->pingpong.easing = state->packetBuffer[5];

    state->pingpong.dark = false;
    uint8_t optionsPointer = 6;
    while (state->packetBuffer[optionsPointer]) {
      switch(state->packetBuffer[optionsPointer]) {
        case 'd': state->pingpong.dark = true; break;
      }
      optionsPointer++;
    }
    
    // Setting PingPong overrides Breathing command
    state->breathing.active = false;
  } else {
    state->pingpong.active = false;
  }
}

/**
 * r+<repeat><duration>
 * <repeat> 1 byte
 * <duration> 2 bytes
 */
void processRainbowCommand(State *state) {
  if (state->packetBuffer[1] == '+') {
    state->rainbow.active = true;
    state->rainbow.repeat = state->packetBuffer[2];
    state->rainbow.duration = convert2BytesToInt(&(state->packetBuffer[3])) * 1000;
  } else {
    state->rainbow.active = false;
  }
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
  state->pingpong.active = false;
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
  state->pingpong.active = false;
  state->breathing.active = false;
}

void processKillCommand(State *state) {
  asm volatile ("  jmp 0"); 
}

void processOnOffCommand(State *state) {
  boolean anyOn = false;
  for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    anyOn |= state->rfState.get(stripIndex) ^ state->bleState.get(stripIndex);
  }

  boolean turnOn = !anyOn;
  for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    state->bleState.set(stripIndex, state->rfState.get(stripIndex) ^ turnOn);
    state->buttonStateChanged.set(stripIndex, true);
  }
}

