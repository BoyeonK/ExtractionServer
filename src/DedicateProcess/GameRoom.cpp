#include "GameRoom.h"

#include "../ObjectPool.h"
#include "PlayerSession.h"
#include "DedicateGlobalVariable.h"
#include "DediServerService.h"
#include "UnityGameObjects/TestGameObjects.h"
#include "UnityGameObjects/PlayerLootContainer.h"
#include "ClientPacketHandler.h"

static_assert(static_cast<int32_t>(GameRoom::MAP_TUTORIAL) == static_cast<int32_t>(MapDataManager::MAP_ID_TUTORIAL),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_TUTORIAL");
static_assert(static_cast<int32_t>(GameRoom::MAP_TENERIFE) == static_cast<int32_t>(MapDataManager::MAP_ID_TENERIFE),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_TENERIFE");

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

void GameRoom::ReleaseInteractingContainer(PlayerSession* pSession) {
    if (pSession == nullptr) return;

    const int32_t containerId = pSession->GetInteractingContainerId();
    pSession->SetInteractingContainerId(-1);
    if (containerId == -1) return;

    Container* pContainer = dynamic_cast<Container*>(FindNonplayerObject(static_cast<uint32_t>(containerId)));
    if (pContainer == nullptr) return;

    if (pContainer->GetInteractingPlayerId() != static_cast<uint32_t>(pSession->GetObjectId())) return;

    pContainer->SetInteractingPlayerId(Container::NO_INTERACTING_PLAYER);
}

bool GameRoom::IsPlayerNearContainer(uint32_t playerObjectId, uint32_t containerObjectId) const {
    const PlayerObject* pPlayerObj = FindPlayerObject(playerObjectId);
    if (pPlayerObj == nullptr) return false;

    const UnityGameObject* pTarget = FindNonplayerObject(containerObjectId);
    if (pTarget == nullptr) return false;

    const float dx = pPlayerObj->position.x - pTarget->position.x;
    const float dy = pPlayerObj->position.y - pTarget->position.y;
    const float dz = pPlayerObj->position.z - pTarget->position.z;

    return (dx * dx + dy * dy + dz * dz) <= CONTAINER_INTERACT_RANGE_SQ;
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

void GameRoom::RegenPlayerShields() {
    const uint32_t nowMs = ClientPacketHandler::NowMs();

    if (_lastRegenMs == 0) {
        _lastRegenMs = nowMs;
        return;
    }

    uint32_t elapsed = nowMs - _lastRegenMs;
    _lastRegenMs = nowMs;

    if (elapsed > MAX_REGEN_STEP_MS)
        elapsed = MAX_REGEN_STEP_MS;

    for (const auto& [objectId, pObject] : _playerObjects) {
        if (pObject == nullptr) continue;
        pObject->RegenShield(elapsed);
    }
}

void GameRoom::Update() {
    RegenPlayerShields();
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

const std::string& GameRoom::FindObjectName(uint32_t objectId) const {
    if (const PlayerObject* pPlayerObj = FindPlayerObject(objectId))
        return pPlayerObj->GetObjectName();

    if (const UnityGameObject* pObject = FindNonplayerObject(objectId))
        return pObject->GetObjectName();

    return OBJECT_NAME_UNRESOLVED;
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

    uint32_t killerObjectId = CombatObject::NO_ATTACKER;
    if (CombatObject* pCombat = dynamic_cast<CombatObject*>(pObject)) {
        killerObjectId = pCombat->GetLastAttackerId();
        pCombat->OnDeathResolved(*this);
    }

    // 이름 조회를 delete 위에 둬서 오브젝트 수명에 기대지 않게 한다
    External_Game_Protocol::D2CNotifyObjectKilled killedPkt;
    killedPkt.set_victim_object_id(objectId);
    killedPkt.set_killer_object_id(killerObjectId);
    killedPkt.set_killer_object_name(FindObjectName(killerObjectId));

    delete pObject;

    Broadcast(killedPkt, ClientPacketHandler::MakeD2CNotifyObjectKilledReliable);
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

bool IsFinalizeReady(PlayerSession* pSession, PlayerSession::TimePoint now) {
    // 사망은 ack 여부와 무관하게 유예를 끝까지 채운다. 클라이언트가 통보를 일찍 받았거나
    // 연결을 끊었다고 해서 유예를 줄일 수 있으면 안 된다
    if (pSession->GetLeaveReason() == PlayerSession::LeaveReason::DEAD)
        return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::DEATH_GRACE_MS;

    const uint32_t notifySeq = pSession->GetLeaveNotifyRSeq();

    if (notifySeq == 0) return true;
    if (!pSession->IsReliablePending(notifySeq)) return true;

    return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::LEAVE_FINALIZE_TIMEOUT_MS;
}

}  // namespace

void GameRoom::DetectDisconnectedSessions() {
    const uint32_t nowMs = ClientPacketHandler::NowMs();

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;
        if (!pSession->IsInplay() || pSession->IsLeaving()) continue;

        // OPTION: echo 가 0이면 판정을 건너뛴다. 클라이언트가 timestampEcho 를 아예 채우지 않으면
        //         전 세션이 영구히 면제되어 감지가 조용히 무력화되므로 흔적을 남길 것
        const uint32_t echoTs = pSession->GetLastEchoTs();
        if (echoTs == 0) continue;

        const uint32_t elapsed = nowMs - echoTs;
        if (elapsed < PlayerSession::DISCONNECT_TIMEOUT_MS) continue;

        std::cout << "[DetectDisconnected] 하향 경로 무응답 (roomId=" << _roomId
                  << ", sessionId=" << sessionId << ", 경과=" << elapsed << "ms)" << std::endl;

        pSession->MarkLeaving(PlayerSession::LeaveReason::DISCONNECTED);
    }
}

uint32_t GameRoom::GetRemainingLifetimeMs() const {
    const uint32_t elapsed = ElapsedMs(_createdAt, std::chrono::steady_clock::now());
    if (elapsed >= ROOM_LIFETIME_MS) return 0;
    return ROOM_LIFETIME_MS - elapsed;
}

void GameRoom::AllKill() {
    std::cout << "[AllKill] 룸 수명 만료 (roomId=" << _roomId << ", mapId=" << _mapId
              << ", 인원=" << _playerSessions.size() << ")" << std::endl;

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;

        pSession->MarkLeaving(PlayerSession::LeaveReason::DEAD);
    }
}

void GameRoom::ProcessLeaves() {
    const PlayerSession::TimePoint now = std::chrono::steady_clock::now();

    if (!_lifetimeExpired && ElapsedMs(_createdAt, now) >= ROOM_LIFETIME_MS) {
        _lifetimeExpired = true;
        AllKill();
    }

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

    DetectDisconnectedSessions();

    // 분리 루프보다 앞이어야 한다. 루프 안에서 지우면 먼저 분리된 세션이 방송한
    // 사망·디스폰 통보가 뒤에 분리되는 세션의 큐에서 함께 사라진다
    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::PENDING)
            pSession->ClearPendingReliableExcept(pSession->GetLeaveNotifyRSeq());
    }

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::PENDING)
            DetachPlayer(pSession);

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::DETACHED &&
            IsFinalizeReady(pSession, now)) {
            pSession->FinalizeLeave();
        }
    }

    CheckAllLeft();
}

void GameRoom::CheckAllLeft() {
    if (_allLeftReported) return;

    for (const auto& [sessionId, pSession] : _playerSessions) {
        if (pSession == nullptr) continue;
        if (pSession->GetLeaveState() != PlayerSession::LeaveState::FINALIZED) return;
    }

    _allLeftReported = true;

    std::cout << "[CheckAllLeft] 룸 전원 이탈 (roomId=" << _roomId << ", mapId=" << _mapId
              << ", 인원=" << _playerSessions.size() << ")" << std::endl;

    pDediServer->ReserveRoomDestroy(_roomId);

    // OPTION : 한 번도 접속하지 않은 세션(INIT)은 이탈 확정에 도달하지 못해 룸 수명 만료
    //          까지 회수가 밀린다. 짧은 입장 타임아웃을 두면 앞당길 수 있다
}

void GameRoom::NotifySpawnObject(UnityGameObject* pGameObject) {
    External_Game_Protocol::D2CNotifySpawnObject pkt;
    pGameObject->Serialize(pkt.mutable_game_object());

    Broadcast(pkt, ClientPacketHandler::MakeD2CNotifySpawnObjectReliable);
}

void GameRoom::SpawnPlayerLootContainer(PlayerObject* pPlayerObject, const PlayerInventory& inventory) {
    if (pPlayerObject == nullptr) return;

    SpawnDynamicObject(new PlayerLootContainer(GetNewObjectId(), pPlayerObject->position,
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

    // 사망자는 유예가 끝날 때까지 SPECTATING 으로 남아 브로드캐스트만 계속 받는다.
    // 두 상태 모두 IsActiveState 가 아니므로 상향 요청은 이 시점부터 전부 거부된다
    pSession->SetSessionState(reason == PlayerSession::LeaveReason::DEAD
                                  ? PlayerSession::SessionState::SPECTATING
                                  : PlayerSession::SessionState::LEFT);
    ReleaseInteractingContainer(pSession);

    PlayerObject* pPlayerObj = (objectId != -1)
        ? FindPlayerObject(static_cast<uint32_t>(objectId))
        : nullptr;

    if (pPlayerObj != nullptr) {
        if (reason == PlayerSession::LeaveReason::DEAD ||
            reason == PlayerSession::LeaveReason::DISCONNECTED) {
            if (reason == PlayerSession::LeaveReason::DEAD) {
                SpawnPlayerLootContainer(pPlayerObj, pSession->GetInventoryMutable());

                const uint32_t killerObjectId = pPlayerObj->GetLastAttackerId();

                External_Game_Protocol::D2CNotifyPlayerKilled killedPkt;
                killedPkt.set_victim_object_id(static_cast<uint32_t>(objectId));
                killedPkt.set_killer_object_id(killerObjectId);
                killedPkt.set_victim_object_name(pPlayerObj->GetObjectName());
                // 사망 유예 동안 가해자가 방을 떠났으면 빈 문자열이 된다
                killedPkt.set_killer_object_name(FindObjectName(killerObjectId));

                // 피해자도 SPECTATING 이라 이 통보를 받는다. 사망 화면의 킬러 표시 근거
                Broadcast(killedPkt, ClientPacketHandler::MakeD2CNotifyPlayerKilledReliable);
            }

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

    pSession->SetLeaveState(PlayerSession::LeaveState::DETACHED);

    // 결과가 확정된 지금 통보한다. 사망 유예가 끝나기를 기다리면 그동안 DB 반영과
    // active_match 락 해제가 밀려, 로비로 먼저 돌아간 클라이언트가 재매칭을 거부당한다
    pSession->NotifyLeftOnce();

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

void TenerifeGameRoom::SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) {
    assert(pPkt != nullptr && "SetSpawnSpot - pPkt is null!");
    assert(!_spawnSpots.empty() && "SetSpawnSpot - spawnSpots is empty!");

    _spawnSpots[_spawnSpotIndex].Serialize(pPkt->mutable_spawn_point());
    _spawnSpotIndex = (_spawnSpotIndex + 1) % _spawnSpots.size();
}

void TenerifeGameRoom::ReleaseThis() {
    ObjectPool<TenerifeGameRoom>::Release(this);
}

void TenerifeGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!CanBeStaticObject(pGameObject)) return;
    if (!_staticObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void TenerifeGameRoom::SpawnDynamicObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
    if (!_dynamicObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnObject(pGameObject);
}

void TenerifeGameRoom::SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) {
    if (pGameObject == nullptr) return;
    if (!_playerObjects.try_emplace(pGameObject->objectId, pGameObject).second) return;

    NotifySpawnPlayerObject(pGameObject, ownerSessionId);
}