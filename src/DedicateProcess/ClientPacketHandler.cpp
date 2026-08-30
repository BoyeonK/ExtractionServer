#include "ClientPacketHandler.h"

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include "../IoUringWrapper.h"
#include "../SendBuffer.h"
#include "DediSessions.h"
#include "GameRoom.h"
#include "UnityGameObjects/PlayerObject.h"
#include "UnityGameObjects/Container.h"
#include "ItemDataManager.h"
#include "TimerExecuter.h"

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
    // 사망 유예 중에도 응답한다. 여기서 끊기면 클라이언트가 접속 끊김으로 판단해
    // 유예의 목적인 씬 유지가 깨진다. 상태를 바꾸지 않는 순수 에코라 안전
    if (!pSession->IsActiveState() && !pSession->IsSpectating()) return false;

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

    return true;
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

    // OPTION: 좌표를 그대로 받는다. 속도·텔레포트 검증을 붙이면 좌표 조작 클라이언트가
    //         막히고 귀환 존 판정도 실효를 얻는다
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

    uint32_t objectId = pRoom->GetNewObjectId();
    const auto& sp = spawnSpotPkt.spawn_point();
    PlayerObject* pPlayerObj = new PlayerObject(objectId, sp.x(), sp.y(), sp.z(),
                                                pSession->GetCharacterType(), pSession->GetUserId());
    pPlayerObj->SetWeapons(
        pSession->GetPrimaryWeapon().item.blueprintId,
        pSession->GetSecondaryWeapon().item.blueprintId
    );
    pPlayerObj->SetArmor(pSession->GetArmorSlot().item.blueprintId);
    pPlayerObj->ChargeShield(pPlayerObj->GetMaxShield());
    pRoom->SpawnPlayerObject(pPlayerObj, pSession->GetSessionId());
    pSession->SetObjectId(static_cast<int32_t>(objectId));
    spawnSpotPkt.set_object_id(objectId);

    pSession->Send(ClientPacketHandler::MakeD2CResponseSpawnMeSpawnSpotReliable(spawnSpotPkt, pSession));

    External_Game_Protocol::D2CFullInventorySync invSyncPkt;
    pSession->SerializeFullInventory(&invSyncPkt);
    pSession->Send(ClientPacketHandler::MakeD2CFullInventorySyncReliable(invSyncPkt, pSession));

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

bool Handle_C2D_RequestOpenContainer(PlayerSession* pSession, External_Game_Protocol::C2DRequestOpenContainer& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    uint32_t containerId = pkt.container_object_id();
    UnityGameObject* pObj = pRoom->FindNonplayerObject(containerId);
    if (pObj == nullptr) return false;

    Container* pContainer = dynamic_cast<Container*>(pObj);
    if (pContainer == nullptr) return false;

    int32_t playerObjectId = pSession->GetObjectId();
    if (playerObjectId == -1) return false;
    if (!pRoom->IsPlayerNearContainer(static_cast<uint32_t>(playerObjectId), containerId)) return false;

    const uint32_t holderId = pContainer->GetInteractingPlayerId();
    if (holderId != Container::NO_INTERACTING_PLAYER && holderId != static_cast<uint32_t>(playerObjectId)) {
        if (pRoom->IsPlayerNearContainer(holderId, containerId)) return false;

        // 점유자의 세션 상태도 함께 되돌린다. 남겨두면 그가 범위 안으로 돌아왔을 때
        // 두 명이 동시에 조작할 수 있게 된다
        if (PlayerSession* pHolder = pRoom->FindSessionByObjectId(static_cast<int32_t>(holderId)))
            pHolder->SetInteractingContainerId(-1);
    }

    // 앞선 점유는 여기서 푼다. 위 검사들보다 앞에 두면 열기에 실패한 요청이 열려 있던
    // 컨테이너까지 잃게 만든다
    pRoom->ReleaseInteractingContainer(pSession);

    pContainer->SetInteractingPlayerId(static_cast<uint32_t>(playerObjectId));
    pSession->SetInteractingContainerId(static_cast<int32_t>(containerId));

    External_Game_Protocol::D2CResponseOpenContainer response;
    pContainer->SerializeOpenContainer(&response);
    pSession->Send(ClientPacketHandler::MakeD2CResponseOpenContainerReliable(response, pSession));
    return true;
}

bool Handle_C2D_CloseContainer(PlayerSession* pSession, External_Game_Protocol::C2DCloseContainer& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    if (pSession->GetInteractingContainerId() == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) {
        pSession->SetInteractingContainerId(-1);
        return true;
    }

    pRoom->ReleaseInteractingContainer(pSession);
    return true;
}

static constexpr uint32_t PLAYER_OBJECT_ID_SENTINEL = 0xFFFFFFFF;

// 0 은 실재하는 인벤토리 버전(세션 시작값)이라 미설정 표시로 쓸 수 없다
static constexpr uint32_t INVENTORY_VERSION_NOT_SET = 0xFFFFFFFF;

// 수신 버퍼 1024B 에서 UDP 헤더 35B 와 여유분을 뺀 값 (GameRoom 의 청킹 한계와 같다)
static constexpr int32_t SAFE_PAYLOAD_LIMIT = 1024 - 45;

// 재장전 연출의 완료 단계. 중간 단계는 클라이언트가 보내지만 이 값만은 서버가 발행한다.
// 1B varint 에 담기는 마지막 값이라 클라이언트 단계 증설이 이 값을 밀지 않는다
static constexpr uint32_t RELOAD_SEQUENCE_COMPLETE = 15;

static void SendInteractContainerObjectDeny(PlayerSession* pSession, uint32_t denyMask) {
    External_Game_Protocol::D2CResponseInteractContainerObjectDeny deny;
    deny.set_deny_reason_mask(denyMask);
    pSession->Send(ClientPacketHandler::MakeD2CResponseInteractContainerObjectDenyReliable(deny, pSession));
}

static void FillWeaponChanged(External_Game_Protocol::D2CNotifyWeaponChanged* outPkt,
                              uint32_t objectId, const PlayerObject* pPlayerObj) {
    outPkt->set_object_id(objectId);
    outPkt->set_weapon_id(pPlayerObj->GetCurrentWeaponId());
    outPkt->set_slot(pPlayerObj->IsUsingPrimary() ? 0 : 1);
    outPkt->set_inventory_version(INVENTORY_VERSION_NOT_SET);
}

static void SendEquipItemDeny(PlayerSession* pSession, uint32_t denyMask) {
    External_Game_Protocol::D2CResponseEquipItemDeny deny;
    deny.set_deny_reason_mask(denyMask);
    pSession->Send(ClientPacketHandler::MakeD2CResponseEquipItemDenyReliable(deny, pSession));
}

bool Handle_C2D_RequestInteractContainerObject(PlayerSession* pSession, External_Game_Protocol::C2DRequestInteractContainerObject& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    uint32_t denyMask = 0;

    do {
        uint32_t interactType = pkt.interact_type();
        if (interactType > 2) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 유효하지 않은 interactType: " << interactType << " (허용 범위: 0~2)" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
        }

        uint32_t startObjectId = pkt.start_object_id();
        uint32_t endObjectId = pkt.end_object_id();

        int32_t    containerId = -1;
        Container* pContainer  = nullptr;

        // 양쪽이 모두 플레이어 인벤토리면 인벤토리 안에서의 정리라 컨테이너가 개입하지 않는다.
        // 열린 컨테이너를 요구하면 그 조작이 통째로 막힌다
        if (startObjectId != PLAYER_OBJECT_ID_SENTINEL || endObjectId != PLAYER_OBJECT_ID_SENTINEL) {
            containerId = pSession->GetInteractingContainerId();
            if (containerId == -1) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] 세션에 상호작용 중인 컨테이너 ID가 없음 (GetInteractingContainerId == -1)" << std::endl;
                denyMask = DENY_CONTAINER_NOT_OPEN; break;
            }

            GameRoom* pRoom = pSession->GetGameRoom();
            if (pRoom == nullptr) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] 세션이 속한 GameRoom이 없음 (GetGameRoom == nullptr)" << std::endl;
                denyMask = DENY_SERVER_INTERNAL; break;
            }

            UnityGameObject* pObj = pRoom->FindNonplayerObject(static_cast<uint32_t>(containerId));
            if (pObj == nullptr) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] containerId " << containerId << "에 해당하는 오브젝트를 룸에서 찾을 수 없음" << std::endl;
                denyMask = DENY_SERVER_INTERNAL; break;
            }

            pContainer = dynamic_cast<Container*>(pObj);
            if (pContainer == nullptr) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] containerId " << containerId << "의 오브젝트가 Container 타입이 아님" << std::endl;
                denyMask = DENY_SERVER_INTERNAL; break;
            }

            int32_t playerObjectId = pSession->GetObjectId();
            if (playerObjectId == -1) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] 세션에 플레이어 오브젝트가 없음 (GetObjectId == -1)" << std::endl;
                denyMask = DENY_SERVER_INTERNAL; break;
            }
            if (!pRoom->IsPlayerNearContainer(static_cast<uint32_t>(playerObjectId), static_cast<uint32_t>(containerId))) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] 컨테이너와의 거리가 상호작용 범위 밖 (containerId=" << containerId << ")" << std::endl;
                denyMask = DENY_OUT_OF_RANGE; break;
            }
        }

        Slot* startSlot = nullptr;
        Slot* endSlot = nullptr;

        if (startObjectId == PLAYER_OBJECT_ID_SENTINEL) {
            PlayerInventory& inv = pSession->GetInventoryMutable();
            if (pkt.start_object_inventory_version() != inv.GetInventoryVersion()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] start: 플레이어 인벤토리 버전 불일치 (클라이언트=" << pkt.start_object_inventory_version() << ", 서버=" << inv.GetInventoryVersion() << ")" << std::endl;
                denyMask = DENY_VERSION_MISMATCH; break;
            }
            startSlot = inv.GetSlotMutable(static_cast<int32_t>(pkt.start_object_slot_idx()));
        } else {
            if (startObjectId != static_cast<uint32_t>(containerId)) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] start: 오브젝트 ID 불일치 (패킷=" << startObjectId << ", 현재 컨테이너=" << containerId << ")" << std::endl;
                denyMask = DENY_CONTAINER_NOT_OPEN; break;
            }
            if (pkt.start_object_inventory_version() != pContainer->GetContainerVersion()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] start: 컨테이너 버전 불일치 (클라이언트=" << pkt.start_object_inventory_version() << ", 서버=" << pContainer->GetContainerVersion() << ")" << std::endl;
                denyMask = DENY_VERSION_MISMATCH; break;
            }
            startSlot = pContainer->GetSlotMutable(pkt.start_object_slot_idx());
        }

        if (endObjectId == PLAYER_OBJECT_ID_SENTINEL) {
            PlayerInventory& inv = pSession->GetInventoryMutable();
            if (pkt.end_object_inventory_version() != inv.GetInventoryVersion()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] end: 플레이어 인벤토리 버전 불일치 (클라이언트=" << pkt.end_object_inventory_version() << ", 서버=" << inv.GetInventoryVersion() << ")" << std::endl;
                denyMask = DENY_VERSION_MISMATCH; break;
            }
            endSlot = inv.GetSlotMutable(static_cast<int32_t>(pkt.end_object_slot_idx()));
        } else {
            if (endObjectId != static_cast<uint32_t>(containerId)) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] end: 오브젝트 ID 불일치 (패킷=" << endObjectId << ", 현재 컨테이너=" << containerId << ")" << std::endl;
                denyMask = DENY_CONTAINER_NOT_OPEN; break;
            }
            if (pkt.end_object_inventory_version() != pContainer->GetContainerVersion()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] end: 컨테이너 버전 불일치 (클라이언트=" << pkt.end_object_inventory_version() << ", 서버=" << pContainer->GetContainerVersion() << ")" << std::endl;
                denyMask = DENY_VERSION_MISMATCH; break;
            }
            endSlot = pContainer->GetSlotMutable(pkt.end_object_slot_idx());
        }

        if (startSlot == nullptr || endSlot == nullptr) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 슬롯 조회 실패 (startSlot=" << startSlot << ", endSlot=" << endSlot << ")" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
        }
        if (startSlot == endSlot) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 동일 슬롯 간 조작 불가 (objectId=" << startObjectId << ", slotIdx=" << pkt.start_object_slot_idx() << ")" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
        }
        if (startSlot->IsEmpty()) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 시작 슬롯(idx=" << pkt.start_object_slot_idx() << ")이 비어있음" << std::endl;
            denyMask = DENY_SLOT_EMPTY; break;
        }

        int32_t quantity = pkt.quantity();

        switch (interactType) {
        case 0: { // get: end가 비어있을 때만 quantity만큼 이동
            if (!endSlot->IsEmpty()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [get] 목적지 슬롯(idx=" << pkt.end_object_slot_idx() << ")이 비어있지 않음" << std::endl;
                denyMask = DENY_SLOT_NOT_EMPTY; break;
            }
            if (quantity <= 0 || quantity > startSlot->quantity) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [get] 유효하지 않은 수량 (요청=" << quantity << ", 보유=" << startSlot->quantity << ")" << std::endl;
                denyMask = DENY_INVALID_QUANTITY; break;
            }

            endSlot->item = startSlot->item;
            endSlot->quantity = quantity;

            startSlot->quantity -= quantity;
            if (startSlot->quantity <= 0)
                startSlot->Clear();
            break;
        }
        case 1: { // swap: 양쪽 모두 아이템이 있어야 하며, 통째로 교환
            if (endSlot->IsEmpty()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [swap] 목적지 슬롯(idx=" << pkt.end_object_slot_idx() << ")이 비어있어 교환 불가" << std::endl;
                denyMask = DENY_SLOT_ALREADY_EMPTY; break;
            }

            std::swap(startSlot->item, endSlot->item);
            std::swap(startSlot->quantity, endSlot->quantity);
            break;
        }
        case 2: { // merge: 동일 blueprintId일 때 quantity만큼 start→end 합산
            if (endSlot->IsEmpty()) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [merge] 목적지 슬롯(idx=" << pkt.end_object_slot_idx() << ")이 비어있어 합산 불가" << std::endl;
                denyMask = DENY_SLOT_ALREADY_EMPTY; break;
            }
            if (startSlot->item.blueprintId != endSlot->item.blueprintId) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [merge] blueprintId 불일치 (start=" << startSlot->item.blueprintId << ", end=" << endSlot->item.blueprintId << ")" << std::endl;
                denyMask = DENY_BLUEPRINT_MISMATCH; break;
            }
            ItemType itemType = ItemDataManager::GetType(startSlot->item.blueprintId);
            if (itemType == ItemType::WEAPON || itemType == ItemType::ARMOR) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [merge] 무기/방어구는 합산 불가 (blueprintId=" << startSlot->item.blueprintId << ")" << std::endl;
                denyMask = DENY_ITEM_TYPE_MISMATCH; break;
            }
            if (quantity <= 0 || quantity > startSlot->quantity) {
                std::cout << "[Handle_C2D_RequestInteractContainerObject] [merge] 유효하지 않은 수량 (요청=" << quantity << ", 보유=" << startSlot->quantity << ")" << std::endl;
                denyMask = DENY_INVALID_QUANTITY; break;
            }

            endSlot->quantity += quantity;

            startSlot->quantity -= quantity;
            if (startSlot->quantity <= 0)
                startSlot->Clear();
            break;
        }
        default:
            std::cout << "[Handle_C2D_RequestInteractContainerObject] switch 도달 불가 분기 (interactType=" << interactType << ")" << std::endl;
            denyMask = DENY_SERVER_INTERNAL;
            break;
        }
        if (denyMask != 0) break;

        bool startIsPlayer = (startObjectId == PLAYER_OBJECT_ID_SENTINEL);
        bool endIsPlayer = (endObjectId == PLAYER_OBJECT_ID_SENTINEL);

        if (startIsPlayer || endIsPlayer) {
            pSession->GetInventoryMutable().UpdateFirstEmptySlotIndex();
            pSession->GetInventoryMutable().IncrementInventoryVersion();
        }
        if (!startIsPlayer || !endIsPlayer)
            pContainer->IncrementContainerVersion();

        External_Game_Protocol::D2CResponseInteractContainerObject response;
        response.set_interact_type(interactType);
        response.set_start_object_id(pkt.start_object_id());
        response.set_start_object_inventory_version(
            startIsPlayer ? pSession->GetInventoryMutable().GetInventoryVersion() : pContainer->GetContainerVersion());
        response.set_start_object_slot_idx(pkt.start_object_slot_idx());
        response.set_quantity(quantity);
        response.set_end_object_id(pkt.end_object_id());
        response.set_end_object_inventory_version(
            endIsPlayer ? pSession->GetInventoryMutable().GetInventoryVersion() : pContainer->GetContainerVersion());
        response.set_end_object_slot_idx(pkt.end_object_slot_idx());

        pSession->Send(ClientPacketHandler::MakeD2CResponseInteractContainerObjectReliable(response, pSession));
        return true;

    } while (false);

    SendInteractContainerObjectDeny(pSession, denyMask);
    return false;
}

bool Handle_C2D_RequestEquipItem(PlayerSession* pSession, External_Game_Protocol::C2DRequestEquipItem& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    uint32_t denyMask = 0;

    do {
        uint32_t actionType = pkt.action_type();
        if (actionType > 1) { denyMask = DENY_SERVER_INTERNAL; break; }

        uint32_t equipSlotType = pkt.equipment_slot_type();
        if (equipSlotType > 2) { denyMask = DENY_SERVER_INTERNAL; break; }

        uint32_t objectId = pkt.object_id();

        Slot* pSlot = nullptr;
        Container* pContainer = nullptr;

        if (objectId == PLAYER_OBJECT_ID_SENTINEL) {
            PlayerInventory& inv = pSession->GetInventoryMutable();
            if (pkt.object_inventory_version() != inv.GetInventoryVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
            pSlot = inv.GetSlotMutable(static_cast<int32_t>(pkt.object_slot_idx()));
        } else {
            int32_t containerId = pSession->GetInteractingContainerId();
            if (containerId == -1) { denyMask = DENY_CONTAINER_NOT_OPEN; break; }
            if (objectId != static_cast<uint32_t>(containerId)) { denyMask = DENY_CONTAINER_NOT_OPEN; break; }

            GameRoom* pRoom = pSession->GetGameRoom();
            if (pRoom == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            UnityGameObject* pObj = pRoom->FindNonplayerObject(static_cast<uint32_t>(containerId));
            if (pObj == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            pContainer = dynamic_cast<Container*>(pObj);
            if (pContainer == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            int32_t playerObjectId = pSession->GetObjectId();
            if (playerObjectId == -1) { denyMask = DENY_SERVER_INTERNAL; break; }
            if (!pRoom->IsPlayerNearContainer(static_cast<uint32_t>(playerObjectId), static_cast<uint32_t>(containerId))) { denyMask = DENY_OUT_OF_RANGE; break; }

            if (pkt.object_inventory_version() != pContainer->GetContainerVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
            pSlot = pContainer->GetSlotMutable(pkt.object_slot_idx());
        }

        if (pSlot == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

        PlayerInventory& inv = pSession->GetInventoryMutable();

        if (objectId != PLAYER_OBJECT_ID_SENTINEL) {
            if (pkt.my_inventory_version() != inv.GetInventoryVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
        }

        bool isPrimary = (equipSlotType == 0);
        bool success = false;
        int32_t unloadedSlotIdx = -1;

        if (actionType == 0) { // equip
            if (equipSlotType <= 1)
                success = inv.EquipWeaponFromSlot(*pSlot, isPrimary, denyMask, unloadedSlotIdx);
            else
                success = inv.EquipArmorFromSlot(*pSlot, denyMask);
        } else { // unequip
            if (equipSlotType <= 1)
                success = inv.UnequipWeaponToSlot(*pSlot, isPrimary, denyMask, unloadedSlotIdx);
            else
                success = inv.UnequipArmorToSlot(*pSlot, denyMask);
        }

        if (!success) break;

        if (pContainer != nullptr)
            pContainer->IncrementContainerVersion();

        int32_t sessionObjectId = pSession->GetObjectId();
        GameRoom* pOwnerRoom = pSession->GetGameRoom();
        if (sessionObjectId != -1 && pOwnerRoom != nullptr) {
            PlayerObject* pPlayerObj = pOwnerRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
            // 방어구는 외형에 드러나지 않아 통보할 것이 없다
            if (pPlayerObj != nullptr) {
                if (equipSlotType == 2) {
                    pPlayerObj->SetArmor(inv.GetArmorSlot().item.blueprintId);
                } else {
                    pPlayerObj->SetWeapons(inv.GetPrimaryWeapon().item.blueprintId,
                                           inv.GetSecondaryWeapon().item.blueprintId);

                    External_Game_Protocol::D2CNotifyWeaponChanged notifyPkt;
                    FillWeaponChanged(&notifyPkt, static_cast<uint32_t>(sessionObjectId), pPlayerObj);

                    pOwnerRoom->BroadcastExcept(notifyPkt,
                                                ClientPacketHandler::MakeD2CNotifyWeaponChangedReliable,
                                                pSession->GetSessionId());
                }
            }
        }

        External_Game_Protocol::D2CResponseEquipItem response;
        response.set_action_type(actionType);
        response.set_equipment_slot_type(equipSlotType);
        response.set_object_id(pkt.object_id());
        response.set_object_inventory_version(
            (objectId == PLAYER_OBJECT_ID_SENTINEL)
                ? inv.GetInventoryVersion()
                : pContainer->GetContainerVersion());
        response.set_object_slot_idx(pkt.object_slot_idx());
        if (objectId != PLAYER_OBJECT_ID_SENTINEL)
            response.set_my_inventory_version(inv.GetInventoryVersion());

        if (unloadedSlotIdx != -1)
            inv.SerializeSlot(unloadedSlotIdx, response.mutable_unloaded_ammo_slot());

        pSession->Send(ClientPacketHandler::MakeD2CResponseEquipItemReliable(response, pSession));
        return true;

    } while (false);

    SendEquipItemDeny(pSession, denyMask);
    return false;
}

bool Handle_C2D_RequestSwitchWeapon(PlayerSession* pSession, External_Game_Protocol::C2DRequestSwitchWeapon& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    External_Game_Protocol::D2CNotifyWeaponChanged notifyPkt;
    uint32_t inventoryVersion = pSession->GetInventoryMutable().GetInventoryVersion();

    do {
        uint32_t targetSlot = pkt.target_slot();
        if (targetSlot > 1) {
            std::cout << "[Handle_C2D_RequestSwitchWeapon] 유효하지 않은 targetSlot: " << targetSlot << " (허용 범위: 0~1)" << std::endl;
            break;
        }

        if (pkt.my_inventory_version() != inventoryVersion) {
            std::cout << "[Handle_C2D_RequestSwitchWeapon] 인벤토리 버전 불일치 (클라이언트=" << pkt.my_inventory_version() << ", 서버=" << inventoryVersion << ")" << std::endl;
            break;
        }

        bool toPrimary = (targetSlot == 0);
        uint32_t targetWeaponId = toPrimary ? pPlayerObj->GetPrimaryWeaponId() : pPlayerObj->GetSecondaryWeaponId();
        if (targetWeaponId == 0) {
            std::cout << "[Handle_C2D_RequestSwitchWeapon] 대상 슬롯에 무기가 없음 (targetSlot=" << targetSlot << ")" << std::endl;
            break;
        }

        pPlayerObj->SetUsingPrimary(toPrimary);

        FillWeaponChanged(&notifyPkt, static_cast<uint32_t>(sessionObjectId), pPlayerObj);
        pRoom->BroadcastExcept(notifyPkt,
                               ClientPacketHandler::MakeD2CNotifyWeaponChangedReliable,
                               pSession->GetSessionId());

        // 요청자에게만 버전을 채워 따로 보낸다 — 남의 인벤토리 버전은 알려주지 않는다
        notifyPkt.set_inventory_version(inventoryVersion);
        pSession->Send(ClientPacketHandler::MakeD2CNotifyWeaponChangedReliable(notifyPkt, pSession));
        return true;

    } while (false);

    FillWeaponChanged(&notifyPkt, static_cast<uint32_t>(sessionObjectId), pPlayerObj);
    notifyPkt.set_inventory_version(inventoryVersion);
    pSession->Send(ClientPacketHandler::MakeD2CNotifyWeaponChangedReliable(notifyPkt, pSession));
    return false;
}

bool Handle_C2D_RequestWeaponFire(PlayerSession* pSession, External_Game_Protocol::C2DRequestWeaponFire& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    // 발사는 unreliable 이라 유실·재정렬로 시퀀스가 건너뛰는 것이 정상이다.
    // 되돌아온 것만 거부하고 기대값은 실제 받은 값 다음으로 점프시킨다.
    // 클라이언트가 번호를 되돌린 경우의 복구는 D2CResponseReload.fire_sequence 가 맡는다
    uint32_t expectedSeq = pSession->GetFireSequence();
    if (static_cast<int32_t>(pkt.fire_sequence() - expectedSeq) < 0) {
        std::cout << "[Handle_C2D_RequestWeaponFire] 낡은 fireSequence (클라이언트=" << pkt.fire_sequence() << ", 서버=" << expectedSeq << ")" << std::endl;
        return false;
    }
    pSession->SetNextFireSequence(pkt.fire_sequence() + 1);

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    if (pPlayerObj->GetCurrentWeaponId() != pkt.weapon_dbid()) {
        std::cout << "[Handle_C2D_RequestWeaponFire] weapon_dbid 불일치 (장착=" << pPlayerObj->GetCurrentWeaponId() << ", 패킷=" << pkt.weapon_dbid() << ")" << std::endl;
        return false;
    }

    PlayerInventory& inv = pSession->GetInventoryMutable();
    Slot& magazineSlot = pPlayerObj->IsUsingPrimary()
        ? inv.GetPrimaryWeaponMagazineMutable()
        : inv.GetSecondaryWeaponMagazineMutable();

    if (magazineSlot.IsEmpty() || magazineSlot.quantity <= 0) {
        std::cout << "[Handle_C2D_RequestWeaponFire] 탄약 부족" << std::endl;
        return false;
    }
    magazineSlot.quantity -= 1;

    // IsEmpty() 가 blueprintId 도 보므로, 비우지 않으면 0발 슬롯이 점유 상태로 남는다
    if (magazineSlot.quantity <= 0)
        magazineSlot.Clear();

    uint32_t hitObjectId = pkt.hit_object_id();
    if (hitObjectId != 0xFFFFFFFF) {
        PlayerObject* pHitPlayer = pRoom->FindPlayerObject(hitObjectId);
        CombatObject* pHitObject = pHitPlayer;
        if (pHitObject == nullptr)
            pHitObject = dynamic_cast<CombatObject*>(pRoom->FindNonplayerObject(hitObjectId));

        if (pHitObject != nullptr && pHitObject->IsAlive()) {
            const WeaponSpec* pSpec = ItemDataManager::GetWeaponSpec(pkt.weapon_dbid());
            if (pSpec != nullptr) {
                pHitObject->TakeDamage(pSpec->baseDamage, static_cast<uint32_t>(sessionObjectId));

                PlayerSession* pHitSession = pRoom->FindSessionByObjectId(static_cast<int32_t>(hitObjectId));
                if (pHitSession != nullptr && pHitSession->IsInplay()) {
                    External_Game_Protocol::D2CNotifyHealthChange healthPkt;
                    healthPkt.set_health_point(pHitObject->GetCurrentHp());
                    healthPkt.set_shield_point(pHitObject->GetCurrentShield());
                    healthPkt.set_reason(External_Game_Protocol::REASON_WEAPON_HIT);
                    healthPkt.set_attacker_object_id(static_cast<uint32_t>(sessionObjectId));

                    SendBuffer* buf = ClientPacketHandler::MakeD2CNotifyHealthChangeReliable(healthPkt, pHitSession);
                    if (buf != nullptr) {
                        if (!pHitObject->IsAlive())
                            pHitSession->SetLeaveNotifyRSeq(pHitSession->GetLastSentRSeq());

                        pHitSession->Send(buf);
                    }
                }

                // 플레이어의 사망은 이탈 경로가 회수한다. 여기서 지우면 위 포인터가 죽는다
                if (pHitPlayer == nullptr && pHitObject->IsDeathPending())
                    pRoom->DestroyDeadObject(hitObjectId);
            }
        }
    }

    External_Game_Protocol::D2CBroadcastWeaponFire broadcastPkt;
    broadcastPkt.set_shooter_object_id(static_cast<uint32_t>(sessionObjectId));
    if (pkt.has_hit_point()) {
        *broadcastPkt.mutable_hit_point() = pkt.hit_point();
    }

    pRoom->BroadcastExcept(broadcastPkt,
                           ClientPacketHandler::MakeD2CBroadcastWeaponFireUnreliable,
                           pSession->GetSessionId());

    return true;
}

static void BroadcastReloadSequence(PlayerSession* pSession, GameRoom* pRoom, uint32_t objectId, uint32_t sequenceNum) {
    External_Game_Protocol::D2CNotifyReloadSequence notifyPkt;
    notifyPkt.set_sequence_num(sequenceNum);
    notifyPkt.set_object_id(objectId);

    pRoom->BroadcastExcept(notifyPkt,
                           ClientPacketHandler::MakeD2CNotifyReloadSequenceUnreliable,
                           pSession->GetSessionId());
}

bool Handle_C2D_RequestReload(PlayerSession* pSession, External_Game_Protocol::C2DRequestReload& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    PlayerInventory& inv = pSession->GetInventoryMutable();

    External_Game_Protocol::D2CResponseReload response;
    uint32_t denyMask = 0;

    do {
        if (pkt.my_inventory_version() != inv.GetInventoryVersion()) {
            std::cout << "[Handle_C2D_RequestReload] 인벤토리 버전 불일치 (클라이언트=" << pkt.my_inventory_version()
                      << ", 서버=" << inv.GetInventoryVersion() << ")" << std::endl;
            denyMask |= DENY_VERSION_MISMATCH;
            break;
        }

        // 손에 든 무기가 없거나, 탄창이 이미 가득 찼거나, 해당 탄종이 인벤토리에 없는 경우
        if (!inv.ReloadMagazine(pPlayerObj->IsUsingPrimary())) {
            denyMask |= DENY_SLOT_EMPTY;
            break;
        }

        response.set_result(true);

    } while (false);

    response.set_deny_reason_mask(denyMask);
    response.set_fire_sequence(pSession->GetFireSequence());

    // 거부에도 싣는다 — 버전 불일치 거부는 이 스냅샷이 곧 재동기화라 별도 왕복이 필요 없다
    inv.SerializeFullInventory(response.mutable_inventory());

    // 25칸이 전부 찬 최악에도 한계의 80% 선이지만, 넘으면 수신 측에서 조용히 잘린다
    if (static_cast<int32_t>(response.ByteSizeLong()) > SAFE_PAYLOAD_LIMIT) {
        std::cout << "[Handle_C2D_RequestReload] 응답이 안전 페이로드 한계를 넘었다 (크기="
                  << response.ByteSizeLong() << ", 한계=" << SAFE_PAYLOAD_LIMIT << ")" << std::endl;
    }

    if (denyMask == 0)
        BroadcastReloadSequence(pSession, pRoom, static_cast<uint32_t>(sessionObjectId), RELOAD_SEQUENCE_COMPLETE);

    pSession->Send(ClientPacketHandler::MakeD2CResponseReloadReliable(response, pSession));
    return denyMask == 0;
}

bool Handle_C2D_NotifyReloadSequence(PlayerSession* pSession, External_Game_Protocol::C2DNotifyReloadSequence& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    // 완료 단계는 재장전이 실제로 일어났는지를 아는 서버만 발행한다
    if (pkt.sequence_num() == RELOAD_SEQUENCE_COMPLETE) {
        std::cout << "[Handle_C2D_NotifyReloadSequence] 서버 전용 완료 단계를 클라이언트가 보냈다" << std::endl;
        return false;
    }

    // 맨손은 재장전 연출이 성립하지 않는다
    if (pPlayerObj->GetCurrentWeaponId() == 0) return false;

    BroadcastReloadSequence(pSession, pRoom, static_cast<uint32_t>(sessionObjectId), pkt.sequence_num());
    return true;
}

static void RecallTick(int32_t sessionId, int32_t uid, uint32_t generation);

static void ScheduleRecallTick(int32_t sessionId, int32_t uid, uint32_t generation) {
    pTimerExecuter->Add(PlayerSession::RECALL_TICK_INTERVAL_MS, [sessionId, uid, generation]() {
        RecallTick(sessionId, uid, generation);
    });
}

static uint32_t SendRecallResult(PlayerSession* pSession, bool result, uint32_t spotIndex,
                                 External_Game_Protocol::RecallResultReason reason) {
    External_Game_Protocol::D2CNotifyRecallResult pkt;
    pkt.set_result(result);
    pkt.set_recall_spot_index(spotIndex);
    pkt.set_reason(reason);

    SendBuffer* pBuffer = ClientPacketHandler::MakeD2CNotifyRecallResultReliable(pkt, pSession);
    if (pBuffer == nullptr) return 0;

    const uint32_t notifyRSeq = pSession->GetLastSentRSeq();
    pSession->Send(pBuffer);
    return notifyRSeq;
}

static void CancelRecall(PlayerSession* pSession, uint32_t spotIndex,
                         External_Game_Protocol::RecallResultReason reason) {
    pSession->EndRecall();

    std::cout << "[RecallTick] 귀환 취소 (sessionId=" << pSession->GetSessionId()
              << ", spotIndex=" << spotIndex
              << ", reason=" << static_cast<int32_t>(reason) << ")" << std::endl;

    if (!pSession->IsActiveState()) return;
    SendRecallResult(pSession, false, spotIndex, reason);
}

static void RecallTick(int32_t sessionId, int32_t uid, uint32_t generation) {
    PlayerSession* pSession = pDediServer->GetPlayerSession(static_cast<int16_t>(sessionId));
    if (pSession == nullptr) return;

    if (!pSession->IsRecalling() || pSession->GetRecallGeneration() != generation) return;
    if (pSession->GetUid() != uid) return;

    const uint32_t spotIndex = pSession->GetRecallSpotIndex();

    if (!pSession->IsInplay()) {
        CancelRecall(pSession, spotIndex, External_Game_Protocol::RECALL_RESULT_SESSION_LOST);
        return;
    }

    if (pSession->IsLeaving()) {
        CancelRecall(pSession, spotIndex, External_Game_Protocol::RECALL_RESULT_SESSION_LOST);
        return;
    }

    const int32_t objectId = pSession->GetObjectId();
    GameRoom*     pRoom    = pSession->GetGameRoom();

    PlayerObject* pPlayerObj = (pRoom != nullptr && objectId != -1)
        ? pRoom->FindPlayerObject(static_cast<uint32_t>(objectId))
        : nullptr;

    if (pPlayerObj == nullptr) {
        CancelRecall(pSession, spotIndex, External_Game_Protocol::RECALL_RESULT_SERVER_INTERNAL);
        return;
    }

    if (!pPlayerObj->IsAlive()) {
        CancelRecall(pSession, spotIndex, External_Game_Protocol::RECALL_RESULT_PLAYER_DEAD);
        return;
    }

    if (!pRoom->IsInRecallZone(spotIndex, pPlayerObj->position)) {
        CancelRecall(pSession, spotIndex, External_Game_Protocol::RECALL_RESULT_OUT_OF_ZONE);
        return;
    }

    if (pSession->AddRecallPass() < PlayerSession::RECALL_REQUIRED_PASS_COUNT) {
        ScheduleRecallTick(sessionId, uid, generation);
        return;
    }

    pSession->EndRecall();
    std::cout << "[RecallTick] 귀환 성공 (sessionId=" << sessionId
              << ", spotIndex=" << spotIndex << ")" << std::endl;

    const uint32_t notifyRSeq =
        SendRecallResult(pSession, true, spotIndex, External_Game_Protocol::RECALL_RESULT_SUCCESS);

    pSession->MarkLeaving(PlayerSession::LeaveReason::RECALLED, notifyRSeq);
}

bool Handle_C2D_RequestRecall(PlayerSession* pSession, External_Game_Protocol::C2DRequestRecall& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    if (pSession->IsLeaving()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    if (pSession->IsRecalling()) {
        std::cout << "[Handle_C2D_RequestRecall] 진행 중인 귀환이 있어 요청 무시 (objectId="
                  << sessionObjectId << ")" << std::endl;
        return true;
    }

    const uint32_t recallSpotIndex = pkt.recall_spot_index();

    const bool result = pRoom->IsInRecallZone(recallSpotIndex, pPlayerObj->position);

    if (!result) {
        std::cout << "[Handle_C2D_RequestRecall] 귀환 거부 (objectId=" << sessionObjectId
                  << ", spotIndex=" << recallSpotIndex << ")" << std::endl;
    }

    External_Game_Protocol::D2CResponseRecall response;
    response.set_result(result);
    response.set_recall_spot_index(recallSpotIndex);

    pSession->Send(ClientPacketHandler::MakeD2CResponseRecallReliable(response, pSession));

    if (!result) return true;

    pSession->BeginRecall(recallSpotIndex);
    ScheduleRecallTick(pSession->GetSessionId(), pSession->GetUid(), pSession->GetRecallGeneration());

    return true;
}

bool Handle_C2D_RequestRecentInventoryInfo(PlayerSession* pSession, External_Game_Protocol::C2DRequestRecentInventoryInfo& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    uint32_t objectId = pkt.object_id();

    if (objectId == PLAYER_OBJECT_ID_SENTINEL) {
        External_Game_Protocol::D2CFullInventorySync response;
        pSession->SerializeFullInventory(&response);
        pSession->Send(ClientPacketHandler::MakeD2CFullInventorySyncReliable(response, pSession));
        return true;
    }

    int32_t containerId = pSession->GetInteractingContainerId();
    if (containerId == -1) return false;
    if (objectId != static_cast<uint32_t>(containerId)) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    UnityGameObject* pObj = pRoom->FindNonplayerObject(objectId);
    if (pObj == nullptr) return false;

    Container* pContainer = dynamic_cast<Container*>(pObj);
    if (pContainer == nullptr) return false;

    External_Game_Protocol::D2CResponseRecentContainerInfo response;
    pContainer->SerializeRecentContainerInfo(&response);
    pSession->Send(ClientPacketHandler::MakeD2CResponseRecentContainerInfoReliable(response, pSession));
    return true;
}