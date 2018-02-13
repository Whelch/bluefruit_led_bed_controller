#include "LedStrip.h"
#include <string.h>
#include <Arduino.h>
#include <SPI.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "config.h"
#include "state.h"
#include "commandParser.h"
#include "mathLib.h"

State state;

void setup() {
  initializeBLE();
  initializeLEDs();
  initializeRF();
  state.lastMicros = micros();
}

void initializeBLE() {

  state.ble.begin(VERBOSE_MODE);

  if(FACTORYRESET_ENABLE) {
    state.ble.factoryReset();
  }

  state.ble.sendCommandCheckOK("AT+GAPDEVNAME=Bed Controller");
  state.ble.echo(false);
  state.ble.verbose(false);
  state.ble.setMode(BLUEFRUIT_MODE_DATA);
}

void initializeLEDs() {
  for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    state.stripColors[stripIndex] = new Color[state.strips[stripIndex].numPixels()];
    state.strips[stripIndex].setBrightness(0);
    state.strips[stripIndex].begin();
    for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
      state.stripColors[stripIndex][ledIndex] = Color(255, 255, 255, 255);
      state.strips[stripIndex].setPixelColor(ledIndex, state.stripColors[stripIndex][ledIndex].getConverted());
    }
    state.strips[stripIndex].show();
  }
}

void initializeRF() {
  pinMode(KEYFOB_PIN_A, INPUT);
  pinMode(KEYFOB_PIN_B, INPUT);
  pinMode(KEYFOB_PIN_C, INPUT);
  pinMode(KEYFOB_PIN_D, INPUT);
}

void loop() {
//  checkFPS();
  
  updateTime();
  
  readInput();
  
  processBLEBuffer();
  processBreathing();
  processPingPong();
  processRainbow();

  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    boolean forceUpdate = false;
    if (state.buttonStateChanged.get(stripIndex)) {
      state.stripDirty.set(stripIndex, true);
      forceUpdate = true;
      if (state.rfState.get(stripIndex) ^ state.bleState.get(stripIndex)) {
        state.strips[stripIndex].setBrightness(255);
      } else {
        state.strips[stripIndex].setBrightness(0);
      }
    }
    
    if (forceUpdate || (state.stripDirty.get(stripIndex) && state.strips[stripIndex].getBrightness())) {
      for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
        state.strips[stripIndex].setPixelColor(ledIndex, state.stripColors[stripIndex][ledIndex].getConverted());
      }
      state.lostMicros += state.strips[stripIndex].numPixels() * 27;
      if (stripIndex == RIGHT_POST_ID) {
        Serial.println("Trying to update right post");
      }
      state.strips[stripIndex].show();
      state.stripDirty.set(stripIndex, false);
    }
  }
}

//void checkFPS() { 
//  state.numCalls++;
//  state.fpsMicros += state.deltaMicros;
//  if(state.fpsMicros > 1000000) {
//    Serial.print(F("FPS "));
//    Serial.println(state.numCalls);
//
//    state.numCalls = 0;
//    state.fpsMicros = 0;
//  }
//}

/**
 * Time tracking, since micros will not be 100% accurate due to lost time in the
 * strip.show() method (which disables interupts)
 */
void updateTime() {
  unsigned long newMicros = micros();
  if(newMicros < state.lastMicros) {
    // internal rollover
    state.deltaMicros = newMicros + state.lostMicros;
  } else {
    state.deltaMicros = (newMicros - state.lastMicros) + state.lostMicros;
  }
  state.lastMicros = newMicros;
  state.lostMicros = 0;
}

void readInput() {
  /**********************
   * Radio input
   *********************/
  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    boolean newRfState;
    switch (stripIndex) {
      case LEFT_POST_ID: newRfState = digitalRead(KEYFOB_PIN_C) == HIGH; break;
      case LEFT_RAIL_ID: newRfState = digitalRead(KEYFOB_PIN_A) == HIGH; break;
      case RIGHT_POST_ID: newRfState = digitalRead(KEYFOB_PIN_D) == HIGH; break;
      case RIGHT_RAIL_ID: newRfState = digitalRead(KEYFOB_PIN_B) == HIGH; break;
    }
    
    state.buttonStateChanged.set(stripIndex, state.rfState.get(stripIndex) != newRfState);
    state.rfState.set(stripIndex, newRfState);
  }

  /**********************
   * Bluetooth input
   *********************/
  readPacket(&state);
}

void processBreathing() {
  if(state.breathing.active) {
    state.breathing.currentTime += state.deltaMicros;
    uint8_t newBrightness;
    if (state.breathing.currentTime >= state.breathing.duration) {
      newBrightness = state.breathing.end;
      uint8_t holdValue = state.breathing.end;
      state.breathing.end = state.breathing.start;
      state.breathing.start = holdValue;
      state.breathing.currentTime = 0;
    } else {
      double currentPosition = easing_cosine((double)state.breathing.currentTime / (double)state.breathing.duration);
      newBrightness = lerp(currentPosition, state.breathing.start, state.breathing.end);
    }
    
    for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
      state.stripDirty.set(stripIndex, true);
      for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
        state.stripColors[stripIndex][ledIndex].o = newBrightness;
      }
    }
  }
}

void processPingPong() {
  if(state.pingpong.active) {
    uint8_t topPixel = state.pingpong.spread - 1;
    uint8_t bottomPixel = LEDS_PER_SIDE - state.pingpong.spread;
    state.pingpong.currentTime += state.deltaMicros;
    if (state.pingpong.currentTime >= state.pingpong.duration) {
      if (state.pingpong.directionUp) {
        state.pingpong.pixel = topPixel;
      } else {
        state.pingpong.pixel = bottomPixel;
      }
      
      state.pingpong.directionUp = !state.pingpong.directionUp;
      state.pingpong.currentTime = 0;
    } else {
      double easePosition = ease(state.pingpong.easing, (double)state.pingpong.currentTime / (double)state.pingpong.duration);
      if (state.pingpong.directionUp) {
        state.pingpong.pixel = lerp(easePosition, bottomPixel, topPixel);
      } else {
        state.pingpong.pixel = lerp(easePosition, topPixel, bottomPixel);
      }
    }
    
    for(uint8_t dist = 0; dist < state.pingpong.spread; dist++) {
      setBothSidesPixelOpacity(state.pingpong.pixel + dist, state.pingpong.falloff[dist]);
      setBothSidesPixelOpacity(state.pingpong.pixel - dist, state.pingpong.falloff[dist]);
    }

    uint32_t blankColor = state.pingpong.dark ? 0 : state.pingpong.falloff[state.pingpong.spread - 1];
    for (uint8_t blankSpread = 0; blankSpread < 10; blankSpread++) {
      setBothSidesPixelOpacity(state.pingpong.pixel + (state.pingpong.spread + blankSpread), blankColor);
      setBothSidesPixelOpacity(state.pingpong.pixel - (state.pingpong.spread + blankSpread), blankColor);
    }
  }
}

void processRainbow() {
  if (state.rainbow.active) {

    uint8_t changePerPixel;

    if (state.rainbow.repeat) {
      changePerPixel = (256 * 6 * state.rainbow.repeat) / LEDS_PER_SIDE;
    } else { // .5 value
      changePerPixel = (256 * 6) / (LEDS_PER_SIDE * 2);
    }
    
    uint16_t currentColorPosition = 0;
    if (state.rainbow.duration) {
      state.rainbow.currentTime += state.deltaMicros;
      if (state.rainbow.currentTime >= state.rainbow.duration) {
        state.rainbow.currentTime %= state.rainbow.duration;
      }
      currentColorPosition = lerp((double)state.rainbow.currentTime / (double)state.rainbow.duration, 0, (256 * 6) - 1);
    }

    for (uint8_t pixel = 0; pixel < LEDS_PER_SIDE; pixel++) {
      if (currentColorPosition < 256) {
        setBothSidesPixelColor(pixel, Color(255, currentColorPosition, 0));
      } else if (currentColorPosition < 256 * 2) {
        setBothSidesPixelColor(pixel, Color(255 - (currentColorPosition % 256), 255, 0));
      } else if (currentColorPosition < 256 * 3) {
        setBothSidesPixelColor(pixel, Color(0, 255, currentColorPosition % 256));
      } else if (currentColorPosition < 256 * 4) {
        setBothSidesPixelColor(pixel, Color(0, 255 - (currentColorPosition % 256), 255));
      } else if (currentColorPosition < 256 * 5) {
        setBothSidesPixelColor(pixel, Color(currentColorPosition % 256, 0, 255));
      } else if (currentColorPosition < 256 * 6) {
        setBothSidesPixelColor(pixel, Color(255, 0, 255 - (currentColorPosition % 256)));
      }

      currentColorPosition = (currentColorPosition + changePerPixel) % ((256 * 6) - 1);
    }
  }
}

void processBLEBuffer() {
  if(state.packetBuffer[0]) {
     switch(tolower(state.packetBuffer[0])) {
      case 'b': processBreathingCommand(&state); break;
      case 'p': processPingPongCommand(&state); break;
      case 'r': processRainbowCommand(&state); break;
      case 'c': processColorCommand(&state); break;
      case 'i': processIntensityCommand(&state); break;
      case 'k': processKillCommand(&state); break;
      case 'o': processOnOffCommand(&state); break;
    }
  }
}

/**
 * Indexed from the top of the post (which is inversed
 */
void setBothSidesPixelColor(uint8_t pixel, Color color) {
  if(pixel < LEDS_PER_SIDE && pixel >= 0) {
    if(pixel < LEFT_POST_LEDS) {
      pixel = LEFT_POST_LEDS - (pixel+1);
      state.stripColors[LEFT_POST_ID][pixel].setColor(color);
      state.stripDirty.set(LEFT_POST_ID, true);
      
      state.stripColors[RIGHT_POST_ID][pixel].setColor(color);
      state.stripDirty.set(RIGHT_POST_ID, true);
    } else {
      pixel -= LEFT_POST_LEDS;
      state.stripColors[LEFT_RAIL_ID][pixel].setColor(color);
      state.stripDirty.set(LEFT_RAIL_ID, true);
      
      state.stripColors[RIGHT_RAIL_ID][pixel].setColor(color);
      state.stripDirty.set(RIGHT_RAIL_ID, true);
    }
  }
}

void setBothSidesPixelOpacity(uint8_t pixel, uint8_t opacity) {
  if(pixel < LEDS_PER_SIDE && pixel >= 0) {
    if(pixel < LEFT_POST_LEDS) {
      pixel = LEFT_POST_LEDS - (pixel+1);
      state.stripColors[LEFT_POST_ID][pixel].o = opacity;
      state.stripDirty.set(LEFT_POST_ID, true);
      
      state.stripColors[RIGHT_POST_ID][pixel].o = opacity;
      state.stripDirty.set(RIGHT_POST_ID, true);
    } else {
      pixel -= LEFT_POST_LEDS;
      state.stripColors[LEFT_RAIL_ID][pixel].o = opacity;
      state.stripDirty.set(LEFT_RAIL_ID, true);
      
      state.stripColors[RIGHT_RAIL_ID][pixel].o = opacity;
      state.stripDirty.set(RIGHT_RAIL_ID, true);
    }
  }
}


