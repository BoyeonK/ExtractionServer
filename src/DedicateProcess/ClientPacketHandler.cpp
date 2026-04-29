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

bool Handle_C2D_ChannelOpen(PlayerSession* pSession, External_Game_Protocol::C2DChannelOpen& pkt, const sockaddr_in& clientAddr) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
    uint16_t port = ntohs(clientAddr.sin_port);

    std::cout << "매치 테스트 12 - O : 송신자 IP: " << ipStr << ", Port: " << port << std::endl;

    pSession->SetPort(port);

    External_Game_Protocol::D2CResponseChannelOpen sendPkt;
    sendPkt.set_echo(pkt.echo());

    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseChannelOpen(sendPkt, pSession);

    //TODO : pSession->Send(sendBuffer); 로 교체할까?
    pDediServer->Send(sendBuffer, pSession->GetAddress());

    std::cout << "매치 테스트 13 - 에코 패킷 전송" << std::endl;

    return true;
}

bool Handle_C2D_HeartBeat(PlayerSession* pSession, External_Game_Protocol::C2DHeartBeat& pkt, const sockaddr_in& clientAddr) {
    External_Game_Protocol::D2CHeartBeat sendPkt;
    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CHeartBeat(sendPkt, pSession, clientAddr);
    pDediServer->Send(sendBuffer, pSession->GetAddress());
    return true;
}

bool Handle_C2D_RequestBlueprint(PlayerSession* pSession, External_Game_Protocol::C2DRequestBlueprint& pkt, const sockaddr_in& clientAddr) {
    // TODO: GameRoom에서 spawn_point, ingame_objects 조회 후 채워넣기
    External_Game_Protocol::D2CResponseBlueprint sendPkt;
    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseBlueprint(sendPkt, pSession, clientAddr);
    pDediServer->Send(sendBuffer, pSession->GetAddress());
    return true;
}

bool Handle_C2D_RequestSpawnMe(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnMe& pkt, const sockaddr_in& clientAddr) {
    // TODO: pSession 기반으로 스폰 오브젝트 목록 조회 후 채워넣기
    External_Game_Protocol::D2CResponseSpawnMe sendPkt;
    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseSpawnMe(sendPkt, pSession, clientAddr);
    pDediServer->Send(sendBuffer, pSession->GetAddress());
    return true;
}