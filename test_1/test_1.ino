#include <Adafruit_NeoPixel.h>
#include <string.h>
#include <Arduino.h>
#include <SPI.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "config.h"
#include "state.h"
#include "commandParser.h"
#include "packetParser.h"
#include "mathLib.h"

State state;

// A small helper
void error(const __FlashStringHelper * err) {
  Serial.println(err);
  while (1);
}

void setup() {
  state.color = Color(255, 255, 255);
  initializeBLE();
  initializeLEDs();
  initializeRF();
  state.lastMicros = micros();
}

void initializeBLE() {
  Serial.begin(115200);

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));
  if(!state.ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit, make sure it's in CommanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if(FACTORYRESET_ENABLE) {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! state.ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }

  state.ble.sendCommandCheckOK("AT+GAPDEVNAME=Bed Controller");

  /* Disable command echo from Bluefruit */
  state.ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  state.ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!\n"));

  state.ble.verbose(false);  // debug info is a little annoying after this point!

  Serial.println(F("******************************"));

  // Set Bluefruit to DATA mode
  Serial.println(F("Switching to DATA mode!"));
  state.ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));
}

void initializeLEDs() {
  for(uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    state.stripColors[stripIndex] = new uint32_t[state.strips[stripIndex].numPixels()];
    state.strips[stripIndex].setBrightness(0);
    state.strips[stripIndex].begin();
    for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
      state.stripColors[stripIndex][ledIndex] = Color(255, 255, 255);
      state.strips[stripIndex].setPixelColor(ledIndex, state.stripColors[stripIndex][ledIndex]);
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
  checkFPS();
  
  updateTime();
  
  readInput();
  
  processBLEBuffer();
  processBrightnessFluxuation();
  processPingPong();
  processRainbow();

  for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
    if (state.buttonStateChanged[stripIndex]) {
      state.stripDirty[stripIndex] = true;
      if (state.rfState[stripIndex] ^ state.bleState[stripIndex]) {
        state.strips[stripIndex].setBrightness(state.brightness);
      } else {
        state.strips[stripIndex].setBrightness(0);
      }
      for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
        state.strips[stripIndex].setPixelColor(ledIndex, state.stripColors[stripIndex][ledIndex]);
      }
    }
    
    if (state.stripDirty[stripIndex]) {
      state.lostMicros += state.strips[stripIndex].numPixels() * 27;
      state.strips[stripIndex].show();
      state.stripDirty[stripIndex] = false;
    }
  }
}

void checkFPS() { 
  state.numCalls++;
  state.fpsMicros += state.deltaMicros;
  if(state.fpsMicros > 1000000) {
    Serial.print("FPS ");
    Serial.println(state.numCalls);

    state.numCalls = 0;
    state.fpsMicros = 0;
  }
}

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
      case LEFT_POST_ID: newRfState = digitalRead(KEYFOB_PIN_A) == HIGH; break;
      case LEFT_RAIL_ID: newRfState = digitalRead(KEYFOB_PIN_C) == HIGH; break;
      case RIGHT_POST_ID: newRfState = digitalRead(KEYFOB_PIN_B) == HIGH; break;
      case RIGHT_RAIL_ID: newRfState = digitalRead(KEYFOB_PIN_D) == HIGH; break;
    }
    
    state.buttonStateChanged[stripIndex] = state.rfState[stripIndex] != newRfState;
    state.rfState[stripIndex] = newRfState;
  }

  /**********************
   * Bluetooth input
   *********************/
  readPacket(&(state.ble));
}

void processBrightnessFluxuation() {
  if(state.brightFlux.active) {
    state.brightFlux.currentTime += state.deltaMicros;
    if (state.brightFlux.currentTime >= state.brightFlux.duration) {
      state.brightness = state.brightFlux.end;
      uint8_t holdValue = state.brightFlux.end;
      state.brightFlux.end = state.brightFlux.start;
      state.brightFlux.start = holdValue;
      state.brightFlux.currentTime = 0;
    } else {
      double currentPosition = easing_cosine((double)state.brightFlux.currentTime / (double)state.brightFlux.duration);
      state.brightness = lerp(currentPosition, state.brightFlux.start, state.brightFlux.end);
    }
    
    for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
      state.strips[stripIndex].setBrightness(state.brightness);
      state.stripDirty[LEFT_POST_ID] = true;
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
      setBothSidesPixelColor(state.pingpong.pixel + dist, state.pingpong.colorFalloff[dist]);
      setBothSidesPixelColor(state.pingpong.pixel - dist, state.pingpong.colorFalloff[dist]);
    }

    uint32_t blankColor = state.pingpong.dark ? 0 : state.pingpong.colorFalloff[state.pingpong.spread - 1];
    for (uint8_t blankSpread = 0; blankSpread < 10; blankSpread++) {
      setBothSidesPixelColor(state.pingpong.pixel + (state.pingpong.spread + blankSpread), blankColor);
      setBothSidesPixelColor(state.pingpong.pixel - (state.pingpong.spread + blankSpread), blankColor);
    }
  }
}

void processRainbow() {
  if (state.rainbow.active) {
//    uint8_t changePerPixel = LEDS_PER_SIDE / state.rainbow.repeat;
    uint8_t changePerPixel = (256 * 6 * state.rainbow.repeat) / LEDS_PER_SIDE;
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
  if(packetbuffer[0]) {
    // Controller Input
    if (packetbuffer[0] == '!') {
      if (packetbuffer[1] == 'C') {
        // Colors
        uint8_t red = packetbuffer[2];
        uint8_t green = packetbuffer[3];
        uint8_t blue = packetbuffer[4];
        
        Serial.print (F("Setting color to #"));
        if (red < 0x10) Serial.print(F("0")); Serial.print(red, HEX);
        if (green < 0x10) Serial.print(F("0")); Serial.print(green, HEX);
        if (blue < 0x10) Serial.print(F("0")); Serial.println(blue, HEX);

        state.color = Color(red, green, blue);

        if(state.pingpong.active) {
          initializePingPongMode(&state);
        } else {
          for (uint16_t stripIndex = 0; stripIndex < NUM_STRIPS; stripIndex++) {
            for(uint16_t ledIndex = 0; ledIndex < state.strips[stripIndex].numPixels(); ledIndex++) {
              state.stripColors[stripIndex][ledIndex] = Color(red, green, blue);
              state.strips[stripIndex].setPixelColor(ledIndex, state.stripColors[stripIndex][ledIndex]);
            }
            state.stripDirty[stripIndex] = true;
          }
        }
      } else if (packetbuffer[1] == 'B') {
        // Buttons
        uint8_t buttonNum = packetbuffer[2] - '0';
        boolean pressed = packetbuffer[3] - '0';
        Serial.print(F("Button ")); Serial.print(buttonNum);
        if (pressed) {
          switch(buttonNum) {
            case 1:
              state.bleState[LEFT_POST_ID] = !state.bleState[LEFT_POST_ID];
              state.buttonStateChanged[LEFT_POST_ID] = true;
              break;
            case 2:
              state.bleState[RIGHT_POST_ID] = !state.bleState[RIGHT_POST_ID];
              state.buttonStateChanged[RIGHT_POST_ID] = true;
              break;
            case 3:
              state.bleState[LEFT_RAIL_ID] = !state.bleState[LEFT_RAIL_ID];
              state.buttonStateChanged[LEFT_RAIL_ID] = true;
              break;
            case 4:
              state.bleState[RIGHT_RAIL_ID] = !state.bleState[RIGHT_RAIL_ID];
              state.buttonStateChanged[RIGHT_RAIL_ID] = true;
              break;
          }
          Serial.println(F(" pressed"));
          
          Serial.print(F("Button State: "));
          Serial.println(state.bleState[LEFT_POST_ID]);
        } else {
          Serial.println(F(" released"));
        }
      }
    } else {
      // Command Line
      String token = strtok(packetbuffer, " -\n");
      for(uint8_t i = 0; token[i]; i++) {
        token[i] = tolower(token[i]);
      }
      
      if (token.equals("b") || token.equals("bright") || token.equals("brightness")) {
        processBrightnessCommand(&state);
      } else if(token.equals("p") || token.equals("ping") || token.equals("pingpong")) {
        processPingPongCommand(&state);
      } else if(token.equals("stat") || token.equals("status")) {
        processStatusCommand(&state);
      } else if(token.equals("r") || token.equals("rainbow")) {
        processRainbowCommand(&state);
      } else if(token.equals("s") || token.equals("save")) {
        processSaveCommand(&state);
      } else if(token.equals("l") || token.equals("load")) {
        processLoadCommand(&state);
      } else if(token.equals("e") || token.equals("ease") || token.equals("easing")) {
        processEasingCommand(&state);
      } else if(token.equals("h") || token.equals("help")) {
        state.ble.println(F("--==[Help]==--"));
        state.ble.println(F("List of available commands:"));
        state.ble.println(F("- Brightness"));
        state.ble.println(F("- PingPong"));
        state.ble.println(F("- Status"));
        state.ble.println(F("- Save"));
        state.ble.println(F("- Load"));
        state.ble.println(F("- Easing"));
        state.ble.println(F("- Help"));
      } else {
        state.ble.println(F("Command not recognized. Please use \"help\" for a list of available commands."));
      }
    }
  }
}

/**
 * Indexed from the top of the post (which is inversed
 */
void setBothSidesPixelColor(uint8_t pixel, uint32_t color) {
  if(pixel < LEDS_PER_SIDE && pixel >= 0) {
    if(pixel < LEFT_POST_LEDS) {
      pixel = LEFT_POST_LEDS - (pixel+1);
      state.stripColors[LEFT_POST_ID][pixel] = color;
      state.strips[LEFT_POST_ID].setPixelColor(pixel, color);
      state.stripDirty[LEFT_POST_ID] = true;
      // TODO add for right post
      
//      state.stripColors[RIGHT_POST_ID][pixel] = color;
//      state.strips[RIGHT_POST_ID].setPixelColor(pixel, color);
//      state.stripDirty[RIGHT_POST_ID] = true;
    } else {
      pixel -= LEFT_POST_LEDS;
      state.stripColors[LEFT_RAIL_ID][pixel] = color;
      state.strips[LEFT_RAIL_ID].setPixelColor(pixel, color);
      state.stripDirty[LEFT_RAIL_ID] = true;
      
//      state.stripColors[RIGHT_RAIL_ID][pixel] = color;
//      state.strips[RIGHT_RAIL_ID].setPixelColor(pixel, color);
//      state.stripDirty[RIGHT_RAIL_ID] = true;
    }
  }
}


