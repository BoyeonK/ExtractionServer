#include "ClientPacketHandler.h"

#include <iostream>
#include <string>
#include <arpa/inet.h>

std::function<bool(PlayerSession*, unsigned char*, int32_t, const sockaddr_in&)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
    return false;
}

bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt, const sockaddr_in& clientAddr) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
    uint16_t port = ntohs(clientAddr.sin_port);
    
    std::cout << "매치 테스트 12 - 송신자 IP: " << ipStr << ", Port: " << port << std::endl;

    pSession->SetPort(port);
    return true;
}