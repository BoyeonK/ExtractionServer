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
    //TODO : ACK요청에 의해 중복 수신 가능함에 따른 예외처리 필요
    
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
    uint16_t port = ntohs(clientAddr.sin_port);

    std::cout << "매치 테스트 12 - O : 송신자 IP: " << ipStr << ", Port: " << port << std::endl;

    pSession->SetPort(port);

    External_Game_Protocol::D2CResponseChannelOpen sendPkt;
    sendPkt.set_echo(pkt.echo());

    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseChannelOpenReliable(sendPkt, pSession);
    pSession->Send(sendBuffer);

    std::cout << "매치 테스트 13 - 에코 패킷 전송" << std::endl;

    return true;
}


bool Handle_C2D_HeartBeat(PlayerSession* pSession, External_Game_Protocol::C2DHeartBeat& pkt, const sockaddr_in& clientAddr) {
    // 다른 패킷과 다르게, Addr이 연결 안된 상태로 HeartBeat가 먼저 올 수 있음. 즉 Addr의 port가 유효한지 먼저 확인하고 동작시켜야 함.
    
    External_Game_Protocol::D2CHeartBeat sendPkt;
    //SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CHeartBeat(sendPkt, pSession, clientAddr);
    //pDediServer->Send(sendBuffer, pSession->GetAddress());
    return true;
}

bool Handle_C2D_RequestBlueprint(PlayerSession* pSession, External_Game_Protocol::C2DRequestBlueprint& pkt, const sockaddr_in& clientAddr) {
    // 임시 더미 구현부 (빌드 통과용)
    return true;
}

/*
bool Handle_C2D_RequestSpawnMe(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnMe& pkt, const sockaddr_in& clientAddr) {
    // TODO: pSession 기반으로 스폰 오브젝트 목록 조회 후 채워넣기
    External_Game_Protocol::D2CResponseSpawnMe sendPkt;
    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseSpawnMe(sendPkt, pSession, clientAddr);
    pDediServer->Send(sendBuffer, pSession->GetAddress());
    return true;
}
*/