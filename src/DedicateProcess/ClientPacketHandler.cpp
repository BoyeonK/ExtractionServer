#include "ClientPacketHandler.h"

std::function<bool(PlayerSession*, unsigned char*, int32_t)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize) {
    return false;
}

bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt) {
    return true;
}