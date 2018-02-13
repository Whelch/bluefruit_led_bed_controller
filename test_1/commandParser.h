#include "state.h"

#ifndef COMMAND_PARSER_H_
#define COMMAND_PARSER_H_

void readPacket(State *state); 

void processBreathingCommand(State *state);

void processPingPongCommand(State *state);

void processRainbowCommand(State *state);

void processColorCommand(State *state);

void processIntensityCommand(State *state);

void processKillCommand(State *state);

void processOnOffCommand(State *state);

#endif
