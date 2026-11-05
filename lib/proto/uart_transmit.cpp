#include <Arduino.h>
#include "uart_transmit.h"
#include "uart_protocol.h"

void uart_send(uint8_t r, uint8_t g, uint8_t b) {
    RGBPacket packet;
   
    packet.header.start = PACKET_START;
    packet.header.type = TYPE_RGB; // Убедитесь, что этот тип добавлен в протокол
    packet.header.length = sizeof(RGBPacket);
    packet.header.flags = 0;
    
    packet.R = r;
    packet.G = g;
    packet.B = b;
      
    packet.crc = crc8((uint8_t*)&packet, packet.header.length - 1);

}