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
    pPlayerObj->ChargeShield(pPlayerObj->GetMaxShield());
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

        // ── 성공 ──
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

        // armor 슬롯 변경이면 PlayerObject에 shield 스탯 반영
        if (equipSlotType == 2) {
            int32_t sessionObjectId = pSession->GetObjectId();
            if (sessionObjectId != -1) {
                GameRoom* pRoom = pSession->GetGameRoom();
                if (pRoom) {
                    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
                    if (pPlayerObj) {
                        pPlayerObj->SetArmor(inv.GetArmorSlot().item.blueprintId);
                    }
                }
            }
        }

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

bool Handle_C2D_RequestWeaponFire(PlayerSession* pSession, External_Game_Protocol::C2DRequestWeaponFire& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    // fireSequence 검증
    uint32_t expectedSeq = pSession->GetFireSequence();
    if (pkt.fire_sequence() != expectedSeq) {
        std::cout << "[Handle_C2D_RequestWeaponFire] fireSequence 불일치 (클라이언트=" << pkt.fire_sequence() << ", 서버=" << expectedSeq << ")" << std::endl;
        return false;
    }
    pSession->IncrementFireSequence();

    // weapon_dbid 검증: 플레이어가 실제 장착 중인 총기인지 확인
    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    if (pPlayerObj->GetCurrentWeaponId() != pkt.weapon_dbid()) {
        std::cout << "[Handle_C2D_RequestWeaponFire] weapon_dbid 불일치 (장착=" << pPlayerObj->GetCurrentWeaponId() << ", 패킷=" << pkt.weapon_dbid() << ")" << std::endl;
        return false;
    }

    // 탄약 차감: 현재 사용 중인 무기의 magazine에서 1발 차감 (테스트를 위해 임시 비활성화)
    // PlayerInventory& inv = pSession->GetInventoryMutable();
    // Slot& magazineSlot = pPlayerObj->IsUsingPrimary()
    //     ? inv.GetPrimaryWeaponMagazineMutable()
    //     : inv.GetSecondaryWeaponMagazineMutable();
    //
    // if (magazineSlot.IsEmpty() || magazineSlot.quantity <= 0) {
    //     std::cout << "[Handle_C2D_RequestWeaponFire] 탄약 부족" << std::endl;
    //     return false;
    // }
    // magazineSlot.quantity -= 1;

    // 피격 대상 데미지 처리
    uint32_t hitObjectId = pkt.hit_object_id();
    if (hitObjectId != 0xFFFFFFFF) {
        PlayerObject* pHitPlayer = pRoom->FindPlayerObject(hitObjectId);
        if (pHitPlayer != nullptr && pHitPlayer->IsAlive()) {
            const WeaponSpec* pSpec = ItemDataManager::GetWeaponSpec(pkt.weapon_dbid());
            if (pSpec != nullptr) {
                pHitPlayer->TakeDamage(pSpec->baseDamage);

                // 피격 대상에게 HP/쉴드 변화 통보
                for (auto& [id, pHitSession] : pRoom->GetPlayerSessions()) {
                    if (pHitSession == nullptr || !pHitSession->IsInplay()) continue;
                    if (pHitSession->GetObjectId() != static_cast<int32_t>(hitObjectId)) continue;

                    External_Game_Protocol::D2CNotifyHealthChange healthPkt;
                    healthPkt.set_health_point(pHitPlayer->GetCurrentHp());
                    healthPkt.set_shield_point(pHitPlayer->GetCurrentShield());
                    healthPkt.set_reason(External_Game_Protocol::REASON_WEAPON_HIT);

                    SendBuffer* buf = ClientPacketHandler::MakeD2CNotifyHealthChangeReliable(healthPkt, pHitSession);
                    if (buf != nullptr)
                        pHitSession->Send(buf);
                    break;
                }
            }
        }
    }

    // 발사자를 제외한 다른 플레이어에게 브로드캐스트
    External_Game_Protocol::D2CBroadcastWeaponFire broadcastPkt;
    broadcastPkt.set_shooter_object_id(static_cast<uint32_t>(sessionObjectId));
    if (pkt.has_hit_point()) {
        *broadcastPkt.mutable_hit_point() = pkt.hit_point();
    }

    for (auto& [id, pOtherSession] : pRoom->GetPlayerSessions()) {
        if (pOtherSession == nullptr || !pOtherSession->IsInplay()) continue;
        if (pOtherSession->GetSessionId() == pSession->GetSessionId()) continue;

        SendBuffer* buf = ClientPacketHandler::MakeD2CBroadcastWeaponFireUnreliable(broadcastPkt, pOtherSession);
        if (buf != nullptr)
            pOtherSession->Send(buf);
    }

    return true;
}

// ── 귀환(탈출) 진행 ──────────────────────────────────────────────────────────
// 승인된 귀환은 1초 간격으로 위치를 RECALL_REQUIRED_PASS_COUNT 회 재검사하고,
// 전부 통과하면(= 약 5초 뒤) 확정된다. 한 번이라도 실패하면 그 시점에 취소.
//
// TimerExecuter 에는 취소 API 가 없으므로, 취소·완료된 귀환의 잔여 콜백은 세션의
// 귀환 세대(generation)와 대조해 스스로 포기한다.
// 콜백은 최대 5초 뒤에 실행되므로 raw PlayerSession*/PlayerObject* 를 캡처하지 않고
// sessionId 로 매번 재조회한다 (uid 는 세션 슬롯이 재사용된 경우를 걸러내기 위한 확인용).
static void RecallTick(int32_t sessionId, int32_t uid, uint32_t generation);

static void ScheduleRecallTick(int32_t sessionId, int32_t uid, uint32_t generation) {
    pTimerExecuter->Add(PlayerSession::RECALL_TICK_INTERVAL_MS, [sessionId, uid, generation]() {
        RecallTick(sessionId, uid, generation);
    });
}

static void SendRecallResult(PlayerSession* pSession, bool result, uint32_t spotIndex,
                             External_Game_Protocol::RecallResultReason reason) {
    External_Game_Protocol::D2CNotifyRecallResult pkt;
    pkt.set_result(result);
    pkt.set_recall_spot_index(spotIndex);
    pkt.set_reason(reason);

    pSession->Send(ClientPacketHandler::MakeD2CNotifyRecallResultReliable(pkt, pSession));
}

// 진행 중인 귀환을 취소하고 사유를 통보한다.
// 세션이 이미 전송 불가 상태면 상태만 정리하고 조용히 끝낸다.
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

    // 이미 끝났거나 취소된 귀환의 잔여 콜백이면 여기서 스스로 포기한다
    if (!pSession->IsRecalling() || pSession->GetRecallGeneration() != generation) return;
    if (pSession->GetUid() != uid) return;

    const uint32_t spotIndex = pSession->GetRecallSpotIndex();

    // 게임에서 빠진 세션 (연결 종료 등) — 귀환 성립 불가
    if (!pSession->IsInplay()) {
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

    // 이번 검사 통과 — 아직 목표 횟수에 못 미치면 다음 검사를 예약한다
    if (pSession->AddRecallPass() < PlayerSession::RECALL_REQUIRED_PASS_COUNT) {
        ScheduleRecallTick(sessionId, uid, generation);
        return;
    }

    // 전 구간 통과 — 귀환 확정
    pSession->EndRecall();
    std::cout << "[RecallTick] 귀환 성공 (sessionId=" << sessionId
              << ", spotIndex=" << spotIndex << ")" << std::endl;
    SendRecallResult(pSession, true, spotIndex, External_Game_Protocol::RECALL_RESULT_SUCCESS);

    // TODO : 실제 귀환 처리 (세션 INPLAY 해제, PlayerObject 제거 및 퇴장 브로드캐스트,
    //        인벤토리 반출 확정 및 DB 반영) 는 별도 작업으로 진행 예정.
    //        D2CNotifyRecallResult 는 reliable 이라 재전송 큐가 세션에 붙으므로,
    //        세션 정리는 이 패킷이 ACK 된 뒤에 수행해야 결과가 유실되지 않는다.
    //        정리가 구현되기 전까지는 귀환 성공 후에도 플레이어가 룸에 남아 있어
    //        재귀환 요청이 다시 승인될 수 있다.
}

bool Handle_C2D_RequestRecall(PlayerSession* pSession, External_Game_Protocol::C2DRequestRecall& pkt, const sockaddr_in& clientAddr) {
    if (!pSession->IsActiveState()) return false;

    int32_t sessionObjectId = pSession->GetObjectId();
    if (sessionObjectId == -1) return false;

    GameRoom* pRoom = pSession->GetGameRoom();
    if (pRoom == nullptr) return false;

    PlayerObject* pPlayerObj = pRoom->FindPlayerObject(static_cast<uint32_t>(sessionObjectId));
    if (pPlayerObj == nullptr) return false;

    // 이미 진행 중인 귀환이 있으면 중복 요청은 무시한다.
    // 진행 중인 귀환의 성공/취소는 D2CNotifyRecallResult 로 따로 통보되고,
    // 승인 응답(D2CResponseRecall)은 reliable 이라 유실되어도 재전송으로 복구되므로
    // 여기서 응답을 다시 만들 필요가 없다.
    if (pSession->IsRecalling()) {
        std::cout << "[Handle_C2D_RequestRecall] 진행 중인 귀환이 있어 요청 무시 (objectId="
                  << sessionObjectId << ")" << std::endl;
        return true;
    }

    const uint32_t recallSpotIndex = pkt.recall_spot_index();

    // 인덱스 범위 밖이거나 해당 영역 안에 없으면 거부.
    // 주의 : 검사 실패는 return false 가 아니다 — 거부도 응답을 보내야 하므로
    //        결과를 담아 전송한 뒤 true 를 반환한다.
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

    // 승인 — 1초 간격 위치 재검사 시작.
    // RECALL_REQUIRED_PASS_COUNT 회를 모두 통과하면(= 약 5초 뒤) 귀환이 확정되고,
    // 그 결과는 D2CNotifyRecallResult 로 통보된다.
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