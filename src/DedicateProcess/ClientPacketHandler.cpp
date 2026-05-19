#include "ClientPacketHandler.h"

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include "../IoUringWrapper.h"
#include "../SendBuffer.h"
#include "DediSessions.h"
#include "GameRoom.h"
#include "UnityGameObjects/PlayerObject.h"

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
    if (!pSession->IsActiveState()) return false;

    SendBuffer* sendBuffer = ClientPacketHandler::MakeD2CHeartBeat(External_Game_Protocol::D2CHeartBeat{}, pSession);
    pSession->Send(sendBuffer);
    return true;
}

bool Handle_C2D_RequestBlueprint(PlayerSession* pSession, External_Game_Protocol::C2DRequestBlueprint& pkt, const sockaddr_in& clientAddr) {
    std::cout << "매치 테스트 14 : C2DRequestBlueprint 수신 및 핸들러 함수 실행"<< std::endl;
    if (!pSession->IsActiveState()) return false;

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

bool Handle_C2D_RequestSpawnByObjectId(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnByObjectId& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;
    if (pSession->GetObjectId() == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    if (pkt.object_id() < 0) return false;

    const uint32_t objectId = static_cast<uint32_t>(pkt.object_id());

    if (UnityGameObject* pObj = pRoom->FindNonplayerObject(objectId)) {
        External_Game_Protocol::D2CResponseSpawnByObjectId response;
        pObj->Serialize(response.mutable_game_object());
        pSession->Send(ClientPacketHandler::MakeD2CResponseSpawnByObjectIdReliable(response, pSession));
        return true;
    }

    if (PlayerObject* pPlayerObj = pRoom->FindPlayerObject(objectId)) {
        External_Game_Protocol::D2CSpawnPlayerObject response;
        response.set_character_type(pPlayerObj->GetCharacterType());
        response.set_weapon_id(pPlayerObj->GetCurrentWeaponId());
        pPlayerObj->Serialize(response.mutable_game_object());
        pSession->Send(ClientPacketHandler::MakeD2CSpawnPlayerObjectReliable(response, pSession));
        return true;
    }

    return true; // 오브젝트 없음 — ACK 처리
}

bool Handle_C2D_UpdatePlayerState(PlayerSession* pSession, External_Game_Protocol::C2DUpdatePlayerState& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    const auto& state = pkt.state();

    if (!state.has_movement_info()) return false;
    if (state.movement_info().object_id() != static_cast<uint32_t>(sessionObjectId)) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    pPlayerObj->ApplyState(state);

    return true;
}

bool Handle_C2D_RequestSpawnMe(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnMe& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    External_Game_Protocol::D2CResponseSpawnMeSpawnSpot spawnSpotPkt;
    pRoom->SetSpawnSpot(&spawnSpotPkt);
    spawnSpotPkt.set_character_type(pSession->GetCharacterType());

    // PlayerObject 생성 및 GameRoom 등록
    uint32_t objectId = pRoom->GetNewObjectId();
    const auto& sp = spawnSpotPkt.spawn_point();
    PlayerObject* pPlayerObj = new PlayerObject(objectId, sp.x(), sp.y(), sp.z(), pSession->GetCharacterType());
    pPlayerObj->SetWeapons(
        pSession->GetPrimaryWeapon().item.blueprintId,
        pSession->GetSecondaryWeapon().item.blueprintId
    );
    pRoom->SpawnPlayerObject(pPlayerObj);
    pSession->SetObjectId(static_cast<int32_t>(objectId));
    spawnSpotPkt.set_object_id(objectId);

    pSession->Send(ClientPacketHandler::MakeD2CResponseSpawnMeSpawnSpotReliable(spawnSpotPkt, pSession));

    std::vector<External_Game_Protocol::D2CResponseSpawnMeDynamicObjects> dynamicObjectsVec;
    pRoom->FillDynamicObjects(dynamicObjectsVec);
    for (const auto& dynPkt : dynamicObjectsVec) {
        pSession->Send(ClientPacketHandler::MakeD2CResponseSpawnMeDynamicObjectsReliable(dynPkt, pSession));
    }

    return true;
}

bool Handle_C2D_RequestSpawnPlayerObjects(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnPlayerObjects& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;
    if (pSession->GetObjectId() == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    External_Game_Protocol::D2CSpawnPlayerObjects responsePkt;
    pRoom->FillPlayerObjects(responsePkt);

    pSession->Send(ClientPacketHandler::MakeD2CSpawnPlayerObjectsReliable(responsePkt, pSession));
    return true;
}

bool Handle_C2D_NotifyLoadingComplete(PlayerSession* pSession, External_Game_Protocol::C2DNotifyLoadingComplete& pkt, const sockaddr_in& clientAddr) {
    if (pSession->GetSessionState() != PlayerSession::SessionState::CONNECTED)
        return false;

    pSession->SetSessionState(PlayerSession::SessionState::INPLAY);
    return true;
}