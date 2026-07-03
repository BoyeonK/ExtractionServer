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
    pPlayerObj->SetArmor(pSession->GetArmorSlot().item.blueprintId);
    pRoom->SpawnPlayerObject(pPlayerObj);
    pSession->SetObjectId(static_cast<int32_t>(objectId));
    spawnSpotPkt.set_object_id(objectId);

    pSession->Send(ClientPacketHandler::MakeD2CResponseSpawnMeSpawnSpotReliable(spawnSpotPkt, pSession));

    // 인벤토리 풀 싱크 전송
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

    if (pSession->GetInteractingContainerId() != -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    uint32_t containerId = pkt.container_object_id();
    UnityGameObject* pObj = pRoom->FindNonplayerObject(containerId);
    if (pObj == nullptr) return false;

    Container* pContainer = dynamic_cast<Container*>(pObj);
    if (pContainer == nullptr) return false;

    pSession->SetInteractingContainerId(static_cast<int32_t>(containerId));

    External_Game_Protocol::D2CResponseOpenContainer response;
    pContainer->SerializeOpenContainer(&response);
    pSession->Send(ClientPacketHandler::MakeD2CResponseOpenContainerReliable(response, pSession));
    return true;
}

bool Handle_C2D_CloseContainer(PlayerSession* pSession, External_Game_Protocol::C2DCloseContainer& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    if (pSession->GetInteractingContainerId() == -1) return false;

    pSession->SetInteractingContainerId(-1);
    return true;
}

static constexpr uint32_t PLAYER_OBJECT_ID_SENTINEL = 0xFFFFFFFF;

static void SendInteractContainerObjectDeny(PlayerSession* pSession, uint32_t denyMask) {
    External_Game_Protocol::D2CResponseInteractContainerObjectDeny deny;
    deny.set_deny_reason_mask(denyMask);
    pSession->Send(ClientPacketHandler::MakeD2CResponseInteractContainerObjectDenyReliable(deny, pSession));
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
        int32_t containerId = pSession->GetInteractingContainerId();
        if (containerId == -1) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 세션에 상호작용 중인 컨테이너 ID가 없음 (GetInteractingContainerId == -1)" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
        }

        uint32_t interactType = pkt.interact_type();
        if (interactType > 2) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] 유효하지 않은 interactType: " << interactType << " (허용 범위: 0~2)" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
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

        Container* pContainer = dynamic_cast<Container*>(pObj);
        if (pContainer == nullptr) {
            std::cout << "[Handle_C2D_RequestInteractContainerObject] containerId " << containerId << "의 오브젝트가 Container 타입이 아님" << std::endl;
            denyMask = DENY_SERVER_INTERNAL; break;
        }

        // start/end 오브젝트 해석 및 슬롯·버전 획득
        Slot* startSlot = nullptr;
        Slot* endSlot = nullptr;

        uint32_t startObjectId = pkt.start_object_id();
        uint32_t endObjectId = pkt.end_object_id();

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
                denyMask = DENY_SERVER_INTERNAL; break;
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
                denyMask = DENY_SERVER_INTERNAL; break;
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

        // ── 성공 ──
        bool startIsPlayer = (startObjectId == PLAYER_OBJECT_ID_SENTINEL);
        bool endIsPlayer = (endObjectId == PLAYER_OBJECT_ID_SENTINEL);

        if (startIsPlayer || endIsPlayer)
            pSession->GetInventoryMutable().IncrementInventoryVersion();
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

    // ── 실패: 거부 패킷 전송 ──
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

        // 외부 슬롯 획득
        Slot* pSlot = nullptr;
        Container* pContainer = nullptr;

        if (objectId == PLAYER_OBJECT_ID_SENTINEL) {
            PlayerInventory& inv = pSession->GetInventoryMutable();
            if (pkt.object_inventory_version() != inv.GetInventoryVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
            pSlot = inv.GetSlotMutable(static_cast<int32_t>(pkt.object_slot_idx()));
        } else {
            int32_t containerId = pSession->GetInteractingContainerId();
            if (containerId == -1) { denyMask = DENY_SERVER_INTERNAL; break; }
            if (objectId != static_cast<uint32_t>(containerId)) { denyMask = DENY_SERVER_INTERNAL; break; }

            GameRoom* pRoom = pSession->GetGameRoom();
            if (pRoom == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            UnityGameObject* pObj = pRoom->FindNonplayerObject(static_cast<uint32_t>(containerId));
            if (pObj == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            pContainer = dynamic_cast<Container*>(pObj);
            if (pContainer == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

            if (pkt.object_inventory_version() != pContainer->GetContainerVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
            pSlot = pContainer->GetSlotMutable(pkt.object_slot_idx());
        }

        if (pSlot == nullptr) { denyMask = DENY_SERVER_INTERNAL; break; }

        PlayerInventory& inv = pSession->GetInventoryMutable();

        // 소스가 컨테이너일 때: 플레이어 인벤토리 버전도 별도 검증
        if (objectId != PLAYER_OBJECT_ID_SENTINEL) {
            if (pkt.my_inventory_version() != inv.GetInventoryVersion()) { denyMask = DENY_VERSION_MISMATCH; break; }
        }

        bool isPrimary = (equipSlotType == 0);
        bool success = false;

        if (actionType == 0) { // equip
            if (equipSlotType <= 1)
                success = inv.EquipWeaponFromSlot(*pSlot, isPrimary, denyMask);
            else
                success = inv.EquipArmorFromSlot(*pSlot, denyMask);
        } else { // unequip
            if (equipSlotType <= 1)
                success = inv.UnequipWeaponToSlot(*pSlot, isPrimary, denyMask);
            else
                success = inv.UnequipArmorToSlot(*pSlot, denyMask);
        }

        if (!success) break;

        // ── 성공 ──
        if (pContainer != nullptr)
            pContainer->IncrementContainerVersion();

        // TODO : 플레이어의 장비 변화를 같은 방의 플레이어에게 브로드캐스팅하기

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

        pSession->Send(ClientPacketHandler::MakeD2CResponseEquipItemReliable(response, pSession));
        return true;

    } while (false);

    // ── 실패: 거부 패킷 전송 ──
    SendEquipItemDeny(pSession, denyMask);
    return false;
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