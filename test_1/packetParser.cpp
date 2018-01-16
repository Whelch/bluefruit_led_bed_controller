#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"


#define PACKET_ACC_LEN                  (15)
#define PACKET_GYRO_LEN                 (15)
#define PACKET_MAG_LEN                  (15)
#define PACKET_QUAT_LEN                 (19)
#define PACKET_BUTTON_LEN               (5)
#define PACKET_COLOR_LEN                (6)
#define PACKET_LOCATION_LEN             (15)

#define READ_BUFFER_SIZE                    40


/* Buffer to hold incoming characters */
uint8_t packetbuffer[READ_BUFFER_SIZE];

/**************************************************************************/
/*!
    @brief  Casts the four bytes at the specified address to a float
*/
/**************************************************************************/
float parsefloat(uint8_t *buffer) 
{
  float f;
  memcpy(&f, buffer, 4);
  return f;
}

/**************************************************************************/
/*! 
    @brief  Prints a hexadecimal value in plain characters
    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void printHex(const uint8_t * data, const uint32_t numBytes)
{
  uint32_t szPos;
  for (szPos=0; szPos < numBytes; szPos++) 
  {
    Serial.print(F("0x"));
    // Append leading 0 for small values
    if (data[szPos] <= 0xF)
    {
      Serial.print(F("0"));
      Serial.print(data[szPos] & 0xf, HEX);
    }
    else
    {
      Serial.print(data[szPos] & 0xff, HEX);
    }
    // Add a trailing space if appropriate
    if ((numBytes > 1) && (szPos != numBytes - 1))
    {
      Serial.print(F(" "));
    }
  }
  Serial.println();
}

/**************************************************************************/
/*!
    @brief  Waits for incoming data and parses it
*/
/**************************************************************************/
void readPacket(Adafruit_BLE *ble) 
{
  memset(packetbuffer, 0, READ_BUFFER_SIZE);
  
  for(uint8_t i = 0; ble->available(); i++) {
    char c = ble->read();
    packetbuffer[i] = c;
  }

//    if (replyidx >= 20) break;
//    if ((packetbuffer[1] == 'A') && (replyidx == PACKET_ACC_LEN))
//      break;
//    if ((packetbuffer[1] == 'G') && (replyidx == PACKET_GYRO_LEN))
//      break;
//    if ((packetbuffer[1] == 'M') && (replyidx == PACKET_MAG_LEN))
//      break;
//    if ((packetbuffer[1] == 'Q') && (replyidx == PACKET_QUAT_LEN))
//      break;
//    if ((packetbuffer[1] == 'B') && (replyidx == PACKET_BUTTON_LEN))
//      break;
//    if ((packetbuffer[1] == 'C') && (replyidx == PACKET_COLOR_LEN))
//      break;
//    if ((packetbuffer[1] == 'L') && (replyidx == PACKET_LOCATION_LEN))
//      break;
}

