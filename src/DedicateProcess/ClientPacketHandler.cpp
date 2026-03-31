#include "ClientPacketHandler.h"

bool Handle_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize) {
    return false;
}

bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt) {
    return true;
}