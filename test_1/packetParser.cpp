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
}

