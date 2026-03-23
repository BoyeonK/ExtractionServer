#pragma once

#pragma pack(push, 1)
struct UDPHeader {
    uint16_t packetId;
    uint16_t sessionId;
    uint32_t sequenceNum;
    uint32_t securityKey;
    uint8_t  flags;
};
#pragma pack(pop)