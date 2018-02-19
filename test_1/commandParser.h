#include "state.h"

#ifndef COMMAND_PARSER_H_
#define COMMAND_PARSER_H_

void readPacket(State *state);

void updateCharacteristic(State *state, int32_t charId, byte* newVal);

void processBreathingCommand(State *state);

void processPingPongCommand(State *state);

void processRainbowCommand(State *state);

void processColorCommand(State *state);

void processIntensityCommand(State *state);

void processToggleCommand(State *state);

void processAllOnOffCommand(State *state);

void processKillCommand(State *state);

void updatePingPongCharacteristic(State *state);

void updateBreathingCharacteristic(State *state);

void updateRainbowCharacteristic(State *state);

#endif
