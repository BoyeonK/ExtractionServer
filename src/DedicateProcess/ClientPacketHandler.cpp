#include "ClientPacketHandler.h"

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include "../IoUringWrapper.h"
#include "../SendBuffer.h"
#include "DediSessions.h"

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

    External_Game_Protocol::D2CTestPkt resp;
    resp.set_echo(pkt.echo());

    uint32_t payloadSize = static_cast<uint32_t>(resp.ByteSizeLong());
    uint32_t totalSize = sizeof(UDPHeader) + payloadSize;

    SendBuffer* sendBuffer = IORing->OpenSendBuffer(totalSize);
    if (sendBuffer == nullptr) return false;

    UDPHeader* header = reinterpret_cast<UDPHeader*>(sendBuffer->Buffer());
    header->packetId    = PKT_ID_D2C_TEST_PKT;
    header->sessionId   = static_cast<uint16_t>(pSession->GetSessionId());
    header->sequenceNum = 0;
    header->securityKey = pSession->GetSecurityKey();
    header->flags       = 0;

    resp.SerializeToArray(sendBuffer->Buffer() + sizeof(UDPHeader), static_cast<int>(payloadSize));
    sendBuffer->Close(totalSize);

    pDediServer->GetClientSession()->Send(sendBuffer, clientAddr);
    return true;
}