#include "state.h"

#ifndef COMMAND_PARSER_H_
#define COMMAND_PARSER_H_

void processBrightnessCommand(State *state);

void processPingPongCommand(State *state);

void processRainbowCommand(State *state);

void processColorCommand(State *state);

void processInfoCommand(State *state);

void processSaveCommand(State *state);

void processLoadCommand(State *state);

void processEasingCommand(State *state);

#endif
