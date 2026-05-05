#include "ClientPacketHandler.h"

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include "../IoUringWrapper.h"
#include "../SendBuffer.h"
#include "DediSessions.h"
#include "GameRoom.h"

std::function<bool(PlayerSession*, unsigned char*, int32_t, const sockaddr_in&)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
    return false;
}

bool Handle_C2D_ChannelOpen(PlayerSession* pSession, External_Game_Protocol::C2DChannelOpen& pkt, const sockaddr_in& clientAddr) {
    if (pSession->GetSessionState() == PlayerSession::SessionState::INIT) { 
        uint16_t port = ntohs(clientAddr.sin_port);
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);

        std::cout << "매치 테스트 12 - O : 송신자 IP: " << ipStr << ", Port: " << port << std::endl;

        pSession->SetPort(port);
        pSession->SetSessionState(PlayerSession::SessionState::CONNECTED);
    }

    External_Game_Protocol::D2CResponseChannelOpen sendPkt;
    sendPkt.set_echo(pkt.echo());

    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseChannelOpenReliable(sendPkt, pSession);
    pSession->Send(sendBuffer);

    std::cout << "매치 테스트 13 - 에코 패킷 전송" << std::endl;

    return true;
}

bool Handle_C2D_HeartBeat(PlayerSession* pSession, External_Game_Protocol::C2DHeartBeat& pkt, const sockaddr_in& clientAddr) {
    std::cout << "하트비트" << std::endl;
    if (pSession->GetSessionState() != PlayerSession::SessionState::CONNECTED) return false;
    
    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CHeartBeat(External_Game_Protocol::D2CHeartBeat{}, pSession);
    pSession->Send(sendBuffer);
    return true;
}

bool Handle_C2D_RequestBlueprint(PlayerSession* pSession, External_Game_Protocol::C2DRequestBlueprint& pkt, const sockaddr_in& clientAddr) {
    std::cout << "매치 테스트 14 : C2DRequestBlueprint 수신 및 핸들러 함수 실행"<< std::endl;
    if (pSession->GetSessionState() != PlayerSession::SessionState::CONNECTED) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    std::vector<External_Game_Protocol::D2CResponseBlueprintStaticObjects> serializedStaticObjectsVec;

    pRoom->FillStaticObjects(serializedStaticObjectsVec);
    
    for (const auto& pkt:serializedStaticObjectsVec) {
        SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CResponseBlueprintStaticObjects(pkt, pSession);
        pSession->Send(sendBuffer);
    }
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