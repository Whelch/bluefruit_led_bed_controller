#include "Adafruit_BLE.h"

void readPacket(Adafruit_BLE *ble);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

extern uint8_t packetbuffer[];
