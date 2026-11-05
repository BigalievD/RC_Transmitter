#include <stdint.h>
#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H


#define PACKET_START 0xAA


enum PacketType
{
    TYPE_TELEMETRY = 0x01,
    TYPE_SETTINGS  = 0x02,
    TYPE_CHANNELS  = 0x03,
    TYPE_RGB =       0x04
};

#pragma pack(push,1)
// Общий заголовок любого пакета
struct PacketHeader{
    uint8_t start;     // 0xAA
    uint8_t type;      // PacketType
    uint8_t length;    // полный размер пакета
    uint8_t flags;      // Пока не используется
};
struct TelemetryPacket{
    PacketHeader header;
    uint8_t battery;
    int8_t  rssi;
    uint8_t crc;
};
struct RGBPacket{
    PacketHeader header;
    uint8_t R;
    uint8_t  G;
    uint8_t B;
    uint8_t crc;
};
struct ChannelsPacket{
    PacketHeader header;
    int32_t channel1;
    int32_t mixer1;
    int32_t channel2;
    int32_t mixer2;
    int32_t channel3;
    int32_t mixer3;
    uint8_t crc;
};
#pragma pack(pop)
// CRC8
uint8_t crc8(const uint8_t *data, uint8_t length);

#endif