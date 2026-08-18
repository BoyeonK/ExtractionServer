#include "GameRoom.h"

#include "../ObjectPool.h"
#include "PlayerSession.h"
#include "DedicateGlobalVariable.h"
#include "DediServerService.h"
#include "UnityGameObjects/TestGameObjects.h"
#include "UnityGameObjects/CorpseContainer.h"
#include "ClientPacketHandler.h"

static_assert(static_cast<int32_t>(GameRoom::MAP_TUTORIAL)   == static_cast<int32_t>(MapDataManager::MAP_ID_TUTORIAL),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_TUTORIAL");
static_assert(static_cast<int32_t>(GameRoom::MAP_WINCHESTER) == static_cast<int32_t>(MapDataManager::MAP_ID_WINCHESTER),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_WINCHESTER");

GameRoom::GameRoom(int32_t roomId, int32_t mapId) : _roomId(roomId), _mapId(mapId) {
    _pRecallZones = MapDataManager::GetRecallZones(mapId, _recallZoneCount);
}

GameRoom::~GameRoom() {
    for (auto& [objectId, pObject] : _staticObjects)  delete pObject;
    for (auto& [objectId, pObject] : _dynamicObjects) delete pObject;
    for (auto& [objectId, pObject] : _playerObjects)  delete pObject;
}

const RecallZone* GameRoom::GetRecallZone(uint32_t index) const {
    if (_pRecallZones == nullptr || index >= _recallZoneCount) return nullptr;
    return &_pRecallZones[index];
}

bool GameRoom::IsInRecallZone(uint32_t index, const Vector3& pos) const {
    const RecallZone* pZone = GetRecallZone(index);
    if (pZone == nullptr) return false;
    return pZone->Contains(pos);
}

void GameRoom::RegisterPlayerSession(PlayerSession* pSession) {
    if (pSession == nullptr) return;

    int32_t sessionId = pSession->GetSessionId(); 
    _playerSessions.insert({sessionId, pSession});
}

void GameRoom::FillStaticObjects(std::vector<External_Game_Protocol::D2CResponseBlueprintStaticObjects>& outVec) {
    if (_staticObjects.empty()) {
        External_Game_Protocol::D2CResponseBlueprintStaticObjects emptyPkt;
        emptyPkt.set_index(0);
        emptyPkt.set_is_last(true);
        outVec.push_back(std::move(emptyPkt));
        return;
    }
    
    // 헤더 35, 안전빵 10
    const int32_t SAFE_PAYLOAD_LIMIT = 1024 - 45;

    outVec.reserve((_staticObjects.size() / 10) + 1);

    External_Game_Protocol::D2CResponseBlueprintStaticObjects current;
    int32_t currentPayloadSize = 0;
    uint32_t index = 0;

    for (auto& [id, pObj] : _staticObjects) {
        if (pObj == nullptr) continue;

        External_Game_Protocol::UnityGameObject pbObj = pObj->Serialize();
        
        int32_t itemSize = pbObj.ByteSizeLong() + 5;

        if (currentPayloadSize + itemSize > SAFE_PAYLOAD_LIMIT) {
            current.set_index(index++);
            current.set_is_last(false);
            outVec.push_back(std::move(current));
            
            current = External_Game_Protocol::D2CResponseBlueprintStaticObjects();
            currentPayloadSize = 0;
        }

        *current.add_ingame_objects() = std::move(pbObj);
        currentPayloadSize += itemSize;
    }

    if (currentPayloadSize > 0) {
        current.set_index(index);
        current.set_is_last(true);
        outVec.push_back(std::move(current));
    } else if (outVec.empty()) {
        current.set_index(0);
        current.set_is_last(true);
        outVec.push_back(std::move(current));
    }
}

void GameRoom::FillDynamicObjects(std::vector<External_Game_Protocol::D2CResponseSpawnMeDynamicObjects>& outVec) {
    if (_dynamicObjects.empty()) {
        External_Game_Protocol::D2CResponseSpawnMeDynamicObjects emptyPkt;
        emptyPkt.set_index(0);
        emptyPkt.set_is_last(true);
        outVec.push_back(std::move(emptyPkt));
        return;
    }

    // 헤더 35, 안전빵 10
    const int32_t SAFE_PAYLOAD_LIMIT = 1024 - 45;

    outVec.reserve((_dynamicObjects.size() / 10) + 1);

    External_Game_Protocol::D2CResponseSpawnMeDynamicObjects current;
    int32_t currentPayloadSize = 0;
    uint32_t index = 0;

    for (auto& [id, pObj] : _dynamicObjects) {
        if (pObj == nullptr) continue;

        External_Game_Protocol::UnityGameObject pbObj = pObj->Serialize();
        int32_t itemSize = pbObj.ByteSizeLong() + 5;

        if (currentPayloadSize + itemSize > SAFE_PAYLOAD_LIMIT) {
            current.set_index(index++);
            current.set_is_last(false);
            outVec.push_back(std::move(current));

            current = External_Game_Protocol::D2CResponseSpawnMeDynamicObjects();
            currentPayloadSize = 0;
        }

        *current.add_ingame_objects() = std::move(pbObj);
        currentPayloadSize += itemSize;
    }

    if (currentPayloadSize > 0) {
        current.set_index(index);
        current.set_is_last(true);
        outVec.push_back(std::move(current));
    } else if (outVec.empty()) {
        current.set_index(0);
        current.set_is_last(true);
        outVec.push_back(std::move(current));
    }
}

void GameRoom::FillPlayerObjects(External_Game_Protocol::D2CSpawnPlayerObjects& outPkt) {
    for (auto& [id, pObj] : _playerObjects) {
        if (pObj == nullptr) continue;
        External_Game_Protocol::D2CSpawnPlayerObject* pEntry = outPkt.add_players();
        pEntry->set_character_type(pObj->GetCharacterType());
        pEntry->set_weapon_id(pObj->GetCurrentWeaponId());
        *pEntry->mutable_game_object() = pObj->Serialize();
    }
}

void GameRoom::FillPlayerStates(External_Game_Protocol::D2CUpdatePlayerStates& outPkt) {
    for (auto& [id, pObj] : _playerObjects) {
        if (pObj == nullptr) continue;
        pObj->FillState(outPkt.add_player_states());
    }
}

void GameRoom::BroadcastPlayerStates() {
    if (_playerObjects.empty()) return;

    External_Game_Protocol::D2CUpdatePlayerStates pkt;
    FillPlayerStates(pkt);

    Broadcast(pkt, ClientPacketHandler::MakeD2CUpdatePlayerStatesUnreliable);
}

void GameRoom::Update() {
    BroadcastPlayerStates();
}

PlayerSession* GameRoom::GetPlayerSession(int32_t sessionId) {
    auto it = _playerSessions.find(sessionId);
    if (it != _playerSessions.end()) {
        return it->second;
    }
    return nullptr;
}

UnityGameObject* GameRoom::FindNonplayerObject(uint32_t objectId) const {
    auto it = _staticObjects.find(objectId);
    if (it != _staticObjects.end()) return it->second;

    it = _dynamicObjects.find(objectId);
    if (it != _dynamicObjects.end()) return it->second;

    return nullptr;
}

PlayerObject* GameRoom::FindPlayerObject(uint32_t objectId) const {
    auto it = _playerObjects.find(objectId);
    if (it != _playerObjects.end()) return it->second;
    return nullptr;
}

PlayerSession* GameRoom::FindSessionByObjectId(int32_t objectId) const {
    if (objectId == -1) return nullptr;

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;
        if (pSession->GetObjectId() == objectId) return pSession;
    }
    return nullptr;
}

bool GameRoom::CanBeStaticObject(UnityGameObject* pGameObject) const {
    if (dynamic_cast<CombatObject*>(pGameObject) == nullptr) return true;

    std::cerr << "[SpawnStaticObject] CombatObject 는 정적 오브젝트가 될 수 없다 (roomId=" << _roomId
              << ", objectId=" << pGameObject->objectId << ")" << std::endl;
    return false;
}

void GameRoom::DestroyDeadObject(uint32_t objectId) {
    auto it = _dynamicObjects.find(objectId);
    if (it == _dynamicObjects.end()) {
        std::cerr << "[DestroyDeadObject] 동적 오브젝트가 아니다 (roomId=" << _roomId
                  << ", objectId=" << objectId << ")" << std::endl;
        return;
    }

    // 훅이 _dynamicObjects 에 흔적을 넣을 수 있어 반복자를 넘겨서는 안 된다
    UnityGameObject* pObject = it->second;
    _dynamicObjects.erase(it);

    if (CombatObject* pCombat = dynamic_cast<CombatObject*>(pObject))
        pCombat->OnDeathResolved(*this);

    delete pObject;

    External_Game_Protocol::D2CNotifyDespawnObject despawnPkt;
    despawnPkt.set_object_id(objectId);

    Broadcast(despawnPkt, ClientPacketHandler::MakeD2CNotifyDespawnObjectReliable);
}

void GameRoom::RemovePlayerObject(uint32_t objectId) {
    auto it = _playerObjects.find(objectId);
    if (it == _playerObjects.end()) return;

    delete it->second;
    _playerObjects.erase(it);
}

namespace {

uint32_t ElapsedMs(PlayerSession::TimePoint from, PlayerSession::TimePoint to) {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count());
}

External_Game_Protocol::DespawnReason ToDespawnReason(PlayerSession::LeaveReason reason) {
    switch (reason) {
    case PlayerSession::LeaveReason::RECALLED:     return External_Game_Protocol::DESPAWN_RECALLED;
    case PlayerSession::LeaveReason::DEAD:         return External_Game_Protocol::DESPAWN_DEAD;
    case PlayerSession::LeaveReason::DISCONNECTED: return External_Game_Protocol::DESPAWN_DISCONNECTED;
    default:                                       return External_Game_Protocol::DESPAWN_UNKNOWN;
    }
}

bool IsDetachReady(PlayerSession* pSession, PlayerSession::TimePoint now) {
    if (pSession->GetLeaveReason() != PlayerSession::LeaveReason::DISCONNECTED)
        return true;

    if (pSession->HasRecvSince(pSession->GetLeaveMarkedAt())) {
        pSession->CancelLeaving();
        std::cout << "[ProcessLeaves] 연결 끊김 예약 취소 - 수신 재개 (sessionId="
                  << pSession->GetSessionId() << ")" << std::endl;
        return false;
    }

    return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::LEAVE_GRACE_MS_DISCONNECTED;
}

bool IsFinalizeReady(PlayerSession* pSession, PlayerSession::TimePoint now) {
    const uint32_t notifySeq = pSession->GetLeaveNotifyRSeq();

    if (notifySeq == 0) return true;
    if (!pSession->IsReliablePending(notifySeq)) return true;

    return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::LEAVE_FINALIZE_TIMEOUT_MS;
}

}  // namespace

void GameRoom::ProcessLeaves() {
    const PlayerSession::TimePoint now = std::chrono::steady_clock::now();

    std::vector<uint32_t> deadObjectIds;
    for (const auto& [objectId, pObject] : _playerObjects) {
        if (pObject != nullptr && pObject->IsDeathPending())
            deadObjectIds.push_back(objectId);
    }

    for (uint32_t objectId : deadObjectIds) {
        PlayerSession* pSession = FindSessionByObjectId(static_cast<int32_t>(objectId));
        if (pSession == nullptr) continue;

        pSession->MarkLeaving(PlayerSession::LeaveReason::DEAD);
    }

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::PENDING) {
            if (!IsDetachReady(pSession, now)) continue;
            DetachPlayer(pSession);
        }

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::DETACHED &&
            IsFinalizeReady(pSession, now)) {
            pSession->FinalizeLeave();
        }
    }

    CheckAllLeft();
}

void GameRoom::CheckAllLeft() {
    if (_allLeftReported) return;
    if (_playerSessions.empty()) return;

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;
        if (pSession->GetLeaveState() != PlayerSession::LeaveState::FINALIZED) return;
    }

    _allLeftReported = true;

    std::cout << "[CheckAllLeft] 룸 전원 이탈 (roomId=" << _roomId << ", mapId=" << _mapId
              << ", 인원=" << _playerSessions.size() << ")" << std::endl;

    pDediServer->ReserveRoomDestroy(_roomId);

    // OPTION : 입장 타임아웃 — 한 번도 접속하지 않은 세션(INIT)은 이탈 확정에 도달하지 못해
    //          전원 이탈 조건이 성립하지 않는다. 일정 시간 후 DISCONNECTED 로 이탈시키면 해소된다
}

void GameRoom::NotifySpawnObject(UnityGameObject* pGameObject) {
    External_Game_Protocol::D2CNotifySpawnObject pkt;
    pGameObject->Serialize(pkt.mutable_game_object());

    Broadcast(pkt, ClientPacketHandler::MakeD2CNotifySpawnObjectReliable);
}

void GameRoom::SpawnCorpseContainer(PlayerObject* pPlayerObject, const PlayerInventory& inventory) {
    if (pPlayerObject == nullptr) return;

    SpawnDynamicObject(new CorpseContainer(GetNewObjectId(), pPlayerObject->position,
                                           pPlayerObject->yawAngle, inventory));
}

void GameRoom::NotifySpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) {
    External_Game_Protocol::D2CSpawnPlayerObject pkt;
    pkt.set_character_type(pGameObject->GetCharacterType());
    pkt.set_weapon_id(pGameObject->GetCurrentWeaponId());
    pGameObject->Serialize(pkt.mutable_game_object());

    BroadcastExcept(pkt, ClientPacketHandler::MakeD2CSpawnPlayerObjectReliable, ownerSessionId);
}

void GameRoom::DetachPlayer(PlayerSession* pSession) {
    const int32_t objectId = pSession->GetObjectId();
    const PlayerSession::LeaveReason reason = pSession->GetLeaveReason();

    pSession->EndRecall();
    pSession->SetSessionState(PlayerSession::SessionState::LEFT);
    pSession->SetInteractingContainerId(-1);

    PlayerObject* pPlayerObj = (objectId != -1)
        ? FindPlayerObject(static_cast<uint32_t>(objectId))
        : nullptr;

    if (pPlayerObj != nullptr) {
        if (reason == PlayerSession::LeaveReason::DEAD ||
            reason == PlayerSession::LeaveReason::DISCONNECTED) {
            if (reason == PlayerSession::LeaveReason::DEAD)
                SpawnCorpseContainer(pPlayerObj, pSession->GetInventoryMutable());

            pSession->GetInventoryMutable().Clear();
        }

        External_Game_Protocol::D2CDespawnPlayerObject despawnPkt;
        despawnPkt.set_object_id(static_cast<uint32_t>(objectId));
        despawnPkt.set_reason(ToDespawnReason(reason));

        RemovePlayerObject(static_cast<uint32_t>(objectId));

        BroadcastExcept(despawnPkt,
                        ClientPacketHandler::MakeD2CDespawnPlayerObjectReliable,
                        pSession->GetSessionId());
    }

    pSession->SetObjectId(-1);

    pSession->ClearPendingReliableExcept(pSession->GetLeaveNotifyRSeq());
    pSession->SetLeaveState(PlayerSession::LeaveState::DETACHED);

    std::cout << "[DetachPlayer] 룸에서 분리 (sessionId=" << pSession->GetSessionId()
              << ", objectId=" << objectId
              << ", reason=" << static_cast<int32_t>(reason) << ")" << std::endl;
}

void TestGameRoom::InitTestGameRoom() {
    uint32_t oid = GetNewObjectId();
    TestItemBox* pBox = new TestItemBox(oid, 0, 0, 0);
    SpawnStaticObject(pBox);
}

void TestGameRoom::SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) {
    assert(pPkt != nullptr && "SetSpawnSpot - pPkt is null!");
    assert(!_spawnSpots.empty() && "SetSpawnSpot - spawnSpots is empty!");

    _spawnSpots[_spawnSpotIndex].Serialize(pPkt->mutable_spawn_point());
    _spawnSpotIndex = (_spawnSpotIndex + 1) % _spawnSpots.size();
}

void TestGameRoom::ReleaseThis() {
    ObjectPool<TestGameRoom>::Release(this);
}

void TestGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!CanBeStaticObject(pGameObject)) return;
    if (!_staticObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void TestGameRoom::SpawnDynamicObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!_dynamicObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void TestGameRoom::SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) {
    if (pGameObject == nullptr) return;
    if (!_playerObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnPlayerObject(pGameObject, ownerSessionId);
}

void WinchesterGameRoom::SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) {
    assert(pPkt != nullptr && "SetSpawnSpot - pPkt is null!");
    assert(!_spawnSpots.empty() && "SetSpawnSpot - spawnSpots is empty!");

    _spawnSpots[_spawnSpotIndex].Serialize(pPkt->mutable_spawn_point());
    _spawnSpotIndex = (_spawnSpotIndex + 1) % _spawnSpots.size();
}

void WinchesterGameRoom::ReleaseThis() {
    ObjectPool<WinchesterGameRoom>::Release(this);
}

void WinchesterGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!CanBeStaticObject(pGameObject)) return;
    if (!_staticObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void WinchesterGameRoom::SpawnDynamicObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!_dynamicObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void WinchesterGameRoom::SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) {
    if (pGameObject == nullptr) return;
    if (!_playerObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnPlayerObject(pGameObject, ownerSessionId);
}