#include "GameRoom.h"

#include "../ObjectPool.h"
#include "PlayerSession.h"
#include "DedicateGlobalVariable.h"
#include "UnityGameObjects/TestGameObjects.h"
#include "ClientPacketHandler.h"

// MapDataManager 의 MapId 는 GameRoom::MapType 의 사본이므로 값이 어긋나면 즉시 잡는다.
static_assert(static_cast<int32_t>(GameRoom::MAP_TUTORIAL)   == static_cast<int32_t>(MapDataManager::MAP_ID_TUTORIAL),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_TUTORIAL");
static_assert(static_cast<int32_t>(GameRoom::MAP_WINCHESTER) == static_cast<int32_t>(MapDataManager::MAP_ID_WINCHESTER),
              "MapType 과 MapDataManager::MapId 불일치 - MAP_WINCHESTER");

// 귀환 영역은 맵당 불변 상수이므로 파생 룸마다 채우지 않고 여기서 한 번만 연결한다.
// (파생 클래스가 늘어나도 초기화를 빠뜨릴 수 없다)
GameRoom::GameRoom(int32_t mapId) : _mapId(mapId) {
    _pRecallZones = MapDataManager::GetRecallZones(mapId, _recallZoneCount);
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
        // 전부 nullptr인 경우 — is_last=true 빈 패킷 보장
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

void GameRoom::RemovePlayerObject(uint32_t objectId) {
    auto it = _playerObjects.find(objectId);
    if (it == _playerObjects.end()) return;

    delete it->second;   // Handle_C2D_RequestSpawnMe() 에서 new 로 만든다 (풀 미사용)
    _playerObjects.erase(it);
}

// ── 이탈(INPLAY 해제) 처리 ──────────────────────────────────────────────────
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

// ② 분리 가능 여부. 연결 끊김만 유예를 거치며, 유예 중 수신이 재개되면 예약을 취소한다.
bool IsDetachReady(PlayerSession* pSession, PlayerSession::TimePoint now) {
    if (pSession->GetLeaveReason() != PlayerSession::LeaveReason::DISCONNECTED)
        return true;

    // 유예 중 패킷이 다시 들어왔다 → 끊김 판정이 오탐이었다
    if (pSession->HasRecvSince(pSession->GetLeaveMarkedAt())) {
        pSession->CancelLeaving();
        std::cout << "[ProcessLeaves] 연결 끊김 예약 취소 - 수신 재개 (sessionId="
                  << pSession->GetSessionId() << ")" << std::endl;
        return false;
    }

    return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::LEAVE_GRACE_MS_DISCONNECTED;
}

// ③ 확정 가능 여부. 이탈 통보(reliable)가 ACK 되기를 기다리되 상한을 둔다.
bool IsFinalizeReady(PlayerSession* pSession, PlayerSession::TimePoint now) {
    const uint32_t notifySeq = pSession->GetLeaveNotifyRSeq();

    if (notifySeq == 0) return true;                        // 통보할 것이 없다
    if (!pSession->IsReliablePending(notifySeq)) return true;  // ACK 받음

    return ElapsedMs(pSession->GetLeaveMarkedAt(), now) >= PlayerSession::LEAVE_FINALIZE_TIMEOUT_MS;
}

}  // namespace

void GameRoom::ProcessLeaves() {
    const PlayerSession::TimePoint now = std::chrono::steady_clock::now();

    // ── 사망 감지 ──
    // _playerObjects 는 아래에서 변형되므로 순회 중에는 objectId 만 모은다.
    std::vector<uint32_t> deadObjectIds;
    for (const auto& [objectId, pObject] : _playerObjects) {
        if (pObject != nullptr && pObject->IsDeathPending())
            deadObjectIds.push_back(objectId);
    }

    for (uint32_t objectId : deadObjectIds) {
        PlayerSession* pSession = FindSessionByObjectId(static_cast<int32_t>(objectId));
        // TODO : 세션 없는 전투 오브젝트(AI 등)의 사망 처리는 별도 작업
        if (pSession == nullptr) continue;

        pSession->MarkLeaving(PlayerSession::LeaveReason::DEAD);
    }

    // ── 예약된 이탈 진행 ──
    // _playerSessions 는 여기서 변형하지 않는다 (세션 해제는 룸 소멸까지 미룬다).
    // 그래야 DetachPlayer() 안의 BroadcastExcept() 가 같은 맵을 중첩 순회해도 안전하다.
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

    std::cout << "[CheckAllLeft] 룸 전원 이탈 (mapId=" << _mapId
              << ", 인원=" << _playerSessions.size() << ")" << std::endl;

    // TODO : 룸 정리를 여기에 붙인다. 진입점을 이 한 곳으로 유지할 것.
    //        ① _playerSessions 의 PlayerSession 해제 — 세션 해제를 룸 소멸까지 미루기로 한
    //           결정의 종착점이다. DediServerService::_players 슬롯과 _freePlayerIds 반납도 함께
    //        ② _staticObjects / _dynamicObjects / _playerObjects 해제 후 ReleaseThis()
    //           (ReleaseThis() 는 아직 호출부가 없어 룸 소멸 경로 자체가 없다)
    //        ③ 남은 룸이 없으면 프로세스 정리까지 — Main 의 DediManager 와 함께 결정 필요

    // OPTION : 입장 타임아웃 — 한 번도 접속하지 않은 세션(INIT)은 이탈 확정에 도달하지 못하므로
    //          위 전원 이탈 조건이 영원히 성립하지 않는다. 룸 생성 후 일정 시간이 지나도
    //          INIT 인 세션을 DISCONNECTED 로 이탈시키면 해소된다.
    //          매칭까지 마친 유저가 끝내 접속하지 않는 경우는 드물어 우선순위는 낮다.
}

void GameRoom::NotifySpawnObject(UnityGameObject* pGameObject) {
    External_Game_Protocol::D2CNotifySpawnObject pkt;
    pGameObject->Serialize(pkt.mutable_game_object());

    Broadcast(pkt, ClientPacketHandler::MakeD2CNotifySpawnObjectReliable);
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

    // 진행 중이던 귀환을 무효화한다 — 세대가 올라가므로 타이머 큐에 남은
    // RecallTick() 콜백은 실행되더라도 스스로 포기한다.
    pSession->EndRecall();
    pSession->SetSessionState(PlayerSession::SessionState::LEFT);
    pSession->SetInteractingContainerId(-1);

    PlayerObject* pPlayerObj = (objectId != -1)
        ? FindPlayerObject(static_cast<uint32_t>(objectId))
        : nullptr;

    if (pPlayerObj != nullptr) {
        // 귀환만 반출이 성립한다. 사망과 연결 끊김은 둘 다 빈손이다
        // (끊김을 사망과 동일하게 처리하기로 결정 — 회선을 끊어도 얻는 것이 없다).
        if (reason == PlayerSession::LeaveReason::DEAD ||
            reason == PlayerSession::LeaveReason::DISCONNECTED) {
            // TODO : 사망(DEAD)에 한해 pPlayerObj->position 에 시신 컨테이너(Container 파생)를
            //        스폰하고, Clear() 대신 인벤토리를 그쪽으로 '이동' 시킨다.
            //        - 반드시 이 자리(오브젝트 제거 전)여야 한다. 제거 후에는 시신 위치를 잃는다
            //        - Container::PlaceItem() 이 protected 이므로, PlayerInventory 를 통째로 받아
            //          채우는 전용 파생 클래스를 두는 편이 낫다
            //        - SpawnDynamicObject() 가 생성 통보까지 보내므로 그것만 호출하면 된다
            //        연결 끊김은 시신을 남기지 않기로 결정했다 — 오탐일 때 정직한 플레이어의
            //        장비를 남에게 넘기지 않는 쪽이 낫다는 판단이다.
            pSession->GetInventoryMutable().Clear();
        }

        // 퇴장 패킷은 오브젝트를 지우기 전에 만들어 둔다
        External_Game_Protocol::D2CDespawnPlayerObject despawnPkt;
        despawnPkt.set_object_id(static_cast<uint32_t>(objectId));
        despawnPkt.set_reason(ToDespawnReason(reason));

        RemovePlayerObject(static_cast<uint32_t>(objectId));

        BroadcastExcept(despawnPkt,
                        ClientPacketHandler::MakeD2CDespawnPlayerObjectReliable,
                        pSession->GetSessionId());
    }

    pSession->SetObjectId(-1);

    // 이탈 통보 한 장만 남기고 재전송 큐를 비운다 (남은 한 장은 FinalizeLeave 에서 폐기).
    // 이 호출이 CheckRetransmits() 의 순회 밖이어야 하는 이유는 GameRoom.h 의 ProcessLeaves 주석 참조.
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

void TestGameRoom::Update() {
    if (_playerObjects.empty()) return;

    External_Game_Protocol::D2CUpdatePlayerStates pkt;
    FillPlayerStates(pkt);

    Broadcast(pkt, ClientPacketHandler::MakeD2CUpdatePlayerStatesUnreliable);
}

void TestGameRoom::ReleaseThis() {
    ObjectPool<TestGameRoom>::Release(this);
}

// InitTestGameRoom() 이 생성자 본문에서 이걸 호출한다 — 그 시점엔 세션이 없어 통보가 무동작이다.
void TestGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
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

void WinchesterGameRoom::Update() {
}

void WinchesterGameRoom::ReleaseThis() {
    ObjectPool<WinchesterGameRoom>::Release(this);
}

void WinchesterGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    if (pGameObject == nullptr) return;
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