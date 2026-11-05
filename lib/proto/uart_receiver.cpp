#include <Arduino.h>
#include "ui/actions.h"
#include "actions.h"
#include "Variables.h"

#include "uart_receiver.h"
#include "uart_protocol.h"


static TelemetryPacket telemetry;
static ChannelsPacket  Channels;

#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define UART_BAUD   115200


void uart_init(void)
{
    Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
}

void uart_receive()
{
  enum
  {
    WAIT_START,
    WAIT_TYPE,
    WAIT_LENGTH,
    WAIT_PAYLOAD
  };
  static uint8_t state = WAIT_START;
  static uint8_t packet[MAX_PACKET_SIZE];
  static uint8_t index = 0;
  static uint8_t packetLength = 0;
  while (Serial1.available())
  {
    uint8_t b = Serial1.read();
    // Если во время приема встретился новый стартовый байт,
    // начинаем принимать новый пакет.
    if (state != WAIT_START && b == PACKET_START)
    {
      packet[0] = b;
      index = 1;
      state = WAIT_TYPE;
      continue;
    }
    switch (state){
      case WAIT_START:
        if (b == PACKET_START)
        {
          packet[0] = b;
          index = 1;
          state = WAIT_TYPE;
        }
        break;
      case WAIT_TYPE:
        packet[index++] = b;
        state = WAIT_LENGTH;
        break;
      case WAIT_LENGTH:
        packet[index++] = b;
        packetLength = b;
        if (packetLength < sizeof(PacketHeader) || packetLength > MAX_PACKET_SIZE)
        {
          state = WAIT_START;
          index = 0;
          packetLength = 0;
          break;
        }
        state = WAIT_PAYLOAD;
        break;
      case WAIT_PAYLOAD:
        packet[index++] = b;
        if (index < packetLength)
          break;
        // Пакет полностью получен
        state = WAIT_START;
        index = 0;
        // Проверка CRC
        if (crc8(packet, packetLength - 1) != packet[packetLength - 1])
          break;
        switch (packet[1])
        {
          case TYPE_TELEMETRY:
            if (packetLength != sizeof(TelemetryPacket)) break;
            memcpy(&telemetry, packet, sizeof(TelemetryPacket));
            update_topbar_icon(telemetry.battery, telemetry.rssi);
            break;
          case TYPE_CHANNELS:
            if (packetLength != sizeof(ChannelsPacket)) break;
            memcpy(&Channels, packet, sizeof(ChannelsPacket)); 
            //работаем с Channels.ChannelsPacket[]                   
            break;
          case TYPE_SETTINGS:
            // обработка позже
            break;
          default:
            break;
        }
      break;
    }
  }
}


