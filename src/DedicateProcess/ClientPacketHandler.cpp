#include "ClientPacketHandler.h"

std::function<bool(PlayerSession*, unsigned char*, int32_t)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize) {
    return false;
}

bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt) {
    std::cout << "매치 테스트 12 - O : 일단 패킷 받음." << std::endl;
    return true;
}